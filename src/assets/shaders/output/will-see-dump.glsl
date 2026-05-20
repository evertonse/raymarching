
#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform sampler2D scene_hdr;

// Why the final frabebuffer doesnt work if not rgba instead of rgb?
// #define IMAGE_FORMAT rgba8
// #define IMAGE_FORMAT rgba32f
#define IMAGE_FORMAT r11f_g11f_b10f

layout(IMAGE_FORMAT, binding = 1) uniform writeonly image2D out_image;

#define lerp mix


uniform float exposure;


// From: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
//       Article's Author recomends this for better curve fit https://github.com/TheRealMJP/BakingLab
//       We've ported under the name 'tonemap_aces2'
// Krzysztof Narkowicz says:
// February 26, 2016 at 20:17
// Yes, you need to multiply by exposure before the tone mapping and do the gamma correction after.
//
vec3 tonemap_aces(const vec3 x) { // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;
   return (x * (a * x + b)) / (x * (c * x + d) + e);
}

// ACES fitted tonemap (from Stephen Hill / MJP)
const mat3 ACESInputMat = mat3(
   0.59719, 0.07600, 0.02840,
   0.35458, 0.90834, 0.13383,
   0.04823, 0.01566, 0.83777
);

const mat3 ACESOutputMat = mat3(
   1.60475, -0.10208, -0.00327,
   -0.53108,  1.10813, -0.07276,
   -0.07367, -0.00605,  1.07602
);

vec3 RRTAndODTFit(vec3 v) {
   vec3 a = v * (v + 0.0245786) - 0.000090537;
   vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
   return a / b;
}

vec3 ACESFitted(vec3 color) {
   color = ACESInputMat * color;  // ACEScg conversion
   color = RRTAndODTFit(color);   // RRT + ODT fit
   color = ACESOutputMat * color;
   return clamp(color, 0.0, 1.0);
}

vec3 tonemap_aces2(const vec3 x) {
   return ACESFitted(x);
}

//
// File derivation and implemenation from https://github.com/bWFuanVzYWth/AgX/blob/main/agx.glsl
// Blender article about agx: https://developer.blender.org/docs/release_notes/4.0/color_management/
//
// Resources: https://github.com/EaryChow/AgX
//

// In practice, there is still debate and confusion around whether sRGB data
// should be displayed with pure 2.2 gamma as defined in the standard,
// or with the inverse of the OETF.
// https://en.wikipedia.org/wiki/SRGB
//
#define BT709_OETF

#if defined(PURE_GAMMA)
vec3 to_linear(vec3 sRGB) { return pow(sRGB, vec3(2.2)); }

vec3 from_linear(vec3 linearRGB) { return pow(linearRGB, vec3(1.0 / 2.2)); }

#elif defined(BT709_OETF)
vec3 to_linear(vec3 sRGB) {
   bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
   vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
   vec3 lower = sRGB / vec3(12.92);

   return mix(higher, lower, cutoff);
}

vec3 from_linear(vec3 linearRGB) {
   bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
   vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
   vec3 lower = linearRGB * vec3(12.92);

   return mix(higher, lower, cutoff);
}

#endif

vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

vec3 agx_curve3(vec3 v) {
   const float threshold = 0.6060606060606061;
   const float a_up = 69.86278913545539;
   const float a_down = 59.507875;
   const float b_up = 13.0 / 4.0;
   const float b_down = 3.0 / 1.0;
   const float c_up = -4.0 / 13.0;
   const float c_down = -1.0 / 3.0;

   vec3 mask = step(v, vec3(threshold));
   vec3 a = a_up + (a_down - a_up) * mask;
   vec3 b = b_up + (b_down - b_up) * mask;
   vec3 c = c_up + (c_down - c_up) * mask;
   return 0.5 + (((-2.0 * threshold)) + 2.0 * v) * pow(1.0 + a * pow(abs(v - threshold), b), c);
}

vec3 agx_tonemapping(vec3 /*Linear BT.709*/ ci) {
   const float min_ev = -12.473931188332413;
   const float max_ev = 4.026068811667588;
   const float dynamic_range = max_ev - min_ev;

   const mat3 agx_mat = mat3(0.8424010709504686, 0.04240107095046854, 0.04240107095046854, 0.07843650156180276, 0.8784365015618028, 0.07843650156180276, 0.0791624274877287, 0.0791624274877287, 0.8791624274877287);
   const mat3 agx_mat_inv = mat3(1.1969986613119143, -0.053001338688085674, -0.053001338688085674, -0.09804562695225345, 1.1519543730477466, -0.09804562695225345, -0.09895303435966087, -0.09895303435966087, 1.151046965640339);

   // Input transform (inset)
   ci = agx_mat * ci;

   // Apply sigmoid function
   vec3 ct = saturate(log2(ci) * (1.0 / dynamic_range) - (min_ev / dynamic_range));
   vec3 co = agx_curve3(ct);

   // Inverse input transform (outset)
   co = agx_mat_inv * co;

   return /*BT.709 (NOT linear)*/ co;
}

vec3 tonemap_agx(vec3 x) {
   return to_linear(agx_tonemapping(x));
}

//
// File derivation and implemenation from https://iolite-engine.com/blog_posts/minimal_agx_implementation
// Shader Toy https://www.shadertoy.com/view/cd3XWr
//

// MIT License
//
// Copyright (c) 2024 Missing Deadlines (Benjamin Wrensch)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// All values used to derive this implementation are sourced from Troy’s initial AgX implementation/OCIO config file available here:
//   https://github.com/sobotka/AgX

// 0: Default, 1: Golden, 2: Punchy
// #define AGX_LOOK 2
#define AGX_LOOK 2

#define BT709_OETF
// #defined PURE_GAMMA

#if defined(PURE_GAMMA)
vec3 internal_to_linear(vec3 sRGB) { return pow(sRGB, vec3(2.2)); }

vec3 internal_from_linear(vec3 linearRGB) { return pow(linearRGB, vec3(1.0 / 2.2)); }

#elif defined(BT709_OETF)
vec3 internal_to_linear(vec3 sRGB) {
   bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
   vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
   vec3 lower = sRGB / vec3(12.92);

   return mix(higher, lower, cutoff);
}

vec3 internal_from_linear(vec3 linearRGB) {
   bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
   vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
   vec3 lower = linearRGB * vec3(12.92);

   return mix(higher, lower, cutoff);
}

#endif

// Mean error^2: 1.85907662e-06
vec3 agxDefaultContrastApprox7thOrder(vec3 x) {
  vec3 x2 = x * x;
  vec3 x4 = x2 * x2;
  vec3 x6 = x4 * x2;

  return - 17.86     * x6 * x
         + 78.01     * x6
         - 126.7     * x4 * x
         + 92.06     * x4
         - 28.72     * x2 * x
         + 4.361     * x2
         - 0.1718    * x
         + 0.002857;
}

// Mean error^2: 3.6705141e-06
vec3 agxDefaultContrastApprox(vec3 x) {
  vec3 x2 = x * x;
  vec3 x4 = x2 * x2;

  return + 15.5     * x4 * x2
         - 40.14    * x4 * x
         + 31.96    * x4
         - 6.868    * x2 * x
         + 0.4298   * x2
         + 0.1191   * x
         - 0.00232;
}

vec3 agx(vec3 val) {
  // Ensure no negative values
  // val = max(float3(0.0), val);
  const mat3 agx_mat = mat3(
    0.842479062253094, 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992,  0.878468636469772,  0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104);

  const float min_ev = -12.47393f;
  const float max_ev = 4.026069f;

  // Input transform (inset)
  val = agx_mat * val;

  // Log2 space encoding
  val = clamp(log2(val), min_ev, max_ev);
  val = (val - min_ev) / (max_ev - min_ev);

  // Apply sigmoid function approximation
  val = agxDefaultContrastApprox7thOrder(val);

  return val;
}

vec3 agxEotf(vec3 val) {
  const mat3 agx_mat_inv = mat3(
    1.19687900512017, -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433, 1.15107367264116);

  // Inverse input transform (outset)
  val = agx_mat_inv * val;

  // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
  // NOTE: We're linearizing the output here. Comment/adjust when
  // *not* using a sRGB render target
  // NOTE deccan: commenting out this
  // val = pow(val, vec3(2.2));

  return val;
}

vec3 agxLook(vec3 val) {
  // Default
  vec3 offset = vec3(0.0);
  vec3 slope = vec3(1.0);
  vec3 power = vec3(1.0);
  float sat = 1.0;

#if AGX_LOOK == 1
  // Golden
  slope = vec3(1.0, 0.9, 0.5);
  power = vec3(0.8);
  sat = 0.8;
#elif AGX_LOOK == 2
  // Punchy
  slope = vec3(1.0);
  power = vec3(1.35, 1.35, 1.35);
  sat = 1.05; // sat = 1.4;
#endif

  // ASC CDL
  val = pow(val * slope + offset, power);

  const vec3 lw = vec3(0.2126, 0.7152, 0.0722);
  float luma = dot(val, lw);

  return luma + sat * (val - luma);
}


vec3 tonemap_agx_minimal(vec3 x) {
   vec3 value = x;
   value = agx(value);
   value = agxLook(value); // Optional
   value = agxEotf(value);
   value = internal_to_linear(value);
   return value;
}


vec3 tonemap_filmic_backend(vec3 x) {
   // Constants from Hable's "Filmic Tonemapping Operators" talk
   // https://www.slideshare.net/slideshow/hable-john-uncharted2-hdr-lighting/3602588
   // const float A = 0.15;
   // const float B = 0.50;
   // const float C = 0.10;
   // const float D = 0.20;
   // const float E = 0.02;
   // const float F = 0.30;

   const float A = 0.22; // Shoulder Strength
   const float B = 0.30; // Linear Strength
   const float C = 0.10; // Linear Angle
   const float D = 0.20; // Toe Strength
   const float E = 0.01; // Toe Numberator
   const float F = 0.30; // Toe Denominator
   return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}


vec3 tonemap_filmic(vec3 color, float exposure) {
   // Exposure bias tweak
   color = tonemap_filmic_backend(color * exposure);
   // white point (11.2 the default value)
   float white_scale = 1.0 / tonemap_filmic_backend(vec3(7.2)).r;
   return color * white_scale;
}


vec3 tonemap_reinhard(const vec3 x) {
   // reinhard tone mapping
   return x / (x + vec3(1.0));
}


// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
vec3 tonemap_lottes(vec3 x) {
  const vec3 a = vec3(1.6);
  const vec3 d = vec3(0.977);
  const vec3 hdrMax = vec3(8.0);
  const vec3 midIn = vec3(0.18);
  const vec3 midOut = vec3(0.267);

  const vec3 b =
      (-pow(midIn, a) + pow(hdrMax, a) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);
  const vec3 c =
      (pow(hdrMax, a * d) * pow(midIn, a) - pow(hdrMax, a) * pow(midIn, a * d) * midOut) /
      ((pow(hdrMax, a * d) - pow(midIn, a * d)) * midOut);

  return pow(x, a) / (pow(x, a * d) * b + c);
}


vec3 tonemap_reinhard(const vec3 hdr_color, float exposure) {
   vec3 mapped = vec3(1.0) - exp(-hdr_color * exposure);
   return mapped;
}

// Uncharted 2 Filmic Tonemap
vec3 tonemap_uncharted(vec3 x) {
   float A = 0.15;
   float B = 0.50;
   float C = 0.10;
   float D = 0.20;
   float E = 0.02;
   float F = 0.30;
   float W = 11.2; // white scale

   x = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
   float white_scale = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
   return x / white_scale;
}


// ACES Tonemap (Unity style)
vec3 tonemap_aces_unity(vec3 x) {
   const mat3 aces_input_matrix = mat3(
      0.59719, 0.35458, 0.04823,
      0.07600, 0.90834, 0.01566,
      0.02840, 0.13383, 0.83777
   );

   const mat3 aces_output_matrix = mat3(
      1.60475, -0.53108, -0.07367,
      -0.10208,  1.10813, -0.00605,
      -0.00327, -0.07276,  1.07602
   );

   x = aces_input_matrix * x;

   x = (x * (x + 0.0245786) - 0.000090537) /
       (x * (0.983729 * x + 0.4329510) + 0.238081);

   x = aces_output_matrix * x;
   return clamp(x, 0.0, 1.0);
}


// ACES Tonemap (Unreal Engine style)
vec3 tonemap_aces_unreal(vec3 x) {
   // Source: Unreal Engine 4 ACES implementation
   x *= 0.6; // exposure bias
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;

   return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


vec3 tonemap_gt7(vec3 x) {
    // Parameters tuned for Gran Turismo
    float P = 1.0;  // max display brightness
    float a = 1.0;  // contrast
    float m = 0.22; // linear section start
    float l = 0.4;  // linear section length
    float c = 1.33; // black tightness
    float b = 0.0;  // pedestal (black level)
    
    float l0 = ((P - m) * l) / a;
    float L0 = m - m / a;
    float L1 = m + (1.0 - m) / a;
    float S0 = m + l0;
    float S1 = m + a * l0;
    float C2 = (a * P) / (P - S1);
    float CP = -C2 / P;
    
    vec3 w0 = 1.0 - smoothstep(0.0, m, x);
    vec3 w2 = step(m + l0, x);
    vec3 w1 = 1.0 - w0 - w2;
    
    vec3 T = m * pow(x / m, vec3(c)) + b;
    vec3 S = P - (P - S1) * exp(CP * (x - S0));
    vec3 L = m + a * (x - m);
    
    return T * w0 + L * w1 + S * w2;
}

// Uchimura 2017, "HDR theory and practice"
// Math: https://www.desmos.com/calculator/gslcdxvipg
// Source: https://www.slideshare.net/nikuque/hdr-theory-and-practicce-jp
vec3 uchimura(vec3 x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  vec3 w0 = vec3(1.0 - smoothstep(0.0, m, x));
  vec3 w2 = vec3(step(m + l0, x));
  vec3 w1 = vec3(1.0 - w0 - w2);

  vec3 T = vec3(m * pow(x / m, vec3(c)) + b);
  vec3 S = vec3(P - (P - S1) * exp(CP * (x - S0)));
  vec3 L = vec3(m + a * (x - m));

  return T * w0 + L * w1 + S * w2;
}

vec3 tonemap_uchimura(vec3 x) {
  const float P = 1.0;  // max display brightness
  const float a = 1.0;  // contrast
  const float m = 0.22; // linear section start
  const float l = 0.4;  // linear section length
  const float c = 1.33; // black
  const float b = 0.0;  // pedestal

  return uchimura(x, P, a, m, l, c, b);
}

float uchimura(float x, float P, float a, float m, float l, float c, float b) {
  float l0 = ((P - m) * l) / a;
  float L0 = m - m / a;
  float L1 = m + (1.0 - m) / a;
  float S0 = m + l0;
  float S1 = m + a * l0;
  float C2 = (a * P) / (P - S1);
  float CP = -C2 / P;

  float w0 = 1.0 - smoothstep(0.0, m, x);
  float w2 = step(m + l0, x);
  float w1 = 1.0 - w0 - w2;

  float T = m * pow(x / m, c) + b;
  float S = P - (P - S1) * exp(CP * (x - S0));
  float L = m + a * (x - m);

  return T * w0 + L * w1 + S * w2;
}

float tonemap_uchimura(float x) {
  const float P = 1.0;  // max display brightness
  const float a = 1.0;  // contrast
  const float m = 0.22; // linear section start
  const float l = 0.4;  // linear section length
  const float c = 1.33; // black
  const float b = 0.0;  // pedestal

  return uchimura(x, P, a, m, l, c, b);
}



const float gamma_magic_number = 2.233333;

//
// Fast approximation
// From http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
//
vec3 linear_to_srgb(vec3 linear_rgb) {
   vec3 rgb = linear_rgb;
   vec3 s1 = sqrt(rgb);
   vec3 s2 = sqrt(s1);
   vec3 s3 = sqrt(s2);
   vec3 srgb = 0.662002687 * s1 + 0.684122060 * s2 - 0.323583601 * s3 - 0.0225411470 * rgb;
   return srgb;
}

vec3 srgb_to_linear(vec3 srgb) { return srgb * (srgb * (srgb * 0.305306011 + 0.682171111) + 0.012522878); }


//
// Gradient noise from Jorge Jimenez's presentation:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
// Example of dithering: https://www.shadertoy.com/view/MlV3R1
//
float noise(in vec2 uv) {
   float noise = fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
   if (false) {
      noise = fract(sin(dot(uv, vec2(12.9898,78.233))) * 43758.5453);
   }
   return noise;
}


void main() {

   ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
   ivec2 size = imageSize(out_image);

   if (pixel.x >= size.x || pixel.y >= size.y) {
      return;
   }

   vec2 uv = (vec2(pixel) + 0.5) / vec2(size);

   vec3 hdr = texture(scene_hdr, uv).rgb;

   const float exposure = 1.0;
   hdr *= exposure;

   // tonemap_aces(FragColor.xyz);
   // FragColor.xyz = tonemap_filmic(FragColor.xyz, 1.0);
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz);
   // const float exposure = 0.8;
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz, exposure);
   // vec3 mapped = tonemap_aces_unity(hdr);
   // vec3 mapped = tonemap_gt7(hdr);
   // vec3 mapped = tonemap_uchimura(hdr);
   // vec3 mapped = tonemap_aces(hdr);
   // vec3 mapped = tonemap_aces2(hdr);
   // vec3 mapped = tonemap_agx(hdr);
   vec3 mapped = tonemap_agx_minimal(hdr);

   mapped += (noise(pixel) - 0.5) / 255.0;

   mapped = linear_to_srgb(mapped);

   imageStore(out_image, pixel, vec4(mapped, 1.0));
   // imageStore(out_image, pixel, mapped);
}
 
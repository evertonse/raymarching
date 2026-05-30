#ifndef COMMON_HEADER
#define COMMON_HEADER

#include "src/renderer/shared/defines.glsl"

// I refuse to call this mix smh.
#ifndef lerp
#   define lerp mix
#endif

// Some helpful constants
#ifndef PI
#   define PI 3.14159265358979323846
#endif

//
// TODO: Add lerp smooth as seen in 49:00 of the Freya's video:
//       https://youtu.be/LSNQuFEDOyQ?si=seG75brEc83xATC-
//

// float packed contains high 16 bits (.x) low 16 bits (.y)
uvec2 unpack_u16_from_float(float packed_float) {
   uint bits = floatBitsToUint(packed_float);
   uint low  = bits & 0xFFFFu;
   uint high = (bits >> 16) & 0xFFFFu;
   return uvec2(high, low);
}

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

#define saturate(x) clamp(x, 0.0, 1.0)
// vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

// Taken from https://observablehq.com/@rreusser/bicubic-texture-interpolation-using-linear-filtering
// Also present in https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl#42179924

vec4 cubic(float v) {
   vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
   vec4 s = n * n * n;
   float x = s.x;
   float y = s.y - 4.0 * s.x;
   float z = s.z - 4.0 * s.y + 6.0 * s.x;
   float w = 6.0 - x - y - z;
   return vec4(x, y, z, w) * (1.0 / 6.0);
}

#ifndef sample_texture
#define sample_texture sample_texture_bicubic
#endif

vec4 sample_texture_bicubic(sampler2D sampler, vec2 uv) {
   vec2 texture_resolution = textureSize(sampler, 0);
   vec2 inverse_texture_resolution = 1.0 / texture_resolution;
   uv = uv * texture_resolution - 0.5;
   vec2 fxy = fract(uv);
   uv -= fxy;

   vec4 xcubic = cubic(fxy.x);
   vec4 ycubic = cubic(fxy.y);

   vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;

   vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
   vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

   offset *= inverse_texture_resolution.xxyy;

   vec4 sample0 = texture2D(sampler, offset.xz);
   vec4 sample1 = texture2D(sampler, offset.yz);
   vec4 sample2 = texture2D(sampler, offset.xw);
   vec4 sample3 = texture2D(sampler, offset.yw);

   float sx = s.x / (s.x + s.y);
   float sy = s.z / (s.z + s.w);

   return lerp(lerp(sample3, sample2, sx), lerp(sample1, sample0, sx), sy);
}

float brightness(vec3 c) { return max(c.r, max(c.g, c.b)); };

vec3 pow3(vec3 v, float p) {
   return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

float luminance(vec3 linear_rgb) { return dot(linear_rgb, vec3(0.2126729, 0.7151522, 0.0721750)); }

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

// Source - https://stackoverflow.com/a/17897228
// Posted by sam hocevar, modified by community. See post 'Timeline' for change history
// Retrieved 2026-05-22, License - CC BY-SA 4.0

// All components are in the range [0...1], including hue.
vec3 rgb_to_hsv(vec3 c) {
   vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
   vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
   vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

   float d = q.x - min(q.w, q.y);
   float e = 1.0e-10;
   return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv_to_rgb(vec3 c) {
   vec4 K = vec4(1., 2. / 3., 1. / 3., 3.);
   vec3 p = abs(fract(c.xxx + K.xyz) * 6. - K.www);
   return c.z * mix(K.xxx, clamp(p - K.xxx, 0., 1.), c.y);
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

float remap(in float value, in float in_min, in float in_max, in float out_min, in float out_max) {
   const float v = value;
   return (v - in_min) / (in_max - in_min) * (out_max - out_min) + out_min;
}

float smooth_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0, 1.0);
   t = t * t * (3.0 - 2.0 * t);
   return mix(out_min, out_max, t);
}

float pow_remap(float value, float in_min, float in_max, float out_min, float out_max, float gamma) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0, 1.0);
   t = pow(t, gamma);
   return mix(out_min, out_max, t);
}

float log_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   const float v = value;
   float t = clamp((v - in_min) / (in_max - in_min), 0.0001, 1.0);
   t = log(t * 9.0 + 1.0) / log(10.0);
   return mix(out_min, out_max, t);
}

#endif //COMMON_HEADER

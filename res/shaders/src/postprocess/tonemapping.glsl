#include "./aces.glsl"
#include "./agx.glsl"
#include "./agx_minimal.glsl"

// GT7-style tonemapping (approximation)
// Based on Polyphony/GT publications and publicly available sample code.


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

////////////////////////////////////////////////////////////////////////////////
// Lottes 2016, "Advanced Techniques and Optimization of HDR Color Pipelines"
vec3 lottes(vec3 x) {
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

// ------------------------------------------------------------
// Uncharted 2 Filmic Tonemap
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// ACES Tonemap (Unity style)
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// ACES Tonemap (Unreal Engine style)
// ------------------------------------------------------------
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


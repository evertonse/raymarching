

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

#pragma compute
#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform sampler2D scene_hdr;
layout(rgba8, binding = 1) uniform writeonly image2D out_image;

#define lerp mix


uniform float exposure;
#include "./tonemapping.glsl"

vec3 aces(vec3 x) {
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;
   return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

const float gamma_magic_number = 2.233333;

vec3 gamma_correct(vec3 colour) {
   const float gamma = 2.2;
   return pow(colour, vec3(1. / gamma));
}

//
// Fast approximation
// From http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
//
vec3 linear_to_srgb(vec3 linear_rgb) {
   vec3 RGB = linear_rgb;
   vec3 S1 = sqrt(RGB);
   vec3 S2 = sqrt(S1);
   vec3 S3 = sqrt(S2);
   vec3 sRGB = 0.662002687 * S1 + 0.684122060 * S2 - 0.323583601 * S3 - 0.0225411470 * RGB;
   return sRGB;
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

   hdr *= 0.85;

   // tonemap_aces(FragColor.xyz);
   // FragColor.xyz = tonemap_filmic(FragColor.xyz, 1.0);
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz);
   // const float exposure = 0.8;
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz, exposure);
   // vec3 mapped = tonemap_aces_unity(hdr);
   // vec3 mapped = tonemap_gt7(hdr);
   vec3 mapped = tonemap_uchimura(hdr);
   // vec3 mapped = tonemap_aces(hdr);

   mapped += (noise(pixel) - 0.5) / 255.0;

   // mapped = gamma_correct(mapped);
   mapped = linear_to_srgb(mapped);

   imageStore(out_image, pixel, vec4(mapped, 1.0));
}

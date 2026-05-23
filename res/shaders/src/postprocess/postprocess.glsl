#pragma compute
#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;
layout(binding = 0) uniform sampler2D scene_hdr;

// Why the final frabebuffer doesnt work if not rgba instead of rgb?
#define IMAGE_FORMAT rgba8
// #define IMAGE_FORMAT rgba32f
// #define IMAGE_FORMAT r11f_g11f_b10f

layout(IMAGE_FORMAT, binding = 1) uniform writeonly image2D out_image;

#define lerp mix


uniform float exposure = 1.0;;
#include "./tonemapping.glsl"

void main() {

   ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
   ivec2 size = imageSize(out_image);

   if (pixel.x >= size.x || pixel.y >= size.y) {
      return;
   }

   vec2 uv = (vec2(pixel) + 0.5) / vec2(size);

   vec3 hdr = texture(scene_hdr, uv).rgb;

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
   float dither = (noise(pixel) - 0.5) / 255.0;

   vec3 mapped = tonemap_agx_minimal(hdr);
   mapped = linear_to_srgb(mapped);
   mapped += dither;

   imageStore(out_image, pixel, vec4(mapped, 1.0));
}

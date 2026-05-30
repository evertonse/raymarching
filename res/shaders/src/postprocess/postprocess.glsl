#pragma compute
#version 460 core

#include "res/shaders/common.glsl"
#include "src/renderer/shared/defines.glsl"

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = BINDING_FRAMEBUFFER_HDR_SCENE_TEXTURE) uniform sampler2D scene_hdr;
layout(binding = BINDING_FRAMEBUFFER_NORMAL_TEXTURE)    uniform sampler2D framebuffer_normal_texture;
layout(binding = BINDING_FRAMEBUFFER_DEPTH_TEXTURE)     uniform sampler2D framebuffer_depth_texture;

float linearize_depth2(float ndc_depth) {
   float near = near_plane; float far = far_plane;
   float clip_z = ndc_depth * 2.0 - 1.0;
   return (2.0 * near * far) / (far + near - clip_z * (far - near));
}

float linearize_depth1(float depth) {
   float z_near = near_plane; float z_far = far_plane;
   return (z_near * z_far) / (z_far - (z_far - z_near) * depth);
}

float linearize_depth3(float depth) {
   return depth;
}

// Is linearize_depth idempotent?
#define linearize_depth linearize_depth3


// Why the final frabebuffer doesnt work if not rgba instead of rgb?
#define IMAGE_FORMAT rgba8
// #define IMAGE_FORMAT rgba32f
// #define IMAGE_FORMAT r11f_g11f_b10f

layout(IMAGE_FORMAT, binding = BINDING_LDR_SCENE_IMAGE) uniform writeonly image2D out_image;

#define lerp mix

float ssao_depth_only(sampler2D depth_buffer, vec2 uv, float radius_px, float strength) {
   float center = linearize_depth(texture(depth_buffer, uv).r);
   float occlusion = 0.0;
   int samples = 0;

   // Simple 3x3 neighbourhood (no random, just fixed offsets)
   for (float x = -1.0; x <= 1.0; x += 1.0) {
      for (float y = -1.0; y <= 1.0; y += 1.0) {
         if (x == 0.0 && y == 0.0) {
            continue;
         }
         vec2 offset = vec2(x, y) * radius_px / textureSize(depth_buffer, 0);
         float neighbor = linearize_depth(texture(depth_buffer, uv + offset).r);
         // If neighbor is closer (depth value smaller) then it contributes occlusion
         occlusion += step(neighbor, center + 0.01);
         samples++;
      }
   }
   occlusion /= float(samples);
   return 1.0 - occlusion * strength;
}

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

   float depth = texture(framebuffer_depth_texture, uv).r;
   vec3 normal = texture(framebuffer_normal_texture, uv).rgb;

   float ao = ssao_depth_only(framebuffer_depth_texture, uv, 5., 1.0);
   // float occlusion_factor = ao > 0.5 ? 1.0 - ao : 1.;
   float occlusion_factor = 1.0 - saturate(ao);

   vec3 mapped = tonemap_agx_minimal(hdr);
   // mapped *= occlusion_factor;
   mapped = linear_to_srgb(mapped);
   mapped += dither;

   // imageStore(out_image, pixel, vec4(vec3(occlusion_factor), 1.0));
   // imageStore(out_image, pixel, vec4(vec3(depth), 1.0));
   // imageStore(out_image, pixel, vec4(vec3(normal), 1.0));
   imageStore(out_image, pixel, vec4(mapped, 1.0));
}

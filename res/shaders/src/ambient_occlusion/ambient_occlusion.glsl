#pragma compute

#version 460 core

#include "res/shaders/common.glsl"
#include "src/renderer/shared/defines.glsl"
#include "src/deps/lygia/lighting/ssao.glsl"

layout(local_size_x = 8, local_size_y = 8) in;


layout(binding = BINDING_FRAMEBUFFER_DIRECT_LIGHT_TEXTURE) uniform sampler2D framebuffer_direct_light_texture;
layout(binding = BINDING_FRAMEBUFFER_POSITION_TEXTURE)     uniform sampler2D framebuffer_position_texture;
layout(binding = BINDING_FRAMEBUFFER_NORMAL_TEXTURE)       uniform sampler2D framebuffer_normal_texture;
layout(binding = BINDING_FRAMEBUFFER_DEPTH_TEXTURE)        uniform sampler2D framebuffer_depth_texture;

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
// #define IMAGE_FORMAT rgba8
#define IMAGE_FORMAT rgba32f
// #define IMAGE_FORMAT r11f_g11f_b10f

layout(IMAGE_FORMAT, binding = BINDING_AMBIENT_OCCLUSION_IMAGE) uniform writeonly image2D out_image;

vec4 sample_depth(vec2 uv) {
   return vec4(sample_texture(framebuffer_position_texture, uv).z);
}

float ssao_depth_only(sampler2D depth_buffer, vec2 uv, float radius_px, float strength) {
   float center = sample_depth(uv).r;
   float occlusion = 0.0;
   int samples = 0;

   // Simple 3x3 neighbourhood (no random, just fixed offsets)
   for (float x = -1.0; x <= 1.0; x += 1.0) {
      for (float y = -1.0; y <= 1.0; y += 1.0) {
         if (x == 0.0 && y == 0.0) {
            continue;
         }
         vec2 offset = vec2(x, y) * radius_px / textureSize(depth_buffer, 0);
         float neighbor = sample_depth(uv + offset).r;
         // If neighbor is closer (depth value smaller) then it contributes occlusion
         occlusion += step(neighbor, center + 0.01);
         samples++;
      }
   }
   occlusion /= float(samples);
   return 1.0 - occlusion * strength;
}

#include "./ao.glsl"
#include "./ssao.glsl"

void main() {

   ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
   // ivec2 size = textureSize(framebuffer_normal_texture, 0);
   ivec2 size = imageSize(out_image);
   float aspect = size.x / size.y;

   if (pixel.x >= size.x || pixel.y >= size.y) {
      return;
   }

   // Make sure this is a consistent way to compute ts across shaders.
   vec2 uv = (vec2(pixel) + 0.5) / vec2(size);

   float dither = (noise(pixel) - 0.5) / 255.0;

   float depth = texture(framebuffer_depth_texture, uv).r;
   vec3 normal = texture(framebuffer_normal_texture, uv).rgb;
   vec3 position = texture(framebuffer_position_texture, uv).rgb;

   // float ao = ssao_depth_only(framebuffer_depth_texture, uv, 5., 1.0);
   uint choice = 2;
   if (1 == choice) {
      vec4 ao = ssao_hemisphere_normal_aligned(
         uv,
         pixel,
         framebuffer_direct_light_texture,
         framebuffer_normal_texture,
         framebuffer_position_texture
      );
      imageStore(out_image, pixel, vec4(ao.r));
      return;
   }

   if (2 == choice) {
      vec4 ao = ssao_hemisphere_normal_ray_marched(
         uv,
         pixel,
         framebuffer_direct_light_texture,
         framebuffer_normal_texture,
         framebuffer_position_texture
      );
      imageStore(out_image, pixel, vec4(ao));
      return;
   }

   if (3 == choice) {
      vec4 ao = ssao_hemisphere_normal_ray_marched2(
         uv,
         pixel,
         framebuffer_direct_light_texture,
         framebuffer_normal_texture,
         framebuffer_position_texture
      );
      imageStore(out_image, pixel, vec4(ao));
      return;
   }

   if (4 == choice) {
      vec4 vis = getVisibility(uv, pixel, framebuffer_direct_light_texture, framebuffer_normal_texture, framebuffer_position_texture);
      imageStore(out_image, pixel, vis);
      return;
   }
}

#pragma compute
#version 460 core

#include "res/shaders/src/buffers.glsl"

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D scene_texture;
layout(binding = 1) uniform sampler2D bloom_current_texture;

layout(rgba32f, binding = 0) writeonly uniform image2D out_image;

// layout(binding = 2) uniform sampler2D bloom_previous_texture;
layout(rgba32f, binding = 1) coherent uniform image2D bloom_previous_image;

#include "./bloom.h"

const bool  additive_blend          = true;
const float bloom_scale             = 1.0;
const float bloom_temporal_factor   = 250.25;
const bool  use_temporal_bloom      = true;
const bool  see_only_bloom          = false;
const vec3  bloom_tint              = vec3(255., 255., 255.)/255.;

#ifndef sample_texture
#define sample_texture sample_texture_bicubic
#endif
// #define sample_texture texture

void main() {

   ivec2 coordinate = ivec2(gl_GlobalInvocationID.xy);

   ivec2 size = imageSize(out_image);

   if (coordinate.x >= size.x || coordinate.y >= size.y) {
      return;
   }
   unpack_constants(push_constants);

   vec2 uv = (vec2(coordinate) + 0.5) / vec2(size);

   vec3 hdr_scene     = texture(scene_texture, uv).rgb;
   vec3 bloom_current = sample_texture(bloom_current_texture, uv).rgb;
   vec3 bloom_scene   = bloom_current;

   ivec2 bloom_previous_coordinate = ivec2(uv * vec2(imageSize(bloom_previous_image)));

   if (use_temporal_bloom) {
      // vec3 bloom_previous = sample_texture(bloom_previous_texture, uv).rgb;
      vec3 bloom_previous = imageLoad(bloom_previous_image, bloom_previous_coordinate).rgb;

      if (false) {
         bloom_scene = lerp(bloom_current, bloom_previous, bloom_temporal_factor);
      }

      //
      // Lerp Smoothing https://youtu.be/LSNQuFEDOyQ?t=2985
      //
      if (false && 0 == per_frame.delta_time) {
         bloom_scene = vec3(per_frame.delta_time * 20);
      } else {
         // bloom_scene = lerp(bloom_previous, bloom_current, exp2(-bloom_temporal_factor * per_frame.delta_time));
         bloom_scene = lerp_decay(bloom_previous, bloom_current, bloom_temporal_factor, per_frame.delta_time);
         // bloom_scene = lerp(bloom_previous, bloom_current, saturate(1. - exp(per_frame.delta_time/bloom_temporal_factor)));
         imageStore(bloom_previous_image, bloom_previous_coordinate, vec4(bloom_scene, 1.0));
      }
   }

   if (see_only_bloom) {
      imageStore(out_image, coordinate, vec4(bloom_scene, 1.0));
      return;
   }


   bloom_scene *= bloom_tint;

   vec3 result = lerp(hdr_scene, bloom_scene * bloom_scale, bloom.strength);
   if (additive_blend) {
      result = hdr_scene + bloom_scene * bloom_scale * bloom.strength ;
   }

   imageStore(out_image, coordinate, vec4(result, 1.0));
};

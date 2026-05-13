
#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D scene_texture;
layout(binding = 1) uniform sampler2D bloom_current_texture;
layout(binding = 2) uniform sampler2D bloom_previous_texture;

layout(rgba32f, binding = 2) writeonly uniform image2D out_img;

#include "./bloom.h"

const bool use_temporal_bloom = true;
const bool see_only_bloom = false;
const bool additive_blend = false;

void main() {

   ivec2 coordinate = ivec2(gl_GlobalInvocationID.xy);

   ivec2 size = imageSize(out_img);

   if (coordinate.x >= size.x || coordinate.y >= size.y) {
      return;
   }
   unpack_constants(push_constants);

   vec2 uv = (vec2(coordinate) + 0.5) / vec2(size);

   vec3 hdr_scene  = texture(scene_texture, uv).rgb;
   vec3 bloom_current  = texture(bloom_current_texture, uv).rgb;
   vec3 bloom_scene    = bloom_current;

   if (use_temporal_bloom) {
      const float t = 0.35;
      vec3 bloom_previous = texture(bloom_previous_texture, uv).rgb;
      bloom_scene = lerp(bloom_current, bloom_previous, t);
   }

   if (see_only_bloom) {
      imageStore(out_img, coordinate, vec4(bloom_scene, 1.0));
      return;
   }

   vec3 result = lerp(hdr_scene, bloom_scene, bloom.strength);
   if (additive_blend) {
      result = hdr_scene + bloom_scene * bloom.strength;
   }

   imageStore(out_img, coordinate, vec4(result, 1.0));
};

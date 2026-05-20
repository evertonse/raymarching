
#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D scene_texture;
layout(binding = 1) uniform sampler2D bloom_current_texture;
layout(binding = 2) uniform sampler2D bloom_previous_texture;

layout(rgba32f, binding = 2) writeonly uniform image2D out_image;

#include "./bloom.h"

const bool  additive_blend        = true;
const float bloom_scale           = 1.0;
const float bloom_temporal_factor = 0.35;
const bool  use_temporal_bloom    = true;
const bool  see_only_bloom        = false;
const vec3  bloom_tint            = vec3(255., 255., 255.)/255.;

#define sample_texture sample_texture_bicubic
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

   if (use_temporal_bloom) {
      const float t = bloom_temporal_factor;
      vec3 bloom_previous = sample_texture(bloom_previous_texture, uv).rgb;
      bloom_scene = lerp(bloom_current, bloom_previous, t);
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

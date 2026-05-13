#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D src_texture;
layout(rgba32f, binding = 1) uniform image2D dst_image;

#include "./bloom.h"

const bool filter_radius_based_on_current_texture_size = false;

// #define sample_texture sample_bicubic // TODO: make this exist
#define sample_texture texture


vec3 sample_tent(vec2 uv, vec2 texel) {

   vec3 c = vec3(0.0);

   c += sample_texture(src_texture, uv + texel * vec2(-1, -1)).rgb * 1.0;
   c += sample_texture(src_texture, uv + texel * vec2( 0, -1)).rgb * 2.0;
   c += sample_texture(src_texture, uv + texel * vec2( 1, -1)).rgb * 1.0;

   c += sample_texture(src_texture, uv + texel * vec2(-1,  0)).rgb * 2.0;
   c += sample_texture(src_texture, uv + texel * vec2( 0,  0)).rgb * 4.0;
   c += sample_texture(src_texture, uv + texel * vec2( 1,  0)).rgb * 2.0;

   c += sample_texture(src_texture, uv + texel * vec2(-1,  1)).rgb * 1.0;
   c += sample_texture(src_texture, uv + texel * vec2( 0,  1)).rgb * 2.0;
   c += sample_texture(src_texture, uv + texel * vec2( 1,  1)).rgb * 1.0;

   return c / 16.0;
}


void main() {
   ivec2 coordinate = ivec2(gl_GlobalInvocationID.xy);

   ivec2 size = imageSize(dst_image);

   if (coordinate.x >= size.x || coordinate.y >= size.y) {
      return;
   }
   unpack_constants(push_constants);

   vec2 uv = (vec2(coordinate) + 0.5) / vec2(size);

   // It's important that filter_radius is in texture coordinate not in pixel so it scales independently of the texture size;
   // Yes we know there's a ton of unneeded calculation per pixel that should be dont in cpu once. Yes, we don't care.
   const float scatter = 1.0;
   const float aspect_ratio = float(bloom.src_width) / bloom.src_height;

   vec2  texel = vec2(bloom.filter_radius, bloom.filter_radius*aspect_ratio) * scatter;
   if (filter_radius_based_on_current_texture_size) {
      texel = 1.0 / vec2(textureSize(src_texture, 0)) * scatter;
   }

   vec4 high_mip = imageLoad(dst_image, coordinate);  // current mip content
   vec3 low_mip  = sample_tent(uv, texel);            // blurred lower mip

   imageStore(dst_image, coordinate, vec4(high_mip.rgb + low_mip, 1.));
};

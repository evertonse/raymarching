#version 460 core

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D src_texture;
layout(rgba32f, binding = 1) uniform image2D dst_image;

#define sample_texture sample_texture_bicubic
#include "res/shaders/common.glsl"
#include "./bloom.h"


const bool filter_radius_based_on_current_texture_size = false;
const bool use_scattering = false;
// Ways to control the bloom emanating radius from bright objects
//   - use scattering and set to a value lower than .5
//   - set clamp max value for prefilter stage
//   - decrease the color instensity from object (bad because object will prolly not keep it's bright white core)
const float scatter_value = .45; // set this to bigger than one to see some shit.
const float scatter = lerp(0.05f, 0.95f, scatter_value);



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
   const float aspect_ratio = float(bloom.src_width) / float(bloom.src_height);


   vec2 texel = vec2(bloom.filter_radius, bloom.filter_radius*aspect_ratio);
   // vec2  texel = vec2(bloom.filter_radius, bloom.filter_radius) * scatter;

   if (filter_radius_based_on_current_texture_size) {
      texel = 1.0 / vec2(textureSize(src_texture, 0));
   }

   vec3 high_mip = imageLoad(dst_image, coordinate).rgb;  // current high mip content
   vec3 low_mip  = sample_tent(uv, texel);                // blurred lower mip



   vec3 out_color;
   if (use_scattering) {
      out_color = 2*lerp(high_mip, low_mip, scatter);
   } else {
      out_color = high_mip + low_mip;
   }


   imageStore(dst_image, coordinate, vec4(out_color, 1.));
};

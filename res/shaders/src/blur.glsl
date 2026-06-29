#pragma compute
#version 460 core

#include "res/shaders/common.glsl"
#include "src/renderer/shared/defines.glsl"
#include "src/deps/lygia/filter/boxBlur/2D.glsl"

#include "src/deps/lygia/filter/gaussianBlur/2D.glsl"


layout(binding = BINDING_BLUR_INPUT_TEXTURE) uniform sampler2D input_texture;

// #define IMAGE_FORMAT rgba8
#define IMAGE_FORMAT rgba32f
// #define IMAGE_FORMAT r11f_g11f_b10f

layout(IMAGE_FORMAT, binding = BINDING_BLUR_IMAGE) uniform writeonly image2D output_image;

layout(local_size_x = 8, local_size_y = 8) in;

// vec3 sample_tent(vec2 uv, vec2 texel) {
//
//    vec3 c = vec3(0.0);
//
//    c += sample_texture(src_texture, uv + texel * vec2(-1, -1)).rgb * 1.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 0, -1)).rgb * 2.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 1, -1)).rgb * 1.0;
//
//    c += sample_texture(src_texture, uv + texel * vec2(-1,  0)).rgb * 2.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 0,  0)).rgb * 4.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 1,  0)).rgb * 2.0;
//
//    c += sample_texture(src_texture, uv + texel * vec2(-1,  1)).rgb * 1.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 0,  1)).rgb * 2.0;
//    c += sample_texture(src_texture, uv + texel * vec2( 1,  1)).rgb * 1.0;
//
//    return c / 16.0;
// }

void main() {

   ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
   ivec2 size = imageSize(output_image);
   float aspect = size.x / size.y;

   if (pixel.x >= size.x || pixel.y >= size.y) {
      return;
   }
   vec2 pixel_size = 1.0 / vec2(size);
   vec2 uv = (vec2(pixel) + 0.5) / vec2(size);

   const int kernel_size = 5;
   vec4 blur = boxBlur2D(input_texture, uv, pixel_size, kernel_size);
   // vec4 blur =  gaussianBlur2D(input_texture, uv, pixel_size, kernel_size);

   imageStore(output_image, pixel, blur);
}

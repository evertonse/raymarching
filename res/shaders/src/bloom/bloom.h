#include "src/renderer/shared/defines.glsl"
#include "src/renderer/shared/bloom_data.glsl"

layout(location = 0) uniform vec4 push_constants[2];

Bloom_Data bloom;
void unpack_constants(vec4 push_constants[2]) {

   bloom.strength =
      push_constants[0].x;

   bloom.filter_radius =
      push_constants[0].y;

   bloom.mip_level =
      floatBitsToInt(push_constants[0].z);

   bloom.prefilter_clamp_max =
      push_constants[0].w;

   bloom.prefilter_threshold =
      push_constants[1].x;

   bloom.prefilter_knee =
      push_constants[1].y;

   bloom.src_width =
      floatBitsToInt(push_constants[1].z);

   bloom.src_height =
      floatBitsToInt(push_constants[1].w);
}


// Taken from https://observablehq.com/@rreusser/bicubic-texture-interpolation-using-linear-filtering
// Also present in https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl#42179924

vec4 cubic(float v) {
   vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
   vec4 s = n * n * n;
   float x = s.x;
   float y = s.y - 4.0 * s.x;
   float z = s.z - 4.0 * s.y + 6.0 * s.x;
   float w = 6.0 - x - y - z;
   return vec4(x, y, z, w) * (1.0 / 6.0);
}

vec4 sample_texture_bicubic(sampler2D sampler, vec2 uv) {
   vec2 texture_resolution = textureSize(sampler, 0);
   vec2 inverse_texture_resolution = 1.0 / texture_resolution;
   uv = uv * texture_resolution - 0.5;
   vec2 fxy = fract(uv);
   uv -= fxy;

   vec4 xcubic = cubic(fxy.x);
   vec4 ycubic = cubic(fxy.y);

   vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;

   vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
   vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

   offset *= inverse_texture_resolution.xxyy;

   vec4 sample0 = texture2D(sampler, offset.xz);
   vec4 sample1 = texture2D(sampler, offset.yz);
   vec4 sample2 = texture2D(sampler, offset.xw);
   vec4 sample3 = texture2D(sampler, offset.yw);

   float sx = s.x / (s.x + s.y);
   float sy = s.z / (s.z + s.w);

   return lerp(lerp(sample3, sample2, sx), lerp(sample1, sample0, sx), sy);
}

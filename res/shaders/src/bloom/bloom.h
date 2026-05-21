#include "res/shaders/common.glsl"
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


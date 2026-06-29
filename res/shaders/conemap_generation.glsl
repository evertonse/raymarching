#pragma compute
#version 460 core

// For some reason we crash in  glFinish when including this ? What !! #include "res/shaders/common.glsl"
layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D height_map;
layout(binding = 1) uniform sampler2D cone_map_in;
layout(rgba32f, binding = 0) uniform image2D cone_map_out;

uniform ivec2 resolution; // texture resolution
uniform bool use_buffer = false;

layout(std430, binding = 0) readonly buffer Texture_Coordinates_In_Resolution_Space {
    ivec2 xy_js[]; // xys are integer texture coordinates, these aren't 0.0 to 1.0 normalized float values, we name this uvs.
};

#define sample_texture texture
float sample_height(vec2 uv) {
   return sample_texture(height_map, uv).r;
   // return 1. - sample_texture(height_map, uv).r;
}

// NOTE: QUOTE: Mipmapping should *NOT* be applied to cone maps, because the filtered values would lead to incorrect intersections.
//              Instead, one should compute the mipmaps manually, by conservatively taking the minimum value for each group of pixels
float depth2relaxedcone(vec2 texture_coordinate_i, vec2 texture_coordinate_j) {
   const int search_steps = 128;
   // const int search_steps = 128 * 2 * 2;

   vec3 p = vec3(texture_coordinate_i, 0);

   vec3 o = vec3(texture_coordinate_j, 0);
   o.z = sample_height(texture_coordinate_j);

   vec3 v = o - p;    // View ray from ti to tj
   if (false) {
      v /= v.z;       // Make sure v.z is 1. (maximum depth)
   } else {
      v /= max(v.z, 0.0001f);
   }
   v *= 1.0 - o.z;
   v /= search_steps; // Scale based on number of steps. Walks slower if more steps.

   p = o;

   for (int i = 0; i < search_steps; i += 1) {
      float d = sample_height(p.xy);
      if (d <= p.z) {
         p += v;
      }
   }
   float start_depth = sample_height(texture_coordinate_i);
   float r = 1.0; // Max ratio start
   if (p.z < start_depth) {
      r = length(p.xy - texture_coordinate_i) / (start_depth - p.z);
   }

   return r;
}

float load_previous_min_ratio(ivec2 xy) {
#if 0
   return imageLoad(cone_map_out, xy).g;
#else
   vec2 uv = (vec2(xy) + 0.5) / vec2(resolution);
   return texture(cone_map_in, uv).g; // .g stores the min ratio
#endif
}

void store_current_min_ratio(ivec2 xy, vec2 uv, float min_ratio) {
   vec4 result = vec4(vec3(min_ratio), sample_height(uv));
   // vec4 test = vec4(gl_LocalInvocationIndex/256.);
   // vec4 test = imageLoad(cone_map_out, xy);
   imageStore(cone_map_out, xy, result);
}


void generate_by_radius(in const ivec2 xy_i, in const vec2 uv_i) {
#if 0
   // It'll go through the full texture
   // can cause crash in glFinish by a TDR (Timeout Detection and Recovery)
   const bool is_circle = false;
   const ivec2 radius = resolution;
#else
   const bool is_circle = true;
   const ivec2 radius = ivec2(128);
#endif

   const int radius_squared = radius.x * radius.y;

   float min_ratio = load_previous_min_ratio(xy_i);
   // Loop over local neighbourhood
   for (int dy = -radius.y; dy <= radius.y; dy += 1) {
      for (int dx = -radius.x; dx <= radius.x; dx += 1) {
         const ivec2 xy_j = xy_i + ivec2(dx, dy);
         // Skip out of bounds, the center pixel itself and samples outside the circle radius
         if (dx == 0 && dy == 0) continue;
         if (is_circle && dx*dx + dy*dy > radius_squared) continue;
         if (xy_j.x < 0 || xy_j.x >= resolution.x || xy_j.y < 0 || xy_j.y >= resolution.y || (dx == 0 && dy == 0)) {
            continue;
         }

         const vec2 uv_j = (vec2(xy_j) + 0.5) / vec2(resolution);

         float ratio = depth2relaxedcone(uv_i, uv_j);
         min_ratio = min(min_ratio, ratio);
      }
   }

   store_current_min_ratio(xy_i, uv_i, min_ratio);
}

void generate_by_storage_buffer(in const ivec2 xy_i, in const vec2 uv_i) {
   float min_ratio = load_previous_min_ratio(xy_i);

   for (int k = 0; k < xy_js.length(); k += 1) {
      ivec2 xy_j = xy_js[k];
      if (xy_j.x == xy_i.x && xy_j.y == xy_i.y) {
         continue;
      }
      const vec2 uv_j = (vec2(xy_j) + 0.5) / vec2(resolution);
      min_ratio = min(min_ratio, depth2relaxedcone(uv_i, uv_j));
   }

   store_current_min_ratio(xy_i, uv_i, min_ratio);
}

void main() {
   ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
   if (xy.x >= resolution.x || xy.y >= resolution.y) {
      return;
   }

   const vec2 uv = (vec2(xy) + vec2(0.5)) / vec2(resolution);

   if (use_buffer) {
      generate_by_storage_buffer(xy, uv);
   } else {
      generate_by_radius(xy, uv);
   }

   return;
}



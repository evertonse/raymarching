#version 460 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 12) uniform sampler2D height_map;
layout(binding = 11) uniform sampler2D cone_map_in;
layout(rgba32f, binding = 5) uniform image2D cone_map_out;

uniform ivec2 resolution;      // texture resolution
uniform vec3  offset;
uniform int   is_first_time;
uniform int   is_partial;
uniform ivec2 resolution_coordinate_j; // Integer texture coordinate in resolution space.

float sample_height(vec2 uv) {
   return texture(height_map, uv).r;
}

// NOTE: QUOTE: mipmapping should not be applied to cone maps, because the filtered values would lead to incorrect intersections.
// Instead, one should compute the mipmaps manually, by conservatively taking the minimum value for each group of pixels
float depth2relaxedcone(vec2 texture_coordinate_i, vec2 texture_coordinate_j) {
   // const int search_steps = 128;
   // const int search_steps = 128 * 2 * 2;
   const int search_steps = 256;

   vec3 p = vec3(texture_coordinate_i, 0);

   vec3 o = vec3(texture_coordinate_j, 0);
   o.z = sample_height(texture_coordinate_j);

   vec3 v = o - p;    // View ray from ti to tj
   v /= v.z;          // Make sure v.z is 1. (maximum depth)
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

const uint searchSteps = 1024*8;
const float oneOverSearchSteps = 1.0/searchSteps;

vec2 texCoord(ivec2 texelInd) {
   ivec2 xy = texelInd;
   const vec2 uv = (vec2(xy) + vec2(0.5)) / vec2(resolution);
   return uv;
}

float getRelaxedCone(float baseHeight, vec2 baseTexCoord, ivec2 texelInd, float minRatio) {
   vec2 t = texCoord(texelInd);

   vec3 src     = vec3(baseTexCoord, 1 + 0.001);
   float height = texture(height_map, t).r;
   vec3 dst     = vec3(t, height);

   if ((dst.z <= baseHeight) || length(dst.xy - baseTexCoord) > minRatio * (dst.z - baseHeight)) {
      return 1.0;
   }

   vec3 vec = dst - src;                     // Ray direction
   vec /= -vec.z;                            // Scale ray direction so that vec.z = -1.0
   vec *= dst.z;                             // Scale again
   vec3 step_fwd = vec * oneOverSearchSteps; // Length of a forward step
   // Search until a new point outside the surface
   vec3 ray_pos = dst + step_fwd;
   for (uint i = 1; i < 2; i += 1) {
      float current_height = texture(height_map, ray_pos.xy).r;
      if (current_height >= ray_pos.z) {
         ray_pos += step_fwd;
      } else {
         break;
      }
   }
   // Original texel depth
   float src_texel_height = baseHeight;

   // Compute the cone ratio
   float cone_ratio = 1.0;
   if (ray_pos.z > src_texel_height) {
      // cone_ratio = length(ray_pos.xy - baseTexCoord);
      cone_ratio = length(ray_pos.xy - baseTexCoord) / (ray_pos.z - src_texel_height);
   }
   return cone_ratio;
}

float getCone(float baseHeight, vec2 baseTexCoord, ivec2 texelInd, float minRatio) {
   return getRelaxedCone(baseHeight, baseTexCoord, texelInd, minRatio);
}


void main2(ivec2 xy, vec2 uv) {
   vec2  baseT = uv; // texture coords
   float baseH = sample_height(uv);

   float minTan = 1.0;
   uint w = resolution.x;
   uint h = resolution.y;
   for (uint i = 0; i < w; ++i) {
      for (uint j = 0; j < h; ++j) {
         ivec2 id = ivec2(i, j);
         // if ((xy.x != id.x || xy.y != id.y)) {
         // }
         minTan = min(minTan, getCone(baseH, baseT, id, minTan));
      }
   }
   vec4 result = vec4(vec3(minTan), baseH);
   // vec4 result = vec4(vec3(baseH), 1.0);
   // vec4 result = vec4(1.0, 0, 0, 1.0);
   imageStore(cone_map_out, xy, result);
}


void main_full(ivec2 xy, vec2 uv) {
   const vec2 ti = uv; // texture coords
   float current_height = sample_height(uv);

   float min_ratio = 1.0;
   uint width = resolution.x;
   uint height = resolution.y;

   for (uint i = 0; i < width; ++i) {
      for (uint j = 0; j < height; ++j) {
         if ((xy.x != i || xy.y != j)) {
            const vec2 tj = (vec2(i, j) + vec2(0.5)) / vec2(resolution);
            min_ratio = min(min_ratio, depth2relaxedcone(ti, tj));
         }
      }
   }

   vec4 result = vec4(vec3(min_ratio), current_height);
   imageStore(cone_map_out, xy, result);
}


void main_partial(ivec2 xy, vec2 uv) {
   const vec2 ti = uv;
   const vec2 tj = (vec2(resolution_coordinate_j.x, resolution_coordinate_j.y) + vec2(0.5)) / vec2(resolution);
   float current_height = sample_height(uv);

   float min_ratio = 1.0;
   if (0 == is_first_time) {
      min_ratio = texture(cone_map_in, uv).x;
   }

   if ((xy.x != resolution_coordinate_j.x || xy.y != resolution_coordinate_j.y)) {
      min_ratio = min(min_ratio, depth2relaxedcone(ti, tj));
   }

   vec4 result = vec4(vec3(min_ratio), current_height);
   imageStore(cone_map_out, xy, result);
}

void main_region(ivec2 xy, vec2 uv) {
   // Source Texel coordinate
   const vec2 ti = uv;
   float current_height = sample_height(uv);

   float min_ratio = 1.0;
   int width  = resolution.x;
   int height = resolution.y;

   // radius in texels
   int radius_texels = int(min(width, height)) / 8;
   radius_texels = max(radius_texels, 4); // clamp to at least 4 texels

   for (int dy = -radius_texels; dy <= radius_texels; ++dy) {
      for (int dx = -radius_texels; dx <= radius_texels; ++dx) {
         int ix = xy.x + dx;
         int iy = xy.y + dy;

         // skip self
         if (dx == 0 && dy == 0) {
            continue;
         }

         // skip out of bounds
         if (ix < 0 || iy < 0 || ix >= width || iy >= height) {
            continue;
         }

         // circular region check
         float dist2 = float(dx * dx + dy * dy);
         const bool do_circular = false;
         if (do_circular && (dist2 > float(radius_texels * radius_texels))) {
            continue;
         }

         // Destination texel
         vec2 tj = (vec2(ix, iy) + vec2(0.5)) / vec2(resolution);

         // compute relaxed cone ratio
         float ratio = depth2relaxedcone(ti, tj);

         // keep smallest ratio (tightest cone)
         min_ratio = min(min_ratio, ratio);
      }
   }

   // write result
   vec4 result = vec4(vec3(min_ratio), current_height);
   imageStore(cone_map_out, xy, result);
}

void main_postprocess_max(ivec2 xy, vec2 uv) {
   const int w = resolution.x;
   const int h = resolution.y;
   const ivec2 baseIJ = xy;
   vec2 texel_val = texture(cone_map_in, uv).ba;

   for (int i = -1; i <= 1; ++i) {
      for (int j = -1; j <= 1; ++j) {
         ivec2 n_IJ = baseIJ + ivec2(i, j);
         n_IJ.x = clamp(n_IJ.x, 0, w - 1);
         n_IJ.y = clamp(n_IJ.y, 0, h - 1);
         vec2 n_uv = (vec2(n_IJ) + vec2(0.5)) / vec2(resolution);

         // coneMap_in.Load(int3(n_IJ, srcLevel)).g;
         // const float n_val = texture(cone_map_in, n_uv).b;
         const float n_val = texture(cone_map_in, n_uv).a;
         texel_val.y = min(texel_val.y, n_val);
      }
   }

   vec4 result = vec4(vec3(texel_val.x),texel_val.y);
   imageStore(cone_map_out, xy, result);
}

void main() {
   ivec2 xy = ivec2(gl_GlobalInvocationID.xy);
   if (xy.x >= resolution.x || xy.y >= resolution.y) {
      return;
   }

   // int offsetx = g_SamplingGroupX*g_SamplingGroupSize+(g_SamplingOrder[g_SamplingGroupPos]%g_SamplingGroupSize);
   // int offsety = g_SamplingGroupY*g_SamplingGroupSize+(g_SamplingOrder[g_SamplingGroupPos]/g_SamplingGroupSize);
   // offset.x = (offsetx - int(resolution.x)/2)/(float)resolution.x+0.5/resolution.x;
   // offset.y = (offsety - int(resolution.y)/2)/(float)resolution.y+0.5/resolution.y;
   // offset.z =0;

   const vec2 uv = (vec2(xy) + vec2(0.5)) / vec2(resolution);
   main_partial(xy, uv);

   // if (1 == is_first_time) {
   //    main_postprocess_max(xy, uv);
   // }

   return;

	// const vec2 uv_y_inverted = uv * vec2(1.,-1.);
	//
 //   float best_cone_ratio = depth2relaxedcone(uv, uv_y_inverted);
	//
 //   float height = texture(height_map, uv).a;
 //   vec4  result = vec4(best_cone_ratio);
 //   // vec4  result = vec4(vec3(best_cone_ratio), height);
 //   // vec4  result = vec4(1, 0, 0., height);
	//
 //   // imageStore(cone_map_out, xy, result);
 //   imageStore(cone_map_out, xy, result);
}


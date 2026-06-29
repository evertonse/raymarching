#define PARALLAXMAPPING_OCCLUSION
#define PARALLAXMAPPING_NUMSEARCHES 32
#include "src/deps/lygia/space/parallaxMapping.glsl"
// Inputs:
//   ro         - ray origin in tangent space (vec3)
//   rd         - ray direction in tangent space (points INTO the surface) (vec3)
//   uv0        - starting uv for the ray origin (vec2)  (the uv of ro)
//   depthMap   - sampler2D containing depth in [0,1] (0 -> z=0, 1 -> z=-1)
//   texelSize  - vec2(1.0/width, 1.0/height) for FD dx/dy
//   maxSteps   - max march steps (int)
//   tMax       - max distance to march (float)
// Outputs (by reference or out params):
//   out_hit    - bool whether we hit
//   out_t      - distance along ray p = ro + t*rd
//   out_uv     - uv at hit
//   out_pos    - hit position in tangent space
//   out_normal - geometric normal in tangent space
//
// Notes:
//  - This code uses central-difference gradient with offset equal to texelSize.
//  - Adjust EPS_UV and the refinement iteration count to taste.
#define one (sin(per_frame.elapsed_time)+1.0 / 2.)

float parallax_sample_height(sampler2D map, vec2 uv) {
   float h = (1.0 - sample_texture_bicubic(map, uv).r) * 0.092;
   return h;
}


vec2 ParallaxMapping(
    vec2 uv,                 // base UV coordinates
    vec3 viewDirTS,          // view direction in tangent space (pointing into surface)
    sampler2D depthMap,      // height map (R channel)
    float minLayers,         // minimum number of steps
    float maxLayers,         // maximum number of steps
    float scale,             // parallax scale (depth scale)
    float bias,              // parallax bias
    int searchSteps          // refinement steps (binary search)
) {
    // Determine number of layers depending on view angle
    float numLayers = lerp(maxLayers, minLayers, abs(viewDirTS.z));
    float layerDepth = 1.0 / numLayers;

    // Per-step UV shift (scaled by parallax depth scale)
    vec2 P = viewDirTS.xy / -viewDirTS.z * scale;
    vec2 deltaUV = P * layerDepth;

    // Initial position in parallax "volume"
    vec2  currUV   = uv;
    float currDepth = 0.0;
    float sampledH = texture(depthMap, currUV).r;

    // --- Steep parallax mapping: march until ray goes under heightmap
    while (currDepth < sampledH && currDepth < 1.0) {
        currUV -= deltaUV;
        currDepth += layerDepth;
        sampledH = texture(depthMap, currUV).r;
    }

    // --- Relief mapping refinement: binary search around intersection
    vec2  prevUV   = currUV + deltaUV;
    float prevDepth = currDepth - layerDepth;
    for (int i = 0; i < searchSteps; i++) {
        vec2 midUV = (currUV + prevUV) * 0.5;
        float midDepth = (currDepth + prevDepth) * 0.5;
        float H = texture(depthMap, midUV).r;

        if (midDepth < H) {
            prevUV = midUV;
            prevDepth = midDepth;
        } else {
            currUV = midUV;
            currDepth = midDepth;
        }
    }

    return currUV; // final parallax-corrected UV
}

vec2 parallaxMapping(
   vec2 uv,
   vec3 view_dir_ts,
   sampler2D depth_map,
   float min_layers,
   float max_layers,
   float scale,
   float bias,
   int search_steps
) {
   // pick number of layers based on view angle
   float num_layers = lerp(max_layers, min_layers, abs(view_dir_ts.z));
   float layer_depth = 1.0 / num_layers;

   // per-step uv shift in tangent space
   vec2 p = view_dir_ts.xy / -view_dir_ts.z * scale;
   vec2 delta_uv = p * layer_depth;

   // ray start
   vec2 curr_uv = uv;
   float curr_depth = 0.0;
   float sampled_h = texture(depth_map, curr_uv).r;

   // steep parallax march
   while (curr_depth < sampled_h && curr_depth < 1.0) {
      curr_uv -= delta_uv;
      curr_depth += layer_depth;
      sampled_h = texture(depth_map, curr_uv).r;
   }

   // previous step
   vec2 prev_uv = curr_uv + delta_uv;
   float prev_depth = curr_depth - layer_depth;

   // refinement with binary search
   for (int i = 0; i < search_steps; i++) {
      vec2 mid_uv = (curr_uv + prev_uv) * 0.5;
      float mid_depth = (curr_depth + prev_depth) * 0.5;
      float h = texture(depth_map, mid_uv).r;

      if (mid_depth < h) {
         prev_uv = mid_uv;
         prev_depth = mid_depth;
      } else {
         curr_uv = mid_uv;
         curr_depth = mid_depth;
      }
   }

   // final linear interpolation between last two points
   float after_h = texture(depth_map, curr_uv).r;
   float before_h = texture(depth_map, prev_uv).r;
   float t = (after_h - curr_depth) / ((after_h - curr_depth) - (before_h - prev_depth));
   vec2 final_uv = lerp(curr_uv, prev_uv, clamp(t, 0.0, 1.0));

   return final_uv;
}


vec2 relief_mapping(
   vec2 uv,
   vec3 position_ts,         // current fragment position in tangent space
   vec3 view_dir_ts,
   sampler2D depth_map,
   float scale,
   vec3 to_light_ts,         // light direction in tangent space
   out vec3  out_pos_ts,      // intersection (u,v,depth)
   out float out_shadow      // shadow factor (0.0 lit, 1.0 shadow)
) {
   const float min_layers = 64;
   const float max_layers = 64;
   const int search_steps = 32;

   // Primary view-ray search
   float num_layers = lerp(max_layers, min_layers, abs(view_dir_ts.z));
   vec2 A = uv;
   vec3 V = (view_dir_ts / -view_dir_ts.z) * scale;
   vec2 B = A + V.xy;


   // linear search
   float t = 0.0;
   for (int i = 0; i < int(num_layers); i++) {
      t += 1.0 / num_layers;
      float d =  parallax_sample_height(depth_map, lerp(A, B, t));
      if (t > d) break;
   }

   float depth_a = t - (1.0 / num_layers);
   float depth_b = t;

   // binary search refinement
   int binary_search_steps = search_steps;
   float depth = 0.0;
   for (int i = 0; i < binary_search_steps; i++) {
      depth = lerp(depth_a, depth_b, 0.5);
      float d = parallax_sample_height(depth_map, lerp(A, B, depth));
      if (d > depth) {
         depth_a = depth;
      } else {
         depth_b = depth;
      }
   }

   // final intersection
   out_pos_ts = vec3(lerp(A, B, depth), depth);

   // ----------------------
   // Shadow ray search
   // ----------------------
   vec3 p_tan = position_ts + V * depth;
   vec3 p_to_light = (position_ts + to_light_ts) - p_tan;

   vec3 l_entry = out_pos_ts + (p_to_light / p_to_light.z) * scale * depth;
   vec3 l_exit  = l_entry + (p_to_light / -p_to_light.z) * scale;

   // secondary relief search (linear + binary, same as above)
   float lt = 0.0;
   for (int i = 0; i < int(num_layers); i++) {
      lt += 1.0 / num_layers;
      float d = parallax_sample_height(depth_map, lerp(l_entry.xy, l_exit.xy, lt));
      if (lt > d) break;
   }

   float ldepth_a = lt - (1.0 / num_layers);
   float ldepth_b = lt;
   float ldepth = 0.0;

   for (int i = 0; i < search_steps; i++) {
      ldepth = lerp(ldepth_a, ldepth_b, 0.5);
      float d = parallax_sample_height(depth_map, lerp(l_entry.xy, l_exit.xy, ldepth));
      if (d > ldepth) {
         ldepth_a = ldepth;
      } else {
         ldepth_b = ldepth;
      }
   }

   // if secondary intersection is closer than the view-ray intersection -> shadowed
   out_shadow = (ldepth < depth - 0.05) ? 1.0 : 0.0;

   // return parallax-corrected uv
   return out_pos_ts.xy;
}


vec2 sample_height_and_cone_ratio(sampler2D relaxedcone_relief_map, vec2 uv) {
   vec4 tex = sample_texture(relaxedcone_relief_map, uv);
#if 1
   float height     = 1.0 - tex.a;
   float cone_ratio = tex.b;
#else
   float height     = tex.r;
   float cone_ratio = tex.g;
#endif
   return vec2(height, cone_ratio);
}


// https://github.com/tomosud/RelaxedConeMap
vec2 ray_intersect_relaxedcone2(sampler2D relaxedcone_relief_map, inout vec3 p, inout vec3 v) {

   const int binary_steps = 16;
   const int cone_steps = 32;
   // v is already in tangent space, XY = along surface, Z = into surface
   const float depth_scale = 0.15;
   // depth_scale should match your parallax height uniform (e.g. 0.05 - 0.2)

   vec3 dir = vec3(v.x, v.y, -v.z / depth_scale); // NOTE: no negation if v.z already points inward
   if (dir.z < 1e-5) {
      return p.xy;
   }
   dir /= dir.z;
   float rr = length(dir.xy);

   vec2 uv0 = p.xy;
   p.z = 0.0; // start at surface

   for (int i = 0; i < cone_steps; i++) {
      vec2 tex = sample_height_and_cone_ratio(relaxedcone_relief_map, p.xy);
      float d = 1.0 - tex.x; // depth = 1 - height
      float c = max(tex.y, 0.002);
      float h = d - p.z;
      if (h <= 0.001) {
         break;
      }
      p += dir * (c * h / (rr + c));
      if (p.z >= 1.0) {
         break;
      }
   }
   if (p.z > 1.0) {
      p += dir * (1.0 - p.z);
   }

   // binary search refinement
   float lo = 0.0, hi = p.z;
   for (int i = 0; i < binary_steps; i++) {
      float mid = 0.5 * (lo + hi);
      vec3 q = vec3(uv0, 0.0) + dir * mid;
      vec2 tex = sample_height_and_cone_ratio(relaxedcone_relief_map, q.xy);
      if (q.z < (1.0 - tex.x)) {
         lo = mid;
      } else {
         hi = mid;
      }
   }
   p = vec3(uv0, 0.0) + dir * hi;
   return p.xy;
}


vec2 ray_intersect_relaxedcone3(sampler2D relaxedcone_relief_map, inout vec3 p, inout vec3 v) {
   // vec3 wdir = normalize(vWorld - uCam);
   float ds = max(0.15, 1e-4);
   vec3 dir = vec3(v.xy, -v.z / ds);
   if (dir.z < 1e-5) {
      // return p.xy;
   }
   dir /= dir.z; // dir.z = 1 (advanced normalized)
   float rr = length(dir.xy);

   vec2 uv0 = p.xy;
   // --- relaxed cone stepping ---
   // vec3 p = vec3(uv0, 0.0);
   for (int i = 0; i < 64; i++) {
      if (i >= 128) {
         break;
      }

      vec2 tex = sample_height_and_cone_ratio(relaxedcone_relief_map, p.xy);
      float d = 1. - tex.x;
      float c = tex.y;
      float h = d - p.z;
      if (h <= 0.001) {
         break;
      }
      p += dir * (c * h / (rr + c));
      if (p.z >= 1.0) {
         break;
      }
   }

   if (p.z > 1.0) {
      p += dir * (1.0 - p.z);
   }

   // --- refine intersection with binary search (relaxed cone guarantees at most one crossing) ---
   float lo = 0.0, hi = p.z;
   for (int i = 0; i < 16; i++) {
      float mid = 0.5 * (lo + hi);
      vec3 q = vec3(uv0, 0.0) + dir * mid;
      vec2 tex = sample_height_and_cone_ratio(relaxedcone_relief_map, q.xy);
      if (q.z < (1. - tex.x)) {
         lo = mid;
      } else {
         hi = mid;
      }
   }
   p = vec3(uv0, 0.0) + dir * hi;
   return p.xy;
}
// DONE:
//       https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-18-relaxed-cone-stepping-relief-mapping
//
vec2 ray_intersect_relaxedcone(sampler2D relaxedcone_relief_map, inout vec3 p, inout vec3 v) {
   const int cone_steps = 32;
   const int binary_steps = 16;
   const float depth_scale = 0.15;

   vec3 p0 = p;

   v = vec3(v.x, v.y, abs(v.z) / depth_scale); // NOTE: no negation if v.z already points inward
   v /= v.z;

   float dist = length(v.xy);

   // First loop for cone intersection
   for (int i = 0; i < cone_steps; i++) {
      vec2 tex = sample_height_and_cone_ratio(relaxedcone_relief_map, p.xy);
      float height = clamp(1. - tex.x - p.z, 0.0, 1.0);
      float cone_ratio = tex.y;
      p += v * (cone_ratio * height / (dist + cone_ratio));
   }

   v *= p.z * 0.5;
   p = p0 + v;

   // Second loop for binary search
   for (int i = 0; i < binary_steps; i++) {
      float height = 1. - sample_height_and_cone_ratio(relaxedcone_relief_map, p.xy).x;
      v *= 0.5;
      if (p.z < height) {
         p += v; // Move up
      } else {
         p -= v; // Move down
      }
   }
   return p.xy;
}



vec2 ParallaxMapping2(vec2 texCoords, vec3 viewDir, sampler2D depthMap, float minLayers, float maxLayers, float heightScale) {

   // number of depth layers
   // int num_layers = 3
   float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
   // float numLayers = 64;
   // calculate the size of each layer
   float layerDepth = 1.0 / numLayers;
   // depth of current layer
   float currentLayerDepth = 0.0;
   // the amount to shift the texture coordinates per layer (from vector P)
   vec2 P = viewDir.xy / viewDir.z * heightScale;
   vec2 deltaTexCoords = P / numLayers;

   // get initial values
   vec2 currentTexCoords = texCoords;
   float currentDepthMapValue = texture(depthMap, currentTexCoords).r;

   while (currentLayerDepth < currentDepthMapValue) {
      // shift texture coordinates along direction of P
      currentTexCoords -= deltaTexCoords;
      // get depthmap value at current texture coordinates
      currentDepthMapValue = texture(depthMap, currentTexCoords).r;
      // get depth of next layer
      currentLayerDepth += layerDepth;
   }

   // get texture coordinates before collision (reverse operations)
   vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

   // get depth after and before collision for linear interpolation
   float afterDepth = currentDepthMapValue - currentLayerDepth;
   float beforeDepth = texture(depthMap, prevTexCoords).r - currentLayerDepth + layerDepth;

   // interpolation of texture coordinates
   float weight = afterDepth / (afterDepth - beforeDepth);
   // vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
   vec2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);

   return finalTexCoords;
}


vec2 parallax_uv_original(vec2 uv, vec3 tangent_space_view_direction, sampler2D depth_map) {
   // Sensible defaults
   const float height_scale = -0.1; // How pronounced the parallax effect is
   const float height_bias  = -1.0;
   const int min_samples = 8;      // Minimum number of samples for performance
   const int max_samples = 64;     // Maximum samples for quality

   // Calculate number of samples based on view angle
   // More samples when looking straight down, fewer when at grazing angles
   float num_samples = mix(float(max_samples), float(min_samples), abs(dot(vec3(0.0, 0.0, 1.0), tangent_space_view_direction)));

   // Calculate the parallax offset vector
   vec2 p = tangent_space_view_direction.xy / tangent_space_view_direction.z * height_scale;

   // Calculate step size
   float layer_depth = 1.0 / num_samples;
   float current_layer_depth = 0.0;
   vec2 delta_tex_coords = p / num_samples;

   // Start values
   vec2 current_tex_coords = uv;
   float current_depth_map_value = texture(depth_map, current_tex_coords).r;

   // Step through depth layers
   while (current_layer_depth < current_depth_map_value) {
      current_tex_coords -= delta_tex_coords;
      current_depth_map_value = texture(depth_map, current_tex_coords).r;
      current_layer_depth += layer_depth;
   }

   // Binary search refinement for better accuracy
   vec2 prev_tex_coords = current_tex_coords + delta_tex_coords;
   float after_depth = current_depth_map_value - current_layer_depth;
   float before_depth = texture(depth_map, prev_tex_coords).r - current_layer_depth + layer_depth;

   // Interpolate between the two closest points
   float weight = after_depth / (after_depth - before_depth);
   vec2 final_tex_coords = prev_tex_coords * weight + current_tex_coords * (1.0 - weight);

   return final_tex_coords;
}

vec2 parallax_uv(vec2 uv, vec3 tangent_space_view_direction, sampler2D tex_depth) {
   vec3 view_dir = tangent_space_view_direction;
   float depth_scale = 0.11;
   int num_layers = 64;
   const int type = 4;
   if (type == 2) {
      // Parallax mapping
      float depth = texture(tex_depth, uv).r;
      vec2 p = view_dir.xy * (depth * depth_scale) / view_dir.z;
      return uv - p;
   } else {
      float layer_depth = 1.0 / num_layers;
      float cur_layer_depth = 0.0;
      vec2 delta_uv = view_dir.xy * depth_scale / (view_dir.z * num_layers);
      vec2 cur_uv = uv;

      float depth_from_tex = texture(tex_depth, cur_uv).r;

      for (int i = 0; i < num_layers; i++) {
         cur_layer_depth += layer_depth;
         cur_uv -= delta_uv;
         depth_from_tex = texture(tex_depth, cur_uv).r;
         if (depth_from_tex < cur_layer_depth) {
            break;
         }
      }

      if (type == 3) {
         // Steep parallax mapping
         return cur_uv;
      } else {
         // Parallax occlusion mapping
         vec2 prev_uv = cur_uv + delta_uv;
         float next = depth_from_tex - cur_layer_depth;
         float prev = texture(tex_depth, prev_uv).r - cur_layer_depth + layer_depth;
         float weight = next / (next - prev);
         return mix(cur_uv, prev_uv, weight);
      }
   }
}

vec2 parallax_uv2(vec2 uv, vec3 tangent_space_view_direction, sampler2D tex_depth) {
   vec3 view_dir = tangent_space_view_direction;
   float depth_scale = 0.9;
   int num_layers = 64;
   const int type = 4;
   if (type == 2) {
      // Parallax mapping
      float depth = texture(tex_depth, uv).r;
      vec2 p = view_dir.xy * (depth * depth_scale) / view_dir.z;
      return uv - p;
   } else {
      float layer_depth = 1.0 / num_layers;
      float cur_layer_depth = 0.0;
      vec2 delta_uv = view_dir.xy * depth_scale / (view_dir.z * num_layers);
      vec2 cur_uv = uv;

      float depth_from_tex = texture(tex_depth, cur_uv).r;

      for (int i = 0; i < num_layers; i++) {
         cur_layer_depth += layer_depth;
         cur_uv -= delta_uv;
         depth_from_tex = texture(tex_depth, cur_uv).r;
         if (depth_from_tex < cur_layer_depth) {
            break;
         }
      }

      if (type == 3) {
         // Steep parallax mapping
         return cur_uv;
      } else {
         // Parallax occlusion mapping
         vec2 prev_uv = cur_uv + delta_uv;
         float next = depth_from_tex - cur_layer_depth;
         float prev = texture(tex_depth, prev_uv).r - cur_layer_depth + layer_depth;
         float weight = next / (next - prev);
         return mix(cur_uv, prev_uv, weight);
      }
   }
}

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir, sampler2D depthMap, float minLayers, float maxLayers, float heightScale) {

   // number of depth layers
   // int num_layers = 3
   float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));
   // float numLayers = 64;
   // calculate the size of each layer
   float layerDepth = 1.0 / numLayers;
   // depth of current layer
   float currentLayerDepth = 0.0;
   // the amount to shift the texture coordinates per layer (from vector P)
   vec2 P = viewDir.xy / viewDir.z * heightScale;
   vec2 deltaTexCoords = P / numLayers;

   // get initial values
   vec2 currentTexCoords = texCoords;
   float currentDepthMapValue = texture(depthMap, currentTexCoords).r;

   while (currentLayerDepth < currentDepthMapValue) {
      // shift texture coordinates along direction of P
      currentTexCoords -= deltaTexCoords;
      // get depthmap value at current texture coordinates
      currentDepthMapValue = texture(depthMap, currentTexCoords).r;
      // get depth of next layer
      currentLayerDepth += layerDepth;
   }

   // get texture coordinates before collision (reverse operations)
   vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

   // get depth after and before collision for linear interpolation
   float afterDepth = currentDepthMapValue - currentLayerDepth;
   float beforeDepth = texture(depthMap, prevTexCoords).r - currentLayerDepth + layerDepth;

   // interpolation of texture coordinates
   float weight = afterDepth / (afterDepth - beforeDepth);
   // vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);
   vec2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);

   return finalTexCoords;
}




//   mat3 TBN (columns T, B, N)
//   vec3 view_dir_world (from fragment to camera, normalized)
//   vec2 uv (original uv)
//   sampler2D normal_map
//   float parallax_scale user tunable
vec2 parallax_offset_from_gradient(mat3 TBN, vec3 world_space_view_dir, vec3 tagent_space_normal, vec2 uv, float parallax_scale) {
   // 1) sample tangent-space normal
   vec3 m = tagent_space_normal;

   // 2) compute gradient in tangent space (h_u,h_v)
   float hu = -m.x / max(m.z, 1e-6);
   float hv = -m.y / max(m.z, 1e-6);

   // 3) view direction in tangent-space
   vec3 v_t = TBN * normalize(world_space_view_dir); // columns T,B,N: maps world->tangent
   // ensure v_t.z not near 0
   float vz = max(v_t.z, 1e-6);

   // 4) directional effective height along view
   float h_eff = hu * v_t.x + hv * v_t.y;

   // 5) UV offset (note: divide by vz for perspective projection effect)
   vec2 dv = vec2(v_t.x, v_t.y);
   vec2 offset_uv = parallax_scale * (h_eff / vz) * dv;
   return offset_uv;
}

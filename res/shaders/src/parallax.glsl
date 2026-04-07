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
   
   const float i = (1024/2/2/2/2/2);
   ivec2 st = ivec2(uv*i);
   float h = texelFetch(map, st, 5).r;
   // float h = texture(map, uv).r
   return h;
}


vec2 parallaxMapping2(
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
   float min_layers,
   float max_layers,
   float scale,
   float bias,
   int search_steps,
   vec3 to_light_ts,         // light direction in tangent space
   out vec3 out_pos_ts,      // intersection (u,v,depth)
   out float out_shadow      // shadow factor (0.0 lit, 1.0 shadow)
) {
   // ----------------------
   // Primary view-ray search
   // ----------------------
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

// DONE:
//       https://developer.nvidia.com/gpugems/gpugems3/part-iii-rendering/chapter-18-relaxed-cone-stepping-relief-mapping
//
vec2 ray_intersect_relaxedcone(sampler2D relaxedcone_relief_map, inout vec3 p, inout vec3 v) {
   const int cone_steps = 25;
   const int binary_steps = 10;

   vec3 p0 = p;

   v /= v.z;

   float dist = length(v.xy);

   // First loop for cone intersection
   for (int i = 0; i < cone_steps; i++) {
      vec4 tex = texture2D(relaxedcone_relief_map, p.xy);
      float height = clamp(tex.w - p.z, 0.0, 1.0);
      float cone_ratio = tex.z;
      p += v * (cone_ratio * height / (dist + cone_ratio));
   }

   v *= p.z * 0.5;
   p = p0 + v;

   // Second loop for binary search
   for (int i = 0; i < binary_steps; i++) {
      vec4 tex = texture2D(relaxedcone_relief_map, p.xy);
      v *= 0.5;
      if (p.z < tex.w) {
         p += v; // Move up
      } else {
         p -= v; // Move down
      }
   }
   return p.xy;
}


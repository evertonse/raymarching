#vs
vec3 eye_pos = (model_mat * vec4(position, 1.0)).xyz; // position is model vertex pos
vec3 eye_dir = world_position_of_model_vec3 - eye_pos; // world_position_of_model is the origin...
// construct bn matrix, multiply eye_dir with it for tangent space eye vector
mat3 tbn = mat3(tangent, bitangent, normal);
vec3 eye = eye_dir * tbn; 
// outputs for parallax mapping in fs
p_eye_dot = eye - eye_pos;
p_eye_pos = ceil(dot(p_eye_dot, p_eye_dot));

#fs
float layers = clamp(p_eye_dot, min_layers, max_layers); // control the number of parallax layers
float height = 1.0 / layers; // height of each layer
vec3 shift = vec3(0.1 * scale * height * p_eye_pos.xy / p_eye.pos.z, height); // scale is parallax scale (float)
vec3 position = vec3(uv - shift.xy * layers * bias, 1.0); // uv is the uv coordinate, bias is the parallax bias (float)

// steep parallax (forward step until ray is below the height map)
float H = texture(map, position.xy).r;
for (int i = 0; i < layers; i++) {
  position -= shift * step(H, position.z); // down
  H = texture(map, position.xy).r;
}

// relief mapping (binary search for intersection)
H = texture(map, position.xy).r;
for (int i = 0; i < SOME_CONSTANT; i++) {
  position -= shift * (step(H, position.z) - 0.5) * exp2(-i); // should precompute these
  H = texture(map, position.xy);
}

// position.xy contains your parallax coordinate
vec2 parallaxMapping(vec2 uv,            // base UV coordinates
                     vec3 viewDirTS,     // view direction in tangent space (pointing into surface)
                     sampler2D depthMap, // height map (R channel)
                     float minLayers,    // minimum number of steps
                     float maxLayers,    // maximum number of steps
                     float scale,        // parallax scale (depth scale)
                     float bias,         // parallax bias
                     int searchSteps     // refinement steps (binary search)
) {
   // Determine number of layers depending on view angle
   float numLayers = mix(maxLayers, minLayers, abs(viewDirTS.z));
   float layerDepth = 1.0 / numLayers;

   // Per-step UV shift (scaled by parallax depth scale)
   vec2 P = viewDirTS.xy / -viewDirTS.z * scale;
   vec2 deltaUV = P * layerDepth;

   // Initial position in parallax "volume"
   vec2 currUV = uv;
   float currDepth = 0.0;
   float sampledH = texture(depthMap, currUV).r;

   // --- Steep parallax mapping: march until ray goes under heightmap
   while (currDepth < sampledH && currDepth < 1.0) {
      currUV -= deltaUV;
      currDepth += layerDepth;
      sampledH = texture(depthMap, currUV).r;
   }

   // --- Relief mapping refinement: binary search around intersection
   vec2 prevUV = currUV + deltaUV;
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

const float pi       = 3.14159265359;
const float halfPi   = pi * 0.5;
const float twoPi    = 2.0 * pi;

const float sampleCount  = 16.0;
const float sampleRadius = 0.85;
const float sliceCount   = 8.0;    // each slice covers both sides → fewer needed
const float hitThickness = 0.5;
const uint  sectorCount  = 32u;

// Interleaved Gradient Noise — low discrepancy, good for temporal accumulation
float randf(int x, int y) {
   return mod(52.9829189 * mod(0.06711056*float(x) + 0.00583715*float(y), 1.0), 1.0);
}

uint bitCount(uint v) {
   v = v - ((v >> 1u) & 0x55555555u);
   v = (v & 0x33333333u) + ((v >> 2u) & 0x33333333u);
   return ((v + (v >> 4u) & 0xF0F0F0Fu) * 0x1010101u) >> 24u;
}

uint updateSectors(float minH, float maxH, uint bits) {
   uint start  = uint(minH * float(sectorCount));
   uint span   = uint(ceil((maxH - minH) * float(sectorCount)));
   uint mask   = span > 0u ? (0xFFFFFFFFu >> (sectorCount - span)) : 0u;
   return bits | (mask << start);
}

vec4 getVisibility(
   vec2      fragUV,
   ivec2     coordinate,
   sampler2D screenLight,
   sampler2D screenNormal,
   sampler2D screenPosition
) {
   vec2  screenSize = vec2(textureSize(screenLight, 0));
   float aspect     = screenSize.x / screenSize.y;

   vec3 position = texture(screenPosition, fragUV).rgb;
   vec3 normal   = normalize(texture(screenNormal, fragUV).rgb);

   // In positive-Z view space: camera at origin, looking down +Z
   // camera direction from fragment = normalize(-position)
   vec3 camera = normalize(-position);

   // Project world radius into UV space at this depth
   // focal = 1/tan(fov/2) = the projection scale factor
   // At depth z, a world-unit spans (focal/z) in NDC, * 0.5 in UV
   // Divide by screenSize.y to get UV units (height-normalized)
   float focal    = 1.0 / tan(fov * 0.5);
   float uvRadius = sampleRadius * focal / (position.z * screenSize.y);
   // uvRadius is the half-radius in UV height-normalized coordinates

   // Per-pixel jitter using IGN
   // Single value jitters the starting angle of all slices together
   // Temporal accumulation (changing frame index) rotates this over time
   float jitter = randf(coordinate.x, coordinate.y);   // ∈ [0, 1)

   float totalVisibility = 0.0;
   vec3  totalLighting   = vec3(0.0);

   // ── Outer integral: φ ∈ [0, π) ────────────────────────────────────
   // Each slice covers BOTH sides (+ω and −ω) making a full diameter
   // Outer integral: ∫₀^π AO2(φ) dφ ≈ (1/Nd) Σᵢ AO2(φᵢ)
   for (float slice = 0.0; slice < sliceCount; slice += 1.0) {

      // φ uniformly in [0, π), jitter shifts all by same random amount
      // This is correct: temporal jitter rotates the entire slice set
      float phi   = (slice + jitter) * pi / sliceCount;
      vec2  omega = vec2(cos(phi), sin(phi));   // unit screen-space direction

      // ── Projected normal setup (GTAO Algorithm 1, lines 9-15) ───────
      vec3 dir        = vec3(omega, 0.0);
      vec3 orthoDir   = dir - dot(dir, camera) * camera;
      vec3 axis       = cross(dir, camera);
      vec3 projNormal = normal - axis * dot(normal, axis);
      float projLen   = length(projNormal);

      // γ' = signed angle of projected normal from camera direction
      // Positive if projNormal tilts toward the positive slice direction
      float signN = sign(dot(orthoDir, projNormal));
      float cosN  = clamp(dot(projNormal, camera) / projLen, -1.0, 1.0);
      float n_ang = signN * acos(cosN);   // γ' ∈ [-π/2, π/2]

      // ── Bitmask reset per slice — Algorithm 1: "Bitmask bi ← 0" ────
      // CRITICAL: must be inside the outer loop, not outside
      uint occlusion  = 0u;
      vec3 sliceLight = vec3(0.0);

      // ── Inner integral: both sides of slice ─────────────────────────
      // Covers +ω direction (side=0) and −ω direction (side=1)
      // Both sides write into the SAME bitmask (occlusion)
      // This correctly prevents double-counting of occluders
      for (int side = 0; side < 2; side++) {
         float sideSign = (side == 0) ? 1.0 : -1.0;
         // sideSign flips θ so the two sides map to OPPOSITE bitmask halves:
         //   side=0 (+ω): θ > 0 → sectors above center (> 0.5)
         //   side=1 (-ω): θ < 0 → sectors below center (< 0.5)
         // Without sideSign, both sides would map to the same sectors → wrong AO

         for (float s = 0.0; s < sampleCount; s += 1.0) {
            // t ∈ (0, 1]: start at 0.5/N to avoid self-sampling at s=0
            float t = (s + 0.5) / sampleCount;

            // UV step with aspect correction:
            // omega.x / aspect corrects for non-square UV space
            // so the step is equal physical distance in both x and y
            vec2 uvStep   = vec2(omega.x / aspect, omega.y) * uvRadius;
            vec2 sampleUV = fragUV + sideSign * t * uvStep;

            // Read screen-space data at sample location
            vec3 samplePos    = texture(screenPosition, sampleUV).rgb;
            vec3 sampleNormal = normalize(texture(screenNormal, sampleUV).rgb);
            vec3 sampleLight  = texture(screenLight, sampleUV).rgb;

            vec3  delta = samplePos - position;   // p→sample vector
            float dist  = length(delta);
            if (dist < 1e-4) continue;
            vec3 sampleDir = delta / dist;         // unit direction

            // ── Front elevation angle ─────────────────────────────────
            // θf = angle of sample horizon from camera direction
            // Multiplied by sideSign so the two sides map to separate sectors
            float cosTheta_f = dot(sampleDir, camera);
            float theta_f    = sideSign * acos(clamp(cosTheta_f, -1.0, 1.0));

            // ── Back elevation angle (thickness heuristic) ────────────
            // Paper line 15: sb = sf - (p/||p||) * t
            // p/||p|| = normalize(position) = -camera
            // So sb = sf + camera * hitThickness
            // Direction from p to sb: delta + camera * hitThickness
            // Fix: original had (delta - camera * t), sign was inverted
            float cosTheta_b = dot(normalize(delta + camera * hitThickness), camera);
            float theta_b    = sideSign * acos(clamp(cosTheta_b, -1.0, 1.0));

            // ── Map angles to bitmask sectors [0, 1] ─────────────────
            //
            // The bitmask hemisphere spans [n_ang - π/2, n_ang + π/2]
            // Mapping:
            //   sector = (θ - n_ang + π/2) / π
            //
            // At θ = n_ang:        sector = 0.5  (center = normal direction)
            // At θ = n_ang + π/2:  sector = 1.0  (positive edge)
            // At θ = n_ang - π/2:  sector = 0.0  (negative edge)
            //
            // Fix: original had (θ + n_ang + π/2)/π  ← wrong sign on n_ang
            float h_min = clamp((min(theta_f,theta_b) - n_ang + halfPi) / pi, 0.0, 1.0);
            float h_max = clamp((max(theta_f,theta_b) - n_ang + halfPi) / pi, 0.0, 1.0);

            // Sectors this sample covers
            uint bj      = updateSectors(h_min, h_max, 0u);
            // New sectors not yet blocked (Algorithm 1: bj & ~bi)
            uint newBits = bj & ~occlusion;

            // ── GI accumulation (Algorithm 1, line 23) ────────────────
            //
            // newSectorFrac = fraction of hemisphere newly lit by this sample
            // sampleLight   = Lo(q) = outgoing radiance at sample q
            // n·l           = receiver Lambert at p
            // nl·(-l)       = emitter Lambert at q (facing toward p)
            //
            // Rendering equation term: Lo(q) * (np·ωi) * (nq·-ωi) * dωi
            //                          ──────  ────────  ──────────  ───
            //                          Li      receiver  emitter     solid angle
            //                                  Lambert   Lambert     (bitmask)
            //
            // Fix: original had (1.0 - bitCount/N) which DECREASES with more hits
            //      should be just bitCount/N which INCREASES (more light = more GI)
            float newFrac  = float(bitCount(newBits)) / float(sectorCount);
            float n_dot_l  = clamp(dot(normal, sampleDir), 0.0, 1.0);
            float nl_dot_l = clamp(dot(sampleNormal, -sampleDir), 0.0, 1.0);
            sliceLight += newFrac * sampleLight * n_dot_l * nl_dot_l;

            // Accumulate bitmask: bi |= bj (Algorithm 1, line 24)
            occlusion |= bj;
         }
      }

      // AO for this slice: unoccluded fraction (Algorithm 1, line 26)
      // 1 - bitCount(bi)/Nb = fraction of hemisphere NOT blocked
      totalVisibility += 1.0 - float(bitCount(occlusion)) / float(sectorCount);
      totalLighting   += sliceLight;
   }

   // Average over slices (Algorithm 1, line 28: return AO/Nd, GI/Nd)
   totalVisibility /= sliceCount;
   totalLighting   /= sliceCount;

   // xyz = indirect lighting (GI bounce)
   // w   = visibility (1=open, 0=fully occluded) → use as AO: ambient *= result.w
   return vec4(totalLighting, totalVisibility);
}

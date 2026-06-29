

#include "src/deps/lygia/generative/random.glsl"
#include "res/shaders/src/perspective.glsl"

#if defined(SSAO_SAMPLES)
#undef SSAO_SAMPLES
#endif

#define SSAO_SAMPLES 128 //16
#define SSAO_RADIUS  10.25
#define SSAO_BIAS    0.015 // 0.025
#define SSAO_MARCH_STEPS 8


// Noise from pixel coordinate — replaces the 4x4 noise texture
// Returns a random tangent-space rotation vector (z=0, rotates around normal)
vec3 tangent_space_vector(ivec2 pixel) {
   return vec3(
      random(pixel.x)        * 2.0 - 1.0,   // x in [-1,1]
      random(pixel.y + 17.0) * 2.0 - 1.0,   // y in [-1,1]
      0.0                                 // z = 0, rotate around normal only
   );
}

// Hemisphere sample in tangent space at index i out of total count
// xy in [-1,1], z in [0,1] (positive hemisphere), magnitude in [0.1,1]
// More samples concentrated near origin. close occlusion weighted more
vec3 hemisphere_sample2(int i, int total) {
   float fi = float(i);

   // Raw random direction in hemisphere (z positive = above surface)
   vec3 s = vec3(
      random(fi)        * 2.0 - 1.0,   // x ∈ [-1,1]
      random(fi + 33.0) * 2.0 - 1.0,   // y ∈ [-1,1]
      random(fi + 67.0)                  // z ∈ [0,1]
   );
   s = normalize(s);

   // Accelerating scale: t^2 pushes more samples near origin
   // lerp(0.1, 1.0, t^2) -> scale in [0.1, 1.0]
   float t     = fi / float(total);
   float scale = 0.1 + 0.9 * t * t;
   return s * scale;
}
// Returns a cosine-weighted hemisphere sample in tangent space at kernel index i.
//
// Uniform sphere distribution via rejection sampling:
//   Generate candidate in [-1,1]^3 cube.
//   Reject if length > 1 (would come from a corner, over-represented).
//   This gives uniform distribution over the unit ball.
//   Then set z = abs(z) to project onto the positive hemisphere.
//
// After rejection+normalize the direction is uniformly distributed on
// the hemisphere. We then scale by an accelerating function of i so that
// more samples land close to the fragment (close occlusion matters more):
//
//   scale(t) = lerp(0.1, 1.0, t²)    t = i / (total-1) ∈ [0,1]
//
//   t²  pushes the distribution toward t=0 (close to origin).
//   At i=0:       scale = 0.1  → sample very close to fragment
//   At i=total-1: scale = 1.0  → sample at full SSAO_RADIUS
//
// The result is a point in tangent space. TBN in the caller rotates it
// to align with the actual surface normal.
vec3 hemisphere_sample(int i, int total) {
    float fi = float(i);

    // Four independent random values per sample:
    //   seed_x, seed_y, seed_z → direction inside unit hemisphere
    //   seed_w                 → independent magnitude draw (the missing piece)
    //
    // Large prime offsets keep the four values uncorrelated with each other.
    float seed_x = fi * 7.0;
    float seed_y = fi * 7.0 + 127.1;
    float seed_z = fi * 7.0 + 269.5;
    float seed_w = fi * 7.0 + 419.2;   // independent magnitude

    // Rejection sample for uniform sphere distribution.
    // Acceptance probability ≈ π/6 ≈ 52% per attempt.
    // After 6 attempts, P(no valid sample) ≈ 0.48^6 ≈ 1.2% — acceptable.
    vec3 s = vec3(0.0, 0.0, 1.0);  // fallback: straight up
    for (int attempt = 0; attempt < 6; attempt++) {
        float jitter = float(attempt) * 91.0;
        vec3 candidate = vec3(
            random(seed_x + jitter) * 2.0 - 1.0,   // x ∈ [-1, 1]
            random(seed_y + jitter) * 2.0 - 1.0,   // y ∈ [-1, 1]
            random(seed_z + jitter)                  // z ∈ [ 0, 1] — upper hemisphere
        );
        if (dot(candidate, candidate) <= 1.0) {
            s = candidate;
            break;
        }
    }

    // Unit direction, uniformly distributed on the hemisphere.
    s = normalize(s);

    // ── Two-stage magnitude, matching the CPU version exactly ────────────────
    //
    // CPU does:
    //   sample  = normalize(sample)       → unit direction
    //   sample *= randomFloats(generator) → random magnitude in [0,1]
    //   scale   = lerp(0.1, 1.0, t²)     → accelerating scale by index
    //   sample *= scale                   → final magnitude = rand * scale
    //
    // The random magnitude draw before the scale is what gives continuous
    // distance variation within each "shell" at index i.
    // Without it, all samples at index i have the same distance from origin
    // (just different directions), creating discrete shells in the kernel.
    //
    // rand_magnitude ∈ [0, 1] — independent from direction seeds
    float rand_magnitude = random(seed_w);

    // Accelerating scale: t² pushes distribution toward origin.
    //   i=0:       t=0.0 → scale=0.10  (closest to fragment)
    //   i=total/2: t=0.5 → scale=0.33
    //   i=total-1: t=1.0 → scale=1.0  (at full SSAO_RADIUS)
    float t     = fi / float(total - 1);
    float scale = mix(0.1, 1.0, t * t);

    // Final magnitude = random continuous variation * index-based scale.
    // This matches: sample *= randomFloats(); sample *= scale;
    return s * rand_magnitude * scale;
}
vec3 hemisphere_sample(int i, int total, ivec2 pixel, vec3 position) {
    float fi = float(i);

    // ── Per-pixel seed perturbation ───────────────────────────────────────────
    //
    // Without this, every pixel uses the same kernel — only the TBN rotation
    // varies per pixel. Two pixels with similar normals get nearly identical
    // sample clouds, just slightly rotated, causing structured noise patterns.
    //
    // We perturb the seed with pixel + position so each pixel gets a genuinely
    // different kernel, not just a rotated copy of the same kernel.
    //
    // pixel    → stable screen-space variation, doesn't shimmer with camera
    // position → surface-space variation, different per object/surface
    // position * 0.1 damps high-frequency camera-space flicker
    //
    // The result: each (pixel, sample_index) pair has a unique seed.
    float pixel_noise = float(pixel.x) * 1973.0 + float(pixel.y) * 9277.0;
    float pos_noise   = position.x * 127.1 + position.y * 311.7 + position.z * 74.9;
    float base        = fi * 7.0 + pixel_noise * 0.01 + pos_noise * 0.1;

    // Four independent seeds — large prime offsets keep them uncorrelated
    float seed_x = base;
    float seed_y = base + 127.1;
    float seed_z = base + 269.5;
    float seed_w = base + 419.2;   // independent magnitude draw

    // ── Uniform hemisphere via rejection sampling ─────────────────────────────
    //
    // Sample full sphere (z ∈ [-1,1]) for rejection so the acceptance boundary
    // is a symmetric full sphere — equal probability for every direction.
    // Then abs(z) folds the lower hemisphere up onto the upper hemisphere,
    // which preserves uniformity (reflection is a bijection on directions).
    //
    // If we sampled z ∈ [0,1] directly, the asymmetric cube [-1,1]x[-1,1]x[0,1]
    // would give higher acceptance near the equator than the pole, breaking
    // the uniform distribution before normalization.
    //
    // Tangent space orientation:
    //   z = normal axis (must be ≥ 0, points away from surface)
    //   x = tangent direction
    //   y = bitangent direction
    vec3 s = vec3(0.0, 0.0, 1.0);  // fallback: straight up along normal
    for (int attempt = 0; attempt < 6; attempt++) {
        float jitter = float(attempt) * 91.0;
        vec3 candidate = vec3(
            random(seed_x + jitter) * 2.0 - 1.0,   // x ∈ [-1, 1]
            random(seed_y + jitter) * 2.0 - 1.0,   // y ∈ [-1, 1]
            random(seed_z + jitter) * 2.0 - 1.0    // z ∈ [-1, 1] full sphere
        );
        if (dot(candidate, candidate) <= 1.0) {
            s = candidate;
            break;
        }
    }

    // Fold into upper hemisphere — z is the normal axis in tangent space
    s = normalize(vec3(s.x, s.y, abs(s.z)));

    // ── Two-stage magnitude ───────────────────────────────────────────────────
    //
    // Stage 1: independent random magnitude in [0,1] per sample
    //          gives continuous distance variation within each index level
    //          without this all samples at index i have the same distance,
    //          creating discrete shells in the kernel
    //
    // Stage 2: accelerating scale by index — t² squeezes toward origin
    //          so more samples land close to the fragment where
    //          occlusion has the most influence on the result
    //
    //   final magnitude = rand_magnitude * scale(i)
    //
    //   i=0:         scale=0.10, rand in [0,   0.10]
    //   i=total/2:   scale=0.33, rand in [0,   0.33]
    //   i=total-1:   scale=1.00, rand in [0,   1.00]
    float rand_magnitude = random(seed_w);

    float t     = fi / float(total - 1);
    float scale = mix(0.1, 1.0, t * t);

    return s * rand_magnitude * scale;
}

// https://www.karlsims.com/random-in-sphere.html#:~:text=Random%20in%20cube%2C%20then%20warp,the%20center%20of%20the%20sphere.
vec3 hemisphere_sample3(int i, int total) {
   float fi = float(i);

   // ── Decorrelated seeds ──────────────────────────────────────────────
   // Large primes prevent correlation between direction and magnitude draws.
   // Multiplying fi by a prime avoids degenerate output at fi=0.
   float seed_u1 = fi * 1973.0 + 9277.0;
   float seed_u2 = fi * 4421.0 + 2341.0;
   float seed_r  = fi * 6271.0 + 5867.0;

   float u1 = random(seed_u1);   // controls elevation (cos theta)
   float u2 = random(seed_u2);   // controls azimuth (phi)

   // ── Cosine-weighted hemisphere via Malley's method ──────────────────
   //
   // Sample a disk of radius sqrt(u1), then lift to hemisphere.
   // The projection compresses elevation proportional to cos(theta):
   //
   //   cos_theta = sqrt(1 - u1)
   //   → when u1→0: cos_theta→1, sample near z=1 (normal direction)  ← dense
   //   → when u1→1: cos_theta→0, sample near horizon                 ← sparse
   //
   // PDF = cos(theta)/π  → importance samples the cos(theta) in the integrand
   // No rejection loop needed — always produces a valid sample
   //
   float cos_theta = sqrt(1.0 - u1);        // z component, biased toward 1
   float sin_theta = sqrt(u1);              // lateral spread, = sqrt(1 - cos²θ)
   float phi       = 2.0 * PI * u2;        // full azimuth rotation

   vec3 direction = vec3(
      sin_theta * cos(phi),   // x ∈ [-1,1], spreads on surface plane
      sin_theta * sin(phi),   // y ∈ [-1,1], spreads on surface plane
      cos_theta               // z ∈  [0,1], concentrated toward normal
   );
   // direction is already a unit vector — no normalize needed
   // dot(direction, vec3(0,0,1)) = cos_theta ∈ [0,1] → always above surface

   // ── Accelerating magnitude scale ────────────────────────────────────
   //
   // Distributes sample distances: more near origin, fewer at full radius.
   // t² gives quadratic falloff — most samples cluster at small distances.
   //
   // i=0:            t=0.0  → scale=0.10  (tight, near fragment)
   // i=total/2:      t=0.5  → scale=0.33
   // i=total-1:      t≈1.0  → scale=1.00  (at SSAO_RADIUS)
   float t         = fi / float(max(total - 1, 1));
   float scale     = mix(0.1, 1.0, t * t);

   // Independent magnitude draw within each shell
   // Prevents all samples at index i having identical distance
   float magnitude = random(seed_r);

   return direction * magnitude * scale;
   // Final magnitude ∈ [0, scale]
   // Combined with cosine-weighted direction:
   //   most samples → near origin AND near normal axis
   //   few samples  → near full radius at grazing angles
}


// All these are textures are assumed to be in View Space.
// 'pixel' is the pixel coornidates.
vec4 ssao_hemisphere_normal_aligned(
   vec2      uv,
   ivec2     pixel,
   sampler2D color_texture,
   sampler2D normal_texture,
   sampler2D position_texture
) {
   // NOTE: Assuming this is the same aspect ratio in forward render
   vec2 size = vec2(textureSize(position_texture, 0));
   float aspect = size.x / size.y; // Always width/height

   // Read position and normal from buffers (both in view space)
   // position.z is POSITIVE in view space. We look into +Z.
   vec3 position = texture(position_texture, uv).xyz;
   vec3 normal = normalize(texture(normal_texture, uv).rgb);

   // Build TBN from noise (some people use noise texture for this)
   // random_vector is a tangent-space rotation direction (z=0)
   // different per pixel so each pixel rotates its hemisphere differently
   // helps breaks up banding
   // vec3 random_vector = tangent_space_vector(pixel);
   //  noise
   vec3 random_vector = random3(position + normal);

   // Gram-Schmidt make tangent perpendicular to normal
   // t = normalize(r - dot(r, n) n)  removes normal component from random_vector
   // TODO: Maybe we should actually use the actual surface tagent from the prepass
   // What if we pass the actual tanget using either mikk space or dxdy from geomtry shader?
   vec3 tangent   = normalize(random_vector - normal * dot(random_vector, normal));
   vec3 bitangent = cross(normal, tangent);
   mat3 TBN       = mat3(tangent, bitangent, normal);
   // TBN * vec3(0,0,1) = n      normal maps to itself
   // TBN * sample_i    = sample rotated to align with actual surface normal

   // Accumulate occlusion over hemisphere samples
   float occlusion = 0.0;
   for (int i = 0; i < SSAO_SAMPLES; i++) {

      //
      // Generate sample in tangent space
      // then transform to view space using TBN
      // then into clip space using the used perspective operation
      // then into ndc by doing a perspective divide
      // then into uv by remaps [-1, 1] into [0, 1]
      //

      //
      // hemisphere_sample returns a point in tangent space:
      //   z  in [0, 1]  -> always above surface (positive hemisphere)
      //   xy in [-1,1]  -> spread across surface plane
      //   magnitude  -> more near origin (close occlusion matters more)
      //

      // TBN rotates tangent-space sample to align with surface normal n
      // * SSAO_RADIUS scales to world-space units
      vec3 sample_position = position + TBN * hemisphere_sample(i, SSAO_SAMPLES) * SSAO_RADIUS;
      float sample_depth = sample_position.z;

      //
      // Project test point to screen uv.
      // We need the screen uv where sample_position would appear so we can look up what geometry actually exists there
      // sample_position is a 3D test point in view space, now is there geometry here?
      //

      vec4 clip = perspective_from_fov(sample_position, fov, aspect, near_plane, far_plane);

      // Perspective divide. clip -> ndc [-1,1]
      vec2 ndc  = clip.xy / clip.w;

      // ndc -> uv [0,1]
      vec2 sample_uv = ndc * 0.5 + 0.5;

      // Read actual view-space position at that screen location
      // real_position.z is the actual view-space depth of real geometry there.
      vec3  real_position = texture(position_texture, sample_uv).xyz;

      // closer to 0   -> nearer to camera
      // more positive -> farther from camera
      float real_depth = real_position.z;

      // Range check
      //
      // Without this distant floors or walls at sample_uv would incorrectly occlude p even though it's far away.
      //
      // smoothstep gives 1.0 when surfaces are within radius,
      //                   0.0 when surfaces are far outside radius
      //                   smooth falloff between
      float range_check = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(position.z - real_depth));

      //
      // Visibility test V(p, wi)
      // If real_depth > sample_depth it means sample_depth is actually closer
      // That doesn't mean that the ray from position to sample_position never crossed the geometry,
      // but at least there's no other point in front of the sample_position blocking the camera's visibility of it.
      //
      // If sample_depth > real_depth then for sure there's geometry in front of it.
      // We could ray march actually, going in small steps from position to sample_position making sure we never got occluded by a depth value.
      // Each step would be a percetange from position to sample_position.
      //

      // In case -Z is camera forward use occlusion += (real_depth >= sample_position.z + SSAO_BIAS ? 1.0 : 0.0) * range_check;
      occlusion += (real_depth <= sample_depth - SSAO_BIAS ? 1.0 : 0.0) * range_check;
   }

   // 0.0 = no samples occluded.  fully open hemisphere receives full ambient
   // 1.0 = all samples occluded. fully enclosed receives no ambient

   // Invert so result plugs directly into: ambient *= ao
   float ao = 1.0 - (occlusion / float(SSAO_SAMPLES));

   return vec4(ao, ao, ao, 1.0);
}

// For comments look ssao_hemisphere_normal_aligned implementation.
// Eventually we would merge into a single func where if steps = 1
// then there's not diference from no raymarching.
vec4 ssao_hemisphere_normal_ray_marched(
   vec2      uv,
   ivec2     pixel,
   sampler2D color_texture,
   sampler2D normal_texture,
   sampler2D position_texture
) {
   vec2 size = vec2(textureSize(position_texture, 0));
   float aspect = size.x / size.y; // Always width/height

   vec3 position = texture(position_texture, uv).xyz;
   vec3 normal = normalize(texture(normal_texture, uv).rgb);

   vec3 random_vector = random3(random3(position) + random3(normal));
   vec3 tangent   = normalize(random_vector - normal * dot(random_vector, normal));
   vec3 bitangent = cross(normal, tangent);
   mat3 TBN       = mat3(tangent, bitangent, normal);

   float occlusion = 0.0;
   vec3 indirect_light = vec3(0.0);  // accumulate color
   for (int i = 0; i < SSAO_SAMPLES; i++) {
      vec3 sample_position = position + TBN * hemisphere_sample2(i, SSAO_SAMPLES) * SSAO_RADIUS;
      // vec3 sample_position = position + TBN * hemisphere_sample(i, SSAO_SAMPLES) * SSAO_RADIUS;
      // vec3 sample_position = position + TBN * hemisphere_sample(i, SSAO_SAMPLES, pixel, position) * SSAO_RADIUS;
      float sample_depth = sample_position.z;



      for (int step = 1; step <= SSAO_MARCH_STEPS; step++) {
         // t in (0, 1] start slightly away from surface to avoid self-occlusion
         float t = float(step) / float(SSAO_MARCH_STEPS);

         // Current point along the ray in view space
         vec3 march_position = lerp(position, sample_position, t);
         float march_depth = march_position.z;

         vec4 clip = perspective_from_fov(march_position, fov, aspect, near_plane, far_plane);
         vec2 ndc  = clip.xy / clip.w;
         vec2 march_uv = ndc * 0.5 + 0.5;
         vec3  real_position = texture(position_texture, march_uv).xyz;
         float real_depth = real_position.z;

         // If 't' is small then the occlusion it very close to original point
         // So it should be perceived as darker. Similarly, if we're occluded on the last step it should be lighter.
         // But it see
         const float ray_proximity_weight = smoothstep(0.0, 1.0, 1. - t);
         const float proximity_weight = ray_proximity_weight * smoothstep(0.0, 1.0, SSAO_RADIUS / abs(position.z - real_depth));

         bool is_occluded = real_depth <= march_depth - SSAO_BIAS;
         if (is_occluded) {
            { // SSAO
               occlusion += proximity_weight;
            }

            { // SSDO
               const vec3 real_normal   = normalize(texture(normal_texture, march_uv).xyz);
               vec3 real_direction = (real_position - position);
               const float d = max(length(real_direction), 1.0);
               real_direction  /= d;
               // real_direction  = normalize(real_direction);

               // How much does the occluder face US (emit toward us)?
               float emitter_facing = max(dot(real_normal, -real_direction), 0.0);
               // How much does OUR surface face the occluder (receive from it)?
               float receiver_facing = max(dot(normal, real_direction), 0.0);


               // Sender area: each sample represents πr²/N of the hemisphere base
               // r here is SSAO_RADIUS, r_max in the paper
               // This directly controls indirect brightness:
               //   smaller radius, less bleeding
               //   larger radius, more bleeding
               // The paper says to use this as an artistic control parameter.
               const float patch_area = PI * SSAO_RADIUS * SSAO_RADIUS / float(SSAO_SAMPLES);
               const float geometry_term = (emitter_facing * receiver_facing) * (patch_area  / (d * d));

               vec3 real_color = texture(color_texture, march_uv).rgb;

               // Normal dissimilarity weight, reduces coplanar bleeding, emphasizes corner bleeding
               // surface parallel to you bounces light sideways.
               // A perpendicular surface (like a wall next to a floor) bounces light directly at you.
               float normal_dissimilarity = 1.0 - max(dot(normal, real_normal), 0.0);

               // geomtry_term already handles distance falloff but we can add another proximity_weight as artistic shit
               float extra_distance_dampening = 1.;
               if (false) {
                  extra_distance_dampening = proximity_weight * normal_dissimilarity;
               }
               // Lo(q) light leaving occluder
               indirect_light += real_color * geometry_term * extra_distance_dampening;

               // indirect_light += real_color * emitter_facing * receiver_facing * proximity_weight;
            }
            break;
         }
      }

   }

   // 0.0 = no samples occluded.  fully open hemisphere receives full ambient
   // 1.0 = all samples occluded. fully enclosed receives no ambient

   // Invert so result plugs directly into: ambient *= ao
   float ao = 1.0 - (occlusion / float(SSAO_SAMPLES));

   return vec4(indirect_light, ao);
}


vec4 ssao_hemisphere_normal_ray_marched2(vec2 uv, ivec2 pixel, sampler2D color_texture, sampler2D normal_texture, sampler2D position_texture) {
   vec2 size = vec2(textureSize(position_texture, 0));
   float aspect = size.x / size.y;

   vec3 position = texture(position_texture, uv).xyz;
   vec3 normal = normalize(texture(normal_texture, uv).rgb);

   vec3 random_vector = random3(position * 0.1 + normal + vec3(float(pixel.x), float(pixel.y), 0.0));

   vec3 tangent = normalize(random_vector - normal * dot(random_vector, normal));
   vec3 bitangent = cross(normal, tangent);
   mat3 TBN = mat3(tangent, bitangent, normal);

   float occlusion = 0.0;
   vec3 indirect = vec3(0.0);
   float indirect_sum = 0.0; // sum of weights for normalisation

   for (int i = 0; i < SSAO_SAMPLES; i++) {

      vec3 sample_end = position + TBN * hemisphere_sample(i, SSAO_SAMPLES, pixel, position) * SSAO_RADIUS;

      // Direction from fragment to sample endpoint — used for Lambert weighting.
      // This is the direction ωᵢ of incoming indirect light if the ray is unoccluded.
      // dot(normal, omega_i) is the cosine term of the rendering equation:
      //   L_indirect = Σ V(p,ωᵢ) · L(ωᵢ) · max(dot(n, ωᵢ), 0) · dω
      vec3 omega_i = normalize(sample_end - position);
      float n_dot_l = max(dot(normal, omega_i), 0.0);

      for (int step = 1; step <= SSAO_MARCH_STEPS; step++) {

         float t = float(step) / float(SSAO_MARCH_STEPS);
         vec3 march_pos = mix(position, sample_end, t);
         float march_depth = march_pos.z; // fixed: test current step not endpoint

         vec4 clip = perspective_from_fov(march_pos, fov, aspect, near_plane, far_plane);
         vec2 ndc = clip.xy / clip.w;
         vec2 march_uv = ndc * 0.5 + 0.5;

         if (any(lessThan(march_uv, vec2(0.0))) || any(greaterThan(march_uv, vec2(1.0)))) {
            continue;
         }

         float real_depth = texture(position_texture, march_uv).z;

         // range check: rejects distant geometry from occluding nearby fragments
         // smoothstep → 1.0 when real geometry is within SSAO_RADIUS of fragment
         //            → 0.0 when real geometry is far outside SSAO_RADIUS
         float range_check = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(position.z - real_depth));

         // proximity weight: closer hit = stronger occlusion
         // quadratic falloff matches solid angle falloff ∝ 1/r²
         //   t=0.00 → weight=1.00  (occluder right next to fragment)
         //   t=0.50 → weight=0.25
         //   t=1.00 → weight=0.00  (occluder at full radius, minimal contribution)
         float ray_proximity_weight = (1.0 - t) * (1.0 - t);

         if (real_depth <= march_depth - SSAO_BIAS && range_check > 0.0) {

            // ── Occlusion ─────────────────────────────────────────────────
            occlusion += ray_proximity_weight * range_check;

            // ── Indirect diffuse ──────────────────────────────────────────
            //
            // The ray hit real geometry at march_uv. That geometry is a
            // light emitter (it has direct lighting stored in color_texture).
            // We gather its colour as indirect light arriving at our fragment.
            //
            // Rendering equation for one sample:
            //   L_ind += L(march_uv) · max(dot(n_p, ωᵢ), 0) · weight
            //
            // L(march_uv)          = direct lighting at the hit surface
            // max(dot(n_p, ωᵢ), 0) = Lambert cosine at receiver —
            //                        how much light from ωᵢ contributes to
            //                        irradiance at p (Lambert's law)
            // ray_proximity_weight  = same distance falloff as AO — nearby
            //                        surfaces contribute more indirect light
            // range_check           = same validity gate as AO
            //
            // We also read the hit surface normal to compute the emitter
            // cosine n_hit · (−ωᵢ): how much the hit surface radiates toward p.
            // This is the full diffuse-to-diffuse transport kernel:
            //   f(p→q) = (n_p · ω) · (n_q · −ω) / r²
            vec3 hit_color = texture(color_texture, march_uv).rgb;
            vec3 hit_normal = normalize(texture(normal_texture, march_uv).rgb);

            // emitter cosine: hit surface radiates toward fragment
            float n_hit_dot = max(dot(hit_normal, -omega_i), 0.0);

            float weight = ray_proximity_weight * range_check * n_dot_l * n_hit_dot;

            indirect += hit_color * weight;
            indirect_sum += weight;

            break;
         }
      }
   }

   float ao = 1.0 - clamp(occlusion / float(SSAO_SAMPLES), 0.0, 1.0);

   // Normalise indirect by total weight sum to get average radiance.
   // If no hits at all, indirect stays black.
   vec3 gi = indirect_sum > 0.001 ? indirect / indirect_sum : vec3(0.0);

   // Scale GI by AO complement: fully occluded = no indirect light reaches fragment.
   // Physically: if the hemisphere is blocked, neither ambient nor indirect arrives.
   gi *= ao;

   // rgb = indirect diffuse irradiance, a = ambient occlusion factor
   return vec4(gi, ao);
}

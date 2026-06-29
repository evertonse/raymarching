vec3 brdf_blinn_phong_old(
      in vec3 light_direction,     in vec3 view_direction, in vec3 normal,
      in vec3 diffuse_color,       in vec3 specular_color,
      in vec3 light_diffuse_color, in vec3 light_specular_color, in vec3 light_ambient_color,
      float specular_exponent,     in float ambient_occlusion_factor
) {

   vec3 wi = normalize(light_direction);
   vec3 wo = normalize(view_direction);
   vec3 n  = normalize(normal);

   // vec3 ambient_color = diffuse_color * specular_color;
   vec3 ambient_color =  0.55160 * diffuse_color  + 0.082671 * specular_color;
   // vec3 ambient_color = vec3(0.212671*diffuse_color.r, 0.715160*diffuse_color.g, 0.072169*diffuse_color.b);


   // Table of materials and constants for ambient: http://devernay.free.fr/cours/opengl/materials.html
   float ambient_intesity  = 0.12 * (0.212671*ambient_color.r + 0.715160*ambient_color.g + 0.072169*ambient_color.b)/(0.1 + (0.212671*diffuse_color.r + 0.715160*diffuse_color.r + 0.072169*diffuse_color.r));
   float diffuse_intesity  = 0.45;
   float specular_intesity = 0.25;

   const bool use_half_vector = true;
   float specular_term = 0;
   if (use_half_vector) {
      vec3 h = normalize(wo + wi);
      specular_term = dot(n, h);
   } else {
      vec3 r = -reflect(wi, normal);
      specular_term = dot(r, wo);
   }

   if (gl_FragCoord.x > 600) {
      ambient_occlusion_factor = 1.0;
   }


   vec3 specular = light_specular_color     * specular_color      * pow(max(0, specular_term), specular_exponent);
   vec3 diffuse  = ambient_occlusion_factor *light_diffuse_color      * diffuse_color       * max(0.0, dot(wi, n));
   vec3 ambient  = ambient_occlusion_factor * light_ambient_color * ambient_color;
   // vec3 specular = vec3(0.8) * specular_color.x * pow(max(0, specular_term), specular_exponent);

   return  vec3(0.)
         + (diffuse_intesity  * diffuse)
         + (specular_intesity * specular)
         + (ambient_intesity  * ambient)
   ;
}

// Blinn-Phong BRDF with physically motivated energy balance.
// https://www.farbrausch.de/~fg/stuff/phong.pdf
// Energy conservation:
//   The diffuse and specular weights must satisfy (onus on caller)
//     diffuse_color + specular_color <= vec3(1.0)
//

vec3 brdf_blinn_phong(
   in vec3  light_direction,
   in vec3  view_direction,
   in vec3  normal,
   in vec3  diffuse_color,
   in vec3  specular_color,
   in vec3  light_color,
   in float specular_exponent,
   in float ambient_occlusion_factor,  // 0=fully occluded, 1=fully open
   in vec3  indirect_irradiance,        // irradiance from SSDO, no albedo applied
   in bool  direct_only
) {
   vec3 wi = normalize(light_direction);
   vec3 wo = normalize(view_direction);
   vec3 n = normalize(normal);
   vec3 h = normalize(wo + wi); // Blinn half-vector

   // Cosine terms
   float n_dot_l = max(dot(n, wi), 0.0); // Lambert term
   float n_dot_h = max(dot(n, h), 0.0);  // Blinn specular term

   const float se = specular_exponent;
   float normalization = ((se + 2.0) * (se + 4.0)) / (8.0 * PI * (pow(2.0, -se * 0.5) + se));
   float specular_term = normalization * pow(n_dot_h, specular_exponent);
   if (mod(per_frame.elapsed_time, 2.) > 1) {
      if (gl_FragCoord.x > 580) {
         ambient_occlusion_factor = 1.0;
         indirect_irradiance = vec3(0);
      }
   } else{
      if (gl_FragCoord.x < 580) {
         ambient_occlusion_factor = 1.0;
         indirect_irradiance = vec3(0);
      }
   }

   // Diffuse
   // Lambert diffuse = (diffuse_color / PI) * L_i * cos(theta)
   vec3 diffuse = (diffuse_color / PI) * light_color * n_dot_l;

   // Specular
   vec3 specular = specular_color * light_color * specular_term * n_dot_l;

   // Ambient approximates unshadowed uniform irradiance from all directions.
   vec3 ambient = 0.21 * diffuse_color * light_color * ambient_occlusion_factor;

   // Indirect diffuse (one bounce from SSDO)
   // indirect_irradiance = weighted sum of Lo(q) * cos(theta_s) * cos(theta_r) * As/(d^2)
   // We also apply (albedo/PI).
   vec3 indirect_diffuse = (diffuse_color / PI) * indirect_irradiance;

   return diffuse + specular + ambient + indirect_diffuse;
}


float n = 10; // 1 100
float ior = 1.5; // 1 2.5
bool include_Fresnel = false;
bool divide_by_NdotL = true;

vec3 BRDF( vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y )
{
    vec3 H = normalize(L+V);

    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);

    float x = acos(NdotH) * n;
    float D = exp( -x*x);
    float G = (NdotV < NdotL) ?
        ((2*NdotV*NdotH < VdotH) ?
         2*NdotH / VdotH :
         1.0 / NdotV)
        :
        ((2*NdotL*NdotH < VdotH) ?
         2*NdotH*NdotL / (VdotH*NdotV) :
         1.0 / NdotV);

    // fresnel
    float c = VdotH;
    float g = sqrt(ior*ior + c*c - 1);
    float F = 0.5 * pow(g-c,2) / pow(g+c,2) * (1 + pow(c*(g+c)-1,2) / pow(c*(g-c)+1,2));

    float val = NdotH < 0 ? 0.0 : D * G * (include_Fresnel ? F : 1.0);

    if (divide_by_NdotL)
        val = val / dot(N,L);
    return vec3(val);
}

// Physically Based BRDF — Cook-Torrance specular + Lambertian diffuse.
//
// Uses the GGX/Trowbridge-Reitz normal distribution, Smith-GGX geometric
// shadowing/masking, and Schlick Fresnel approximation.
//
// Material inputs:
//   albedo          — base color (linear, no gamma)
//   metallic        — 0 = dielectric, 1 = metal
//   roughness       — perceptual roughness [0,1], squared internally to α
//   ambient_occlusion_factor — 0 = fully occluded, 1 = fully open
//
// For metals:    F0 = albedo,       diffuse contribution = 0
// For dielectrics: F0 = 0.04 (water/plastic default), diffuse = albedo
//
// Energy conservation:
//   The Cook-Torrance specular BRDF is:
//
//     f_spec(wi, wo) = D(h) · G(wi, wo) · F(wo, h)
//                      ─────────────────────────────
//                          4 · (n·wi) · (n·wo)
//
//   The diffuse BRDF (Lambertian) is:
//
//     f_diff = albedo / π
//
//   The full BRDF:
//
//     f = (1 - F) · (1 - metallic) · f_diff   +   F · f_spec
//
//   (1 - F) ensures energy lost to specular reflection is not also
//   diffusely reflected. Metals have no diffuse term.
//
// Rendering equation (single punctual light):
//
//   L_out(wo) = f(wi, wo) · L_i · (n · wi)
//             + albedo · L_ambient · AO          ← ambient term

vec3 brdf_pbr(
    in vec3  light_direction,
    in vec3  view_direction,
    in vec3  normal,
    in vec3  albedo,
    in float metallic,
    in float roughness,
    in vec3  light_color,
    in vec3  light_ambient_color,
    in float ambient_occlusion_factor
) {
    vec3  wi = normalize(light_direction);
    vec3  wo = normalize(view_direction);
    vec3  n  = normalize(normal);
    vec3  h  = normalize(wi + wo);

    float n_dot_l = max(dot(n, wi), 0.0);
    float n_dot_v = max(dot(n, wo), 0.0);
    float n_dot_h = max(dot(n, h),  0.0);
    float h_dot_v = max(dot(h, wo), 0.0);  // for Fresnel — angle between half-vector and view

    // ── Roughness remap ──────────────────────────────────────────────────────
    // Perceptual roughness is squared to get α (GGX width parameter).
    // This makes the roughness slider feel linear to artists:
    //   r=0.0 → α=0.0 (perfect mirror)
    //   r=0.5 → α=0.25
    //   r=1.0 → α=1.0 (fully rough / Lambertian-like specular)
    float alpha  = roughness * roughness;
    float alpha2 = alpha * alpha;   // used in D and G

    // ── F0: specular reflectance at normal incidence ──────────────────────────
    // Dielectrics: ~0.04 (water, plastic, skin — most non-metals)
    // Metals:      F0 = albedo (the metal color IS the specular color)
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // ── D: GGX Normal Distribution Function ──────────────────────────────────
    // Measures what fraction of microfacets have their normal aligned with h.
    //
    //           α²
    // D(h) = ──────────────────────────────
    //         π · ((n·h)² · (α²−1) + 1)²
    //
    // Peaks sharply at n·h=1 for small α (smooth), broad for large α (rough).
    {
        float denom = (n_dot_h * n_dot_h) * (alpha2 - 1.0) + 1.0;
        // denom² and π normalization
    }
    float D = alpha2 / (PI * pow((n_dot_h * n_dot_h) * (alpha2 - 1.0) + 1.0, 2.0));

    // ── G: Smith Geometric Shadowing-Masking ──────────────────────────────────
    // Accounts for microfacets shadowing each other (from light) and
    // masking each other (from view).
    //
    // Smith separates G into two independent 1D terms:
    //   G(wi, wo) = G1(wi) · G1(wo)
    //
    // GGX G1 (Schlick-GGX remapping):
    //   k = α / 2                     ← for punctual lights
    //
    //             n·v
    //   G1(v) = ───────
    //            n·v·(1−k) + k
    //
    // The remapping k = α/2 (not (α+1)²/8) is used for IBL; for punctual
    // lights the common choice is k = (α+1)²/8. We use the punctual form.
    float k  = (alpha + 1.0) * (alpha + 1.0) / 8.0;
    float G1_l = n_dot_l / (n_dot_l * (1.0 - k) + k);   // shadowing from light
    float G1_v = n_dot_v / (n_dot_v * (1.0 - k) + k);   // masking from view
    float G    = G1_l * G1_v;

    // ── F: Schlick Fresnel Approximation ─────────────────────────────────────
    // Fresnel describes how much light is reflected vs refracted as a function
    // of the angle between the view and the half-vector.
    //
    //   F(wo, h) = F0 + (1 − F0) · (1 − h·wo)^5
    //
    // At h·wo=1 (looking straight at surface): F = F0
    // At h·wo=0 (grazing angle):               F = 1.0 (full reflection)
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - h_dot_v, 5.0);

    // ── Cook-Torrance Specular ────────────────────────────────────────────────
    //
    //                D · G · F
    //   f_spec = ─────────────────
    //             4 · (n·wi)(n·wo)
    //
    // The 4·(n·wi)·(n·wo) denominator comes from the Jacobian of the
    // half-vector transform and the microfacet normalization.
    // We clamp the denominator to 0.0001 to avoid divide-by-zero at
    // grazing angles where n·wi or n·wo → 0.
    vec3 specular = (D * G * F) / max(4.0 * n_dot_l * n_dot_v, 0.0001);

    // ── Lambertian Diffuse ────────────────────────────────────────────────────
    // kD = fraction of light that is diffusely reflected.
    //
    //   kD = (1 − F) · (1 − metallic)
    //
    // (1 − F):       energy not reflected specularly is available for diffuse
    // (1 − metallic): metals have no diffuse term — free electrons absorb and
    //                 re-emit at the specular lobe only
    vec3  kD      = (1.0 - F) * (1.0 - metallic);
    vec3  diffuse = kD * albedo / PI;

    // ── Direct lighting ───────────────────────────────────────────────────────
    // Rendering equation for a punctual light:
    //   L_out = (f_diff + f_spec) · L_i · (n · wi)
    //
    // n_dot_l = cos(θ) — Lambert's law: irradiance falls as surface tilts away.
    // Both diffuse and specular receive it; specular's own n_dot_l in the
    // denominator cancels with this one only partially (the G term retains it).
    vec3 direct = (diffuse + specular) * light_color * n_dot_l;

    // ── Ambient ───────────────────────────────────────────────────────────────
    // A simple ambient approximation: albedo tinted by ambient light color,
    // modulated by AO. For metals the ambient term uses F0 instead of albedo
    // since metals have no diffuse. We blend between the two with metallic.
    //
    // A proper IBL implementation would replace this with:
    //   ambient = F · prefiltered_env(r, roughness) · BRDF_LUT(n_dot_v, roughness)
    //           + kD · irradiance_map(n) · albedo
    // but that requires precomputed cubemaps.
    vec3 ambient_albedo = mix(albedo, F0, metallic);
    vec3 ambient        = ambient_albedo * light_ambient_color * ambient_occlusion_factor;

    return direct + ambient;
}

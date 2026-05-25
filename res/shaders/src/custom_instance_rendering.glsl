
const bool show_border_outline = false;
const bool debug_color_for_missing_textures = true;

vec2 texture_sheet_uv(vec2 uv, uint tiles_x, uint tiles_y, float t, float cycles) {
   // Total frames in the sheet
   float total_frames = float(tiles_x * tiles_y);

   // Which frame are we on loops 'cycles' times over lifetime
   float frame = mod(t * cycles * total_frames, total_frames);
   uint frame_index = uint(floor(frame));

   // 2D position of the frame in the grid
   uint col = frame_index % tiles_x;
   uint row = frame_index / tiles_x;

   // Tile size in UV space
   vec2 tile_size = vec2(1.0 / float(tiles_x), 1.0 / float(tiles_y));

   // Unity orders rows top-to-bottom, flip row
   uint flipped_row = (tiles_y - 1) - row;

   // Offset uv into the correct tile
   return (uv * tile_size) + vec2(float(col), float(flipped_row)) * tile_size;
}

vec3 health_background(vec2 uv, float health_percent) {
   vec3 background = vec3(0.15, 0.15, 0.15);

   if (uv.x > health_percent) {
      return background;
   }

   const float intensity = 4.2;
   const float c0_intensity = 1. + (intensity - 1.) * 14.14 ;
   const vec3  c0 = vec3(154., 38., 27.)  / 255. * c0_intensity;

   const float c1_intensity = 1. + (intensity - 1.) * 2.2;
   const vec3  c1 = vec3(170., 53., 38.)  / 255. * c1_intensity;

   const float c2_intensity = 1. + (intensity - 1.) * 4.1;
   const vec3  c2 = vec3(203., 106., 96.) / 255. * c2_intensity;

   // const float c3_intensity = 1. + (intensity - 1.) * (10. + 32. * ((1. + sin(per_frame.elapsed_time)/2.)));
   const float c3_intensity = 1. + (intensity - 1.) * (10. + 32.);
   const vec3  c3 = vec3(195.,110.,101.)  / 255. * c3_intensity;

   // The first 0.38 percent from top to bottom
   float power_n = 0.6;
   float bot = 0.;
   float top = 1. - (0.38 + 0.1);
   if (uv.y < top && uv.y > bot) {
      float t = remap(uv.y, bot, top, 0., 1.);
      t = pow(t, power_n);
      return lerp(c0, c1, t);
   }

   power_n = 3;
   bot = top;
   top = 1. - (0.38);
   if (uv.y < top && uv.y > bot) {
      float t = remap(uv.y, bot, top, 0., 1.);
      t = pow(t, power_n);
      return lerp(c1, c2, t);
   }

   power_n = 1;
   bot = top;
   top = 1.;
   if (uv.y < top && uv.y > bot) {
      float t = remap(uv.y, bot, top, 0., 1.);
      t = pow(t, power_n);
      return lerp(c2, c3, t);
   }

   return background;
}


// NOTE: Adapted from this https://www.shadertoy.com/view/mlXSzS and this https://www.shadertoy.com/view/dlXSzB
vec4 health_bar_sdf(vec2 uv, float health_current, float health_max, float bar_aspect_ratio) {
   float NUM_TICKS   = (health_max / 100.) - 1.;
   const float DIVISOR     = 10.0;
   const float TICK_WIDTH  = 0.009; // fraction of quad width
   const float TICK_HEIGHT = 0.6;  // fraction of quad height
   if (uv.x < TICK_WIDTH || uv.x > (1.f - TICK_WIDTH) || uv.y < bar_aspect_ratio * TICK_WIDTH || uv.y > (1.f - bar_aspect_ratio * TICK_WIDTH)) {
      return vec4(0., 0., 0., 1.);
   }


   vec3 background = health_background(uv, health_current/health_max);


   float slot_width = 1.0 / NUM_TICKS;
   float tick_index = round(uv.x / slot_width);

   bool is_major = mod(tick_index, DIVISOR) == 0.0;
   float tick_w = is_major ? TICK_WIDTH * 2.0 : TICK_WIDTH;
   float tick_h = is_major ? 1.0 : TICK_HEIGHT;

   float tick_center = tick_index * slot_width;
   float half_w = tick_w * 0.5;

   // uv.y = 0 is bottom of quad, ticks hang from top
   bool in_x = uv.x > tick_center - half_w && uv.x < tick_center + half_w;
   bool in_y = uv.y > (1.0 - tick_h);

   vec3 color = (in_x && in_y) ? vec3(0., 0, 0) : background;
   return vec4(color, 1.0);
}



vec3 mix_particle_color_multiply(vec3 texture_color, vec3 particle_color) {
   vec3 hsv_particle = rgb_to_hsv(particle_color);
   vec3 hsv_texture = rgb_to_hsv(texture_color);
   // Take hue+sat from particle, multiply values together
   return hsv_to_rgb(vec3(hsv_particle.xy, hsv_particle.z * hsv_texture.z));
}

// Gradient noise from Jorge Jimenez's presentation:
// http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
// Example of dithering: https://www.shadertoy.com/view/MlV3R1
float gradient_noise(in vec2 uv) {
   float noise = fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
   if (false) {
      noise = fract(sin(dot(uv, vec2(12.9898,78.233))) * 43758.5453);
   }
   return noise;
}

vec4 custom(vec2 uv, uint render_state, uint instance_rendering_mode, vec4 custom_1, vec4 custom_2) {
   if (2 == instance_rendering_mode) {
      const float intensity = 100.;
      return vec4(vec3(1.) * intensity, 1.) * color_tint;
   }

   if (instance_rendering_mode >= 3) {

      uint  tiles_x         = floatBitsToInt(custom_2.x);
      uint  tiles_y         = floatBitsToInt(custom_2.y);
      // float cycles       = custom_2.z;
      float cycles          = 1;
      float frame_over_time = custom_2.w;
      // float t = per_frame.elapsed_time;
      float t = custom_1.w;


      // uint tiles_x = 2; uint tiles_y = 3; float t = per_frame.elapsed_time; float cycles = 1;

      if (!(0 == tiles_x)) {
         // tiles_x = 2; tiles_y = 3; t = per_frame.elapsed_time/2; cycles = 1;
         // cycles = tiles_x*tiles_y);
         t = (2/6.);
         uv = texture_sheet_uv(uv, tiles_x, tiles_y, t, cycles);
      }

      const float thickness = 0.025;
      const bool in_border = (uv.x < thickness || uv.x > (1. - thickness) || uv.y < thickness || uv.y > (1. - thickness));
      if (show_border_outline && in_border) {
         // Orient by color. Useful for debugging.
         if (uv.y > (1. - thickness)) {
            return vec4(0., 0., 1., 0.8);
         }

         if (uv.x < thickness) {
            return vec4(1., 1., 1., 0.8);
         }
         return vec4(1., 0., 0., 0.8);
      }

      const float hdr_intensity                   = custom_1.x;
      const float hdr_alpha_compose               = custom_1.y;
      const float hdr_alpha_coefficient_intensity = custom_1.z;

      const Material material = materials[material_index];
      vec4 diffuse_texture = vec4(1.);
      if (material.diffuse_handle != uvec2(0)) {
         diffuse_texture = texture(sampler2D(material.diffuse_handle), uv);
      } else {
         if (debug_color_for_missing_textures) {
            return vec4(1., 0., 0., 1.);
         }
      }

      // TODO: Mode gamma_correction to after sbti loading
      const float noise = gradient_noise(gl_FragCoord.xy + vec2(gl_SampleID));

      const float scale = 1. / 255.;
      float added_noise = lerp(-0.5 * scale, 0.5 * scale, noise);
      // NOTE: Somehow adding a noise lags
      added_noise *= 0;

      const vec3 tint   = srgb_to_linear(color_tint.rgb);
      // const vec3 tint   = srgb_to_linear(vec3(0.4, 0, 0));
      const vec3 albedo = srgb_to_linear(diffuse_texture.rgb) * tint + added_noise;
      // const vec3 albedo = mix_particle_color_multiply(srgb_to_linear(diffuse_texture.rgb), tint) + added_noise;

      const float alpha = saturate(diffuse_texture.a * color_tint.a);
      // const float alpha = saturate(luminance(albedo) * diffuse_texture.a * color_tint.a);
      // const float alpha = saturate(brightness(albedo) * diffuse_texture.a * color_tint.a);



      const float hdr_intensity_linear = lerp(
         hdr_intensity,
         pow(hdr_intensity, hdr_alpha_coefficient_intensity * alpha),
         hdr_alpha_compose
      );

      const vec3 hdr_color = srgb_to_linear(vec3(191., 191., 191.) / 255.0) * hdr_intensity_linear;
      vec4 fragment_color = vec4(albedo * hdr_color, alpha);

      // const uint blend_mode = render_state - 1; // NOTE: not the best since this number is pretty much dependent on C enum. Yikes.
      const uint blend_mode = 0; // NOTE: not the best since this number is pretty much dependent on C enum. Yikes.
      if (0 == blend_mode) {
         // Alpha blend: premultiplied
         fragment_color = vec4(albedo * hdr_color * alpha, alpha);
      } else if (1 == blend_mode) {
         // Additive: alpha = 0 so dst is unchanged, src adds on top
         // (ONE, ONE) blend mode i think is necessary
         fragment_color = vec4(albedo * hdr_color, alpha);
         // fragment_color = vec4(albedo, 0);
      } else {
         float grey = dot(albedo * hdr_color, vec3(0.2126, 0.7152, 0.0722));
         return vec4(0.0, 0.0, 0.0, 1.0 - grey);
      }

      return fragment_color;

   }

   float current = custom_1.x;
   float max     = custom_1.y;
   float aspect  = custom_1.z;
   if (gl_FrontFacing) {
      return vec4(1.);
   }
   vec4 health = health_bar_sdf(uv, current, max, aspect);
   return health;
}

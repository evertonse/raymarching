

const bool show_border_outline              = false;
const bool debug_color_for_missing_textures = true;

// https://github.com/keaukraine/webgl-buddha/blob/a37daa8eb2f6391077cfec0b4f1d589085194153/js/app/SoftDiffuseColoredShader.js#L28
float linearize_depth(float ndc_depth, float near, float far) {
   float clip_z = ndc_depth * 2.0 - 1.0;
   return (2.0 * near * far) / (far + near - clip_z * (far - near));
}

//
// Beware that 'linearize_depth' might not be exact actually. Because we warp based on x and y and don't actually use a matrix to do perpective
// Search this function in codebase vec4 perspective_from_fov(vec3 position, float fov_y_rad, float aspect, float z_near, float z_far);
// It might even be linear because I remember playing with it. So it might not be perfect right now but it does smooth out particles enough.
// It's not that critical, as long as they're in the same space even if not linear.
// If we see any hard particles, 'linearize_depth' is most likely the culprit
//
// SimonDev video about soft particles: https://youtu.be/arn_3WzCJQ8?si=tREAjBbc2lCfHY27
//
float soft_particle_fade(float fade_distance) {
   // Sample scene depth at this pixel (assuming framebuffer_depth_texture is the same width as the framebuffer)
   const vec2 screen_uv = gl_FragCoord.xy / textureSize(framebuffer_depth_texture, 0);
   const float scene_ndc_depth = texture(framebuffer_depth_texture, screen_uv).r;

   const float near = near_plane, far = far_plane;

   // Linearize both depths
   const float scene_linear    = linearize_depth(scene_ndc_depth, near_plane, far_plane);
   const float fragment_linear = linearize_depth(gl_FragCoord.z, near_plane, far_plane);

   // Distance from particle to scene geometry
   const float depth_difference = scene_linear - fragment_linear;

   // The closer 'depth_difference' gets to 0 the more faded it's gonna be.
   float t = saturate(depth_difference / fade_distance);

   t = smoothstep(0., 1., t);
   // t *= t;
   return t;
}


vec2 texture_sheet_uv(vec2 uv, uvec2 tiles, float start_frame, float t, float cycles) {
   float total_frames = float(tiles.x * tiles.y);

   // Which frame are we considering we need to complete 'cycles' times over lifetime ('t' gets to 1 it means we have to complete had completed 'cycles' cycles)
   uint frame_index = uint(floor(mod(start_frame + t * cycles * total_frames, total_frames)));

   // 2D positions in the grid
   uint col = frame_index % tiles.x;
   uint row = frame_index / tiles.x;

   // UV space
   vec2 tile_size = vec2(1.0 / float(tiles.x), 1.0 / float(tiles.y));

   // Unity orders rows top to bottom, flip row to allow easy porting for us
   uint flipped_row = (tiles.y - 1) - row;

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
   vec2 screen_size  = textureSize(framebuffer_depth_texture, 0);
   vec2 screen_uv    = gl_FragCoord.xy / screen_size;
   vec4 scene_depth = texture(framebuffer_depth_texture, uv);

   if (false && 1200. == screen_size.x && 1012. == screen_size.y) {
      // vec3 out_now = vec3(linearize_depth(scene_depth.r, near_plane, far_plane));
      vec3 out_now = vec3(linearize_depth(scene_depth.r, near_plane, far_plane));
      // return vec4(out_now, 1);
      return vec4(vec3(scene_depth.r), 1);
   }

   if (2 == instance_rendering_mode) {
      const float intensity = 100.;
      return vec4(vec3(1.) * intensity, 1.) * color_tint;
   }

   if (instance_rendering_mode >= 3) {
      uvec2 tiles       = unpack_u16_from_float(custom_2.x);
      float start_frame = custom_2.y;
      float cycles      = custom_2.z;
      float t           = custom_1.w;
      if (0 != tiles.x) {
         uv = texture_sheet_uv(uv, tiles, start_frame, t, cycles);
      }


      if (show_border_outline) {
         const float thickness = 0.025;
         const bool  in_border = (uv.x < thickness || uv.x > (1. - thickness) || uv.y < thickness || uv.y > (1. - thickness));
         if (in_border) {
            // Orient by color. Useful for debugging.
            if (uv.y > (1. - thickness)) {
               return vec4(0., 0., 1., 0.8);
            }
            if (uv.x < thickness) {
               return vec4(1., 1., 1., 0.8);
            }
            return vec4(1., 0., 0., 0.8);
         }
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
      // const float noise = gradient_noise(gl_FragCoord.xy + vec2(gl_SampleID));

      const float scale = 1. / 255.;

      const vec3 tint   = srgb_to_linear(color_tint.rgb);
      // const vec3 tint   = srgb_to_linear(vec3(0.4, 0, 0));
      const vec3 albedo = srgb_to_linear(diffuse_texture.rgb) * tint;

      // TODO: Make this be dependent on the acutal size of particle, SimonDev show how some times it's too smooth to the point of seemingly never appear in front of geometry
      // And if it's too little smooth, well you get hard particles.
      const float fade_distance = 0.000095;
      const float soft_particle_alpha = soft_particle_fade(fade_distance);

      const float alpha = saturate(diffuse_texture.a * color_tint.a * soft_particle_alpha);
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


#include "./remaps.glsl"

vec3 health_background(vec2 uv, float health_percent) {
   vec3 background = vec3(0.15, 0.15, 0.15);

   if (uv.x > health_percent) {
      return background;
   }

   const vec3 c0 = vec3(154., 38., 27.)  / 255.;
   const vec3 c1 = vec3(170., 53., 38.)  / 255.;
   const vec3 c2 = vec3(203., 106., 96.) / 255.;
   const vec3 c3 = vec3(195.,110.,101.)  / 255.;

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
   uv.y = 1.f - uv.y; // quick remap to make y be the bottom of our quads and
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
/* Gradient noise from Jorge Jimenez's presentation: */
/* http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
float gradient_noise(in vec2 uv) {
   return fract(52.9829189 * fract(dot(uv, vec2(0.06711056, 0.00583715))));
}

vec4 custom(vec2 uv, uint instance_rendering_mode, vec4 custom_1, vec4 custom_2) {

   if (2 == instance_rendering_mode) {
      const float thickness = 0.002;
      if (uv.x < thickness || uv.x > (1. - thickness) || uv.y < thickness || uv.y > (1. - thickness)) {
         // return vec4(1., 0., 0., 1.);
      }

      Material material = materials[material_index];
      if (material.diffuse_handle != uvec2(0)) {
         // TODO: Mode gamma_correction to after sbti loading
         vec4 dtexture = vec4(1.);
         dtexture = texture(sampler2D(material.diffuse_handle), uv);
         // gl_FragCoord.xy
         dtexture += (1.0 / 255.0) * gradient_noise(uv) - (0.5 / 255.0);
         // dtexture.rgb = gamma_correct_texture(dtexture.rgb);
         // return dtexture * vec4(17/255., 24/255., 34/255., color_tint.w);
         // return vec4(dtexture.rgb * dtexture.a * color_tint.rgb, dtexture.a * color_tint.a);
         // float noise = (1.0 / 255.0) * gradient_noise(gl_FragCoord.xy);
         float noise = gradient_noise(gl_FragCoord.xy);
         float a = dtexture.a + dtexture.a*noise;
         return vec4(dtexture.rgb, a) * color_tint;
         // return vec4(vec3(1.), pow(alpha, 2.2)  + gradient_noise(gl_FragCoord.xy));
      } else {
         return vec4(1., 0., 0., 1.);
      }
   }

   float current = custom_1.x;
   float max     = custom_1.y;
   float aspect  = custom_1.z;
   if (gl_FrontFacing) {
      return vec4(1.);
   }
   return health_bar_sdf(uv, current, max, aspect);
}

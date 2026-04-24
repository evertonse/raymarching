
// NOTE: Adapted from this https://www.shadertoy.com/view/mlXSzS and this https://www.shadertoy.com/view/dlXSzB
vec4 health_bar_sdf(vec2 uv, float health_current, float health_max) {
   uv.y = 1.f - uv.y; // quick remap to make y be the bottom of our quads and
   const float NUM_TICKS   = 50.0;
   // float NUM_TICKS   = (health_max / 100.) - 1.;
   // const float NUM_TICKS   = 10;
   const float DIVISOR     = 5.0;
   const float TICK_WIDTH  = 0.003; // fraction of quad width
   const float TICK_HEIGHT = 0.5;  // fraction of quad height
   vec3 background = vec3(0.15, 0.15, 0.15);
   if (0.4 > uv.y) {
      background = vec3(195., 110., 101.) / 255.;
   }
   if (uv.y < 0.1 && uv.x < 0.1) {
      background = vec3(195., 120., 101.) / 255.;
   }

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

vec4 custom(vec2 uv, vec4 custom_1, vec4 custom_2) {
   float current = custom_1.x;
   float max = custom_1.y;
   return health_bar_sdf(uv, current, max);
}

// Usage: Particle_System ps = particle_system_from_string(src);

static stb_lexer L;
static char L_store[1024];
static bool L_pushed;

static void lex_init(const char *s) {
}
static long lex_next(void) {
   return L_pushed ? (L_pushed = false, L.token) : stb_c_lexer_get_token(&L) ? L.token : 0; 
}
static long lex_peek(void) {
   long t = lex_next();
   L_pushed = true;
   return t;
}
static void lex_expect(long t) { lex_next(); /* trust the format */ }

static float parse_expr(void) {
   float sign = 1;
   if (lex_peek() == '-') {
      lex_next();
      sign = -1;
   }
   long t = lex_next();
   float v = (t == CLEX_floatlit) ? (float)L.real_number : (t == CLEX_intlit) ? (float)L.int_number : (t == CLEX_id && !strcmp(L.string, "DEG2RAD")) ? (3.14159265f / 180.f) : (t == CLEX_id && !strcmp(L.string, "true")) ? 1.f : (t == CLEX_id && !strcmp(L.string, "false")) ? 0.f : 0.f;
   v *= sign;
   if (lex_peek() == '*') {
      lex_next();
      v *= parse_expr();
   }
   if (lex_peek() == '/') {
      lex_next();
      v /= parse_expr();
   }
   return v;
}

static void skip_value(void) {
   if (lex_peek() == '{') {
      lex_next();
      int d = 1;
      while (d > 0) {
         long t = lex_next();
         if (t == '{')
            d++;
         else if (t == '}' || t == 0)
            d--;
      }
   } else {
      long t;
      while ((t = lex_next()) != ',' && t != '}' && t != 0)
         ;
      if (t == '}' || t == ',')
         L_pushed = true;
   }
}

static void parse_fields(Particle_System *ps, char *path, int plen);

static void apply_leaf(Particle_System *ps, const char *path) {
#define F(P, DST)                                                                                                                                                                                                                                                                                                              \
   if (!strcmp(path, P)) {                                                                                                                                                                                                                                                                                                     \
      ps->DST = parse_expr();                                                                                                                                                                                                                                                                                                  \
      return;                                                                                                                                                                                                                                                                                                                  \
   }
#define FB(P, DST)                                                                                                                                                                                                                                                                                                             \
   if (!strcmp(path, P)) {                                                                                                                                                                                                                                                                                                     \
      ps->DST = (bool)parse_expr();                                                                                                                                                                                                                                                                                            \
      return;                                                                                                                                                                                                                                                                                                                  \
   }
   F("duration", duration)
   FB("looping", looping)
   F("spawn_rate", spawn_rate)
   F("spawn_accumulator", spawn_accumulator)
   F("start.lifetime.min", start.lifetime.min)
   F("start.lifetime.max", start.lifetime.max)
   F("start.speed.min", start.speed.min)
   F("start.speed.max", start.speed.max)
   F("start.size.min", start.size.min)
   F("start.size.max", start.size.max)
   F("start.gravity.min", start.gravity.min)
   F("start.gravity.max", start.gravity.max)
   F("start.drag.min", start.drag.min)
   F("start.drag.max", start.drag.max)
   F("start.color.from.x", start.color.from.x)
   F("start.color.from.y", start.color.from.y)
   F("start.color.from.z", start.color.from.z)
   F("start.color.from.w", start.color.from.w)
   F("start.color.to.x", start.color.to.x)
   F("start.color.to.y", start.color.to.y)
   F("start.color.to.z", start.color.to.z)
   F("start.color.to.w", start.color.to.w)
   F("start.rotation.from.x", start.rotation.from.x)
   F("start.rotation.from.y", start.rotation.from.y)
   F("start.rotation.from.z", start.rotation.from.z)
   F("start.rotation.to.x", start.rotation.to.x)
   F("start.rotation.to.y", start.rotation.to.y)
   F("start.rotation.to.z", start.rotation.to.z)
   F("over_lifetime.rotation.x", over_lifetime.rotation.x)
   F("over_lifetime.rotation.y", over_lifetime.rotation.y)
   F("over_lifetime.rotation.z", over_lifetime.rotation.z)
   F("over_lifetime.size.x", over_lifetime.size.x)
   F("over_lifetime.size.y", over_lifetime.size.y)
   F("over_lifetime.size.z", over_lifetime.size.z)
   F("over_lifetime.color.count", over_lifetime.color.count)
   F("transform_mode.speed_scale", transform_mode.speed_scale)
   F("transform_mode.length_scale", transform_mode.length_scale)
   F("spawn_shape.half_angle", spawn_shape.half_angle)
   F("spawn_shape.direction.x", spawn_shape.direction.x)
   F("spawn_shape.direction.y", spawn_shape.direction.y)
   F("spawn_shape.direction.z", spawn_shape.direction.z)
   F("transform.translation.x", transform.position.x)
   F("transform.translation.y", transform.position.y)
   F("transform.translation.z", transform.position.z)
   F("transform.scale.x", transform.scale.x)
   F("transform.scale.y", transform.scale.y)
   F("transform.scale.z", transform.scale.z)
   F("transform.rotation.x", transform.rotation.x)
   F("transform.rotation.y", transform.rotation.y)
   F("transform.rotation.z", transform.rotation.z)
   F("transform.rotation.w", transform.rotation.w)
   FB("is_local_simulation_space", is_local_simulation_space)
#undef F
#undef FB

   if (!strcmp(path, "texture_path")) {
      lex_next();
      ps->texture_path = L.string;
      return;
   }
   if (!strcmp(path, "transform_mode.value")) {
      lex_next();
      ps->transform_mode.value = !strcmp(L.string, "PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD") ? PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD : PARTICLE_TRANSFORM_MODE_BILLBOARD;
      return;
   }
   if (!strcmp(path, "spawn_shape.value")) {
      lex_next();
      ps->spawn_shape.value = !strcmp(L.string, "PARTICLE_SPAWN_SHAPE_CONE") ? PARTICLE_SPAWN_SHAPE_CONE : !strcmp(L.string, "PARTICLE_SPAWN_SHAPE_SPHERE") ? PARTICLE_SPAWN_SHAPE_SPHERE : PARTICLE_SPAWN_SHAPE_DIRECTION;
      return;
   }

   // over_lifetime color arrays parse positionally
   // e.g. path = "over_lifetime.color.values" or "over_lifetime.color.timings"
   if (!strcmp(path, "over_lifetime.color.values")) {
      lex_expect('{');
      for (uint i = 0; i < PARTICLE_MAX_COLOR_KEYS && lex_peek() != '}'; i++) {
         if (lex_peek() == '{') {
            char sub[256];
            snprintf(sub, sizeof(sub), "over_lifetime.color.values.%d", i);
            parse_fields(ps, sub, strlen(sub)); // will hit x/y/z/w but we don't map those
                                                // simpler: just parse 4 floats positionally
         }
         if (lex_peek() == ',')
            lex_next();
      }
      lex_expect('}');
      return;
   }

   skip_value();
}

static void parse_fields(Particle_System *ps, char *path, int plen) {
   lex_expect('{');
   long t;
   while ((t = lex_next()) != '}' && t != 0) {
      if (t != '.')
         continue;
      lex_next(); // field name in L.string

      // append .fieldname to path
      char save = path[plen];
      int nlen = snprintf(path + plen, 256 - plen, "%s%s", plen ? "." : "", L.string);
      nlen += plen;

      lex_expect('=');

      if (lex_peek() == '{')
         parse_fields(ps, path, nlen);
      else
         apply_leaf(ps, path);

      path[plen] = save; // restore path

      if (lex_peek() == ',')
         lex_next();
   }
}

Particle_System particle_system_from_string(const char *src) {
   Particle_System ps = particle_system_default;
   stb_c_lexer_init(&L, s, s + strlen(s), L_store, sizeof(L_store));
   L_pushed = false;
   char path[256] = {0};
   parse_fields(&ps, path, 0);
   return ps;
}

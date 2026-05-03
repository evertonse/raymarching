#include "raymath.h"

typedef struct {
   Vector3 position;
   Vector3 velocity;

   struct {
      Vector3 current, start;
   } size;

   struct {
      Color current, start;
   } color;

   struct {
      Vector3 current, start;
   } rotation;

   float age;
   float lifetime;

   bool alive;
} Particle;


#define MAX_PARTICLES 50

typedef struct {

   // emitter
   float duration;
   bool  looping;
   float emitter_age;

   // spawn
   float spawn_rate;
   float spawn_accumulator;

   struct {

      struct {
         float min, max;
      } lifetime;

      struct {
         float min, max;
      } speed;

      struct {
         float min, max;
      } size;

      struct {
         Color from, to;
      } color;

      struct {
         Vector3 from, to;
      } rotation;

   } start;

   struct {
      #define PARTICLE_MAX_COLOR_KEYS 8

      struct {
         Color values [PARTICLE_MAX_COLOR_KEYS];
         float timings[PARTICLE_MAX_COLOR_KEYS];
         uint  count;
      } color;

      Vector3 rotation;

      Vector3 size;

   } over_lifetime;

   bool is_billboard;

   ZString texture_path;

   Particle particles[MAX_PARTICLES];

   uint particle_count;

} Particle_System;


Vector3 overload lerp(Vector3 a, Vector3 b, float t) {
   return Vector3Lerp(a, b, t);
}

Color overload lerp(Color a, Color b, float t) {
   return Vector4Lerp(a, b, t);
}


Color internal sample_color_gradient(Color *colors, float *timings, uint count, float t) {
   // before first key
   if (t <= timings[0]) {
      return colors[0];
   }

   // after last key
   if (t >= timings[count - 1]) {
      return colors[count - 1];
   }

   // find segment
   for (uint i = 0; i < count - 1; i++) {

      float t0 = timings[i];
      float t1 = timings[i + 1];

      if (t >= t0 && t <= t1) {

         // local interpolation inside segment
         float local_t = (t - t0) / (t1 - t0);

         return lerp(colors[i], colors[i + 1], local_t);
      }
   }

   return colors[count - 1];
}


void internal spawn_particle(Particle_System *ps, Vector3 emitter_position) {
   for (uint i = 0; i < MAX_PARTICLES; i++) {
      Particle *p = &ps->particles[i];
      if (p->alive) {
         continue;
      }

      // randomize within ranges
      float lifetime = randf_range(ps->start.lifetime.min, ps->start.lifetime.max);
      float speed    = randf_range(ps->start.speed.min, ps->start.speed.max);
      float size     = randf_range(ps->start.size.min, ps->start.size.max);
      float t_color  = randf_range(0.0f, 1.0f);

      // Pick random value per axis between from and to
      Vector3 rotation = {
          randf_range(ps->start.rotation.from.x, ps->start.rotation.to.x),
          randf_range(ps->start.rotation.from.y, ps->start.rotation.to.y),
          randf_range(ps->start.rotation.from.z, ps->start.rotation.to.z),
      };

      // Random direction uniform sphere so particles spread in all directions
      // if we want flat emission (ground effect) we need to zero out the y component
      // Actually just any flatten effect on any plane is just a projection away
      float theta = randf_range(0.0f, 2.0f * PI);
      float phi = randf_range(0.0f, PI);
      Vector3 dir = {
          sinf(phi) * cosf(theta),
          sinf(phi) * sinf(theta),
          cosf(phi),
      };

      p->position = emitter_position;
      p->velocity = Vector3Scale(dir, speed);

      p->size.start = (Vector3){size, size, size};
      p->size.current = p->size.start;

      p->color.start = lerp(ps->start.color.from, ps->start.color.to, t_color);
      p->color.current = p->color.start;

      p->rotation.start = rotation;
      p->rotation.current = rotation;

      p->age = 0.0f;
      p->lifetime = lifetime;
      p->alive = true;
      return;
   }
   // silently drop if full
}


void internal update_particle(Particle_System *ps, Particle *p, float dt) {
   if (!p->alive) {
      return;
   }

   p->age += dt;

   if (p->age >= p->lifetime) {
      p->alive = false;
      return;
   }

   float t = p->age / p->lifetime;

   // Velocity update (before position)
   // gravity
   p->velocity.y -= 9.8f * dt;

   float drag_per_second = 0.3f;  // 0=no drag, 1=instant stop per second
   p->velocity = mul(powf(1.0f - drag_per_second, dt), p->velocity);

   // position from velocity (after velocity is updated by forces)
   p->position = add(p->position, mul(p->velocity, dt));

   // color, rotation, size over lifetime
   p->color.current = mul(
      p->color.start,
      sample_color_gradient(ps->over_lifetime.color.values, ps->over_lifetime.color.timings, ps->over_lifetime.color.count, t)
   );

   // over_lifetime.rotation is degrees per second, additive per frame
   p->rotation.current = add(p->rotation.current, mul(ps->over_lifetime.rotation, dt));

   p->size.current     = lerp(p->size.start, mul(p->size.start, ps->over_lifetime.size), t);
}


void update_particle_system(Particle_System *ps, Vector3 emitter_position, float dt) {

   // Advance emitter clock
   ps->emitter_age += dt;
   if (ps->looping && ps->emitter_age >= ps->duration) {
      ps->emitter_age -= ps->duration;
      // -= instead of = 0 so we don't lose overshoot
      // e.g. duration = 1s and dt pushed us to 1.003s then emitter_age becomes 0.003s
      // keeping that tiny remainder means emission stays perfectly timed
   }

   bool emitting = ps->looping || (ps->emitter_age < ps->duration);

   // Emit new particles
   if (emitting) {
      // Accumulator is needed to avoid spawning 0 every frame
      ps->spawn_accumulator += ps->spawn_rate * dt;
      while (ps->spawn_accumulator >= 1.0f) {
         spawn_particle(ps, emitter_position);
         ps->spawn_accumulator -= 1.0f;
      }
   }

   // Simulate live particles
   ps->particle_count = 0;

   for (uint i = 0; i < MAX_PARTICLES; i++) {
      Particle *p = &ps->particles[i];
      if (!p->alive) {
         continue;
      }
      update_particle(ps, p, dt);
      ps->particle_count++;
   }
}


void setup_particle_render_state(void) {
   // Rendering Mode = Fade
   // SrcAlpha, OneMinusSrcAlpha — color AND alpha fade together
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD);

   // ZWrite = Off — particles don't write depth
   // they still READ depth (test against opaque geometry)
   // but don't block each other
   glDepthMask(GL_FALSE);
   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_LESS);

   // no backface culling — quads are single sided
   // but we want both sides visible if camera goes behind
   glDisable(GL_CULL_FACE);

   // trace_info("simulation_speed = %f", simulation_speed);
   // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   int values[] = {GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA, GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR, GL_CONSTANT_ALPHA, GL_ONE_MINUS_CONSTANT_ALPHA};
   ZString values_string[] = {
       "GL_ZERO", "GL_ONE", "GL_SRC_COLOR", "GL_ONE_MINUS_SRC_COLOR", "GL_DST_COLOR", "GL_ONE_MINUS_DST_COLOR", "GL_SRC_ALPHA", "GL_ONE_MINUS_SRC_ALPHA", "GL_DST_ALPHA", "GL_ONE_MINUS_DST_ALPHA", "GL_CONSTANT_COLOR", "GL_ONE_MINUS_CONSTANT_COLOR", "GL_CONSTANT_ALPHA", "GL_ONE_MINUS_CONSTANT_ALPHA"};
   static int current_1 = 0;
   static int current_2 = 0;

   if (is_button_pressed(BUTTON_1)) {
      current_1 = (current_1 + 1) % count_of(values);
      trace_info("glBlendFunc(%s, %s)", values_string[current_1], values_string[current_2]);
   }

   if (is_button_pressed(BUTTON_2)) {
      current_2 = (current_2 + 1) % count_of(values);
      trace_info("glBlendFunc(%s, %s)", values_string[current_1], values_string[current_2]);
   }
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   // glBlendFunc(values[current_1], values[current_2]);

   // glBlendEquation(GL_FUNC_SUBTRACT);

   glDepthMask(GL_FALSE);
}


void draw_vfx(Vector3 unit_position, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   Color particle_color = vector4(17/255., 24/255., 34/255., 1.);
   static Particle_System ps = {0};

   static bool loaded = false;
   static Model quad = {0};
   static Scene_Node quad_nodes[MAX_PARTICLES] = {0};
   setup_particle_render_state();

   if (!loaded) {
      loaded = true;
      // quad = create_model_from_mesh(generate_quad_mesh(1.0f, 1.0f));

      quad = create_model_from_mesh(
         generate_quad_mesh(1.0f, 1.0f),
         "res/textures/vfx/Swirl01.png", nullptr, nullptr, nullptr
      );

      // first node owns the mesh
      quad_nodes[0] = create_scene_node(&quad, transform_identity);

      // rest share the same renderable, start invisible
      for (uint i = 1; i < MAX_PARTICLES; i++) {
         quad_nodes[i] = create_scene_node(quad_nodes[0], transform_identity);
         update_color_tint(quad_nodes[i], vector4(0.f));
      }
      // hide the first one too until a particle claims it
      update_color_tint(quad_nodes[0], vector4(0.f));

      ps = (Particle_System){
         // emitter
         .duration = 1.,
         .looping = true,

         // spawn
         .spawn_rate = 10,
         .start = {
            .lifetime = {
               .min = 0.6,
               .max = 0.8
            },
            .size = {
               .min = 7 * 6,
               .max = 7 * 6
            },
            .color = {
               .from = particle_color,
               .to = particle_color
            },
            .rotation = {
               .from = mul(DEG2RAD, vector3( 360, 360, -360)),
               .to   = mul(DEG2RAD, vector3(-360, -360, 360)),
            },
         },
         .over_lifetime = {
            .color = {
               .values  = {vector4(1, 1, 1, 0), vector4(1), vector4(1, 1, 1, 0)},
               .timings = {0., 0.5, 1.0},
               .count   = 3
            },
            .rotation = mul(DEG2RAD, vector3(360)),
            .size = vector3(1.f),
         },
         .is_billboard = true,
         .texture_path = nullptr,
      };
   }

   static float simulation_speed = 1.;

   if (is_button_pressed(BUTTON_RIGHT)) {
      simulation_speed = 1.;
      trace_info("simulation_speed = %f", simulation_speed);
   } else if (is_button_pressed(BUTTON_LEFT)) {
      simulation_speed = 0.05;
      trace_info("simulation_speed = %f", simulation_speed);
   }

   update_particle_system(&ps, unit_position, time_delta() * simulation_speed);
   for (uint i = 0; i < MAX_PARTICLES; i++) {
      Particle *p = &ps.particles[i]; // ps is local struct, not pointer → .
      Scene_Node node = quad_nodes[i];

      if (!p->alive) {
         // hide dead slot zero alpha makes it invisible
         update_color_tint(node, vector4(1., 0., 0., 0.));
         continue;
      }

      // DIRTY
      update_rendering_mode(node, 2);

      {
         // Rotation
         Quaternion face = billboard_rotation(true, p->position, camera_position, camera_forward, camera_right, camera_up);
         // Quaternion spin = QuaternionFromAxisAngle(camera_forward, p->rotation.current.z * DEG2RAD);
         float pitch = p->rotation.current.y, yaw = p->rotation.current.x, roll = p->rotation.current.z;
         Quaternion spin = QuaternionFromEuler(pitch, yaw, roll);
         Quaternion rotation  = QuaternionMultiply(face, spin);

         // Transform
         Transform t = {
             .translation = p->position,
             .rotation = rotation,
             .scale = p->size.current,
         };
         update_transform(node, t);
      }

      update_color_tint(node, p->color.current);
   }
}

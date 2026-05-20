#include "raymath.h"
#include <math.h>

const Vector3 default_gravity = {0, -9.81, 0};

typedef struct {
   //
   // These are in System Space
   //
   Vector3 position; // local space
   Vector3 velocity;

   Vector3 spawned_position; // world space emitter position at the time of spawn

   float drag;  // Drag per second. Default to 0.25f // 0=no drag, 1=instant stop per second
   float gravity; // Each particle can be affected by different gravity values permitting slow or fast fall

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


typedef enum {
   PARTICLE_TRANSFORM_MODE_BILLBOARD,
   PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD
} Particle_Transform_Mode;


typedef enum {
   PARTICLE_SPAWN_SHAPE_DIRECTION,
   PARTICLE_SPAWN_SHAPE_SPHERE,
   PARTICLE_SPAWN_SHAPE_CONE,
} Particle_Spawn_Shape;


#define MAX_PARTICLES 5*200
// #define MAX_PARTICLES 1

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
         float min, max;
      } gravity;

      struct {
         float min, max;
      } drag;

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

   struct {
      float speed_scale;  // additional length per unit of speed. 0 = no speed stretching
      float length_scale; // base length multiplier. 1.0 = square, 2.0 = twice as long as wide
                          //
                          // final_length = size * (length_scale + speed_scale * |velocity|)
                          // final_width  = size  (unchanged)
                          //
                          // defaults: length_scale=1, speed_scale=0
      Particle_Transform_Mode value;
   } transform_mode;

   struct {
      Vector3 direction;
      float   half_angle;
      Particle_Spawn_Shape value;
   } spawn_shape;

   ZString texture_path;



   Particle particles[MAX_PARTICLES];

   uint particle_count;
   bool is_simulation_space_local;
   Transform transform;
} Particle_System;


static const Particle_System particle_system_default = {
   .duration = 5.0f,
   .looping = true,
   .emitter_age = 0.0f,

   .spawn_rate        = 10.0f,
   .spawn_accumulator = 0.0f,

   // start ranges
   .start = {
      .lifetime = { .min = 1.0f,   .max = 1.0f },
      .speed    = { .min = 0.0f,   .max = 0.0f },
      .size     = { .min = 1.0f,   .max = 1.0f },
      .gravity  = { .min = 0.0f,   .max = 0.0f },
      .drag     = { .min = 0.025f, .max = 0.025f },

      .color    = { .from = {1,1,1,1}, .to = {1,1,1,1} },
      .rotation = { .from = {0,0,0},   .to = {0,0,0}   },
   },

   .over_lifetime = {
      .color = { .values = {{0}}, .timings = {0}, .count = 0 },
      .rotation = {0,0,0},
      .size = {1,1,1}
   },

   .transform_mode = {
      .speed_scale  = 1.0f,
      .length_scale = 1.0f,
      .value = PARTICLE_TRANSFORM_MODE_BILLBOARD,
   },

   .spawn_shape = {
      .direction  = {0,1,0},
      .half_angle = DEG2RAD * 45,
      .value = PARTICLE_SPAWN_SHAPE_DIRECTION,
   },

   // texture
   .texture_path = "",

   // runtime state
   .particles = {0}, // zero‐initializes entire array
   .particle_count = 0,
   .is_simulation_space_local = false,
   .transform = {
      .translation = {0,0,0},
      .rotation    = {0,0,0,1},
      .scale       = {1,1,1}
   }
};


typedef struct {
   bool loaded;
   Model model;
   Scene_Node nodes[MAX_PARTICLES];
} Particle_System_Render_Resources;


Vector3 overload lerp(Vector3 a, Vector3 b, float t) {
   return Vector3Lerp(a, b, t);
}


Color overload lerp(Color a, Color b, float t) {
   return Vector4Lerp(a, b, t);
}


Color internal sample_color_gradient(const Color *colors, const float *timings, uint count, float t) {
   // before first key
   if (count < 1) {
      return vector4(1);
   }

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
         // local_t *= local_t;
         float new_local_t = smoothstep(0.0, 1.0, local_t);

         return lerp(colors[i], colors[i + 1], new_local_t);

      }
   }

   return colors[count - 1];
}

typedef struct {
   Vector3 position;
   Vector3 direction; // normalized
} Particle_Spawn_Result;


Particle_Spawn_Result particle_cone_spawn(Vector3 direction, float half_angle) {
   // Pick a random direction within half_angle radians of 'direction'
   // half_angle = 0     straight line
   // half_angle = PI/2  hemisphere

   // Random azimuth full circle
   float phi = random_float() * 2.0f * PI;

   //
   // Random polar angle within cone for area-uniform distribution.
   //
   // This is about solid angle uniformity because not every angle is created equal in regards to area.
   // Naive approach would cause clustering seen here: https://youtu.be/uehgVIWXdlk?si=nXZ5yhQABq7xUD6w
   //
   // float theta = random_float() * half_angle; // Naive aproach
   //
   float theta = acosf(1.0f - random_float() * (1.0f - cosf(half_angle)));

   // Build direction from spherical coordinates
   Vector3 local_direction = {
       sinf(theta) * cosf(phi),
       sinf(theta) * sinf(phi),
       cosf(theta),
   };

   // Rotate local_direction +Z to match 'direction'
   Vector3 z = {0, 0, 1};
   Quaternion rotation = QuaternionFromVector3ToVector3(z, normalize(direction));

   // TODO: Add support to spawn from volume or base radius
   return (Particle_Spawn_Result){
       .position = {0},
       .direction = Vector3RotateByQuaternion(local_direction, rotation),
   };
}

// Return stub instead of null no need to check pointer
Particle* spawn_particle(Particle_System *ps, Vector3 emitter_position, float starting_age) {
   for (uint i = 0; i < MAX_PARTICLES; i++) {
      Particle *p = &ps->particles[i];
      if (p && p->alive) {
         continue;
      }

      // Randomize within ranges
      float lifetime = random_float(ps->start.lifetime.min, ps->start.lifetime.max);
      float speed    = random_float(ps->start.speed   .min, ps->start.speed   .max);
      float size     = random_float(ps->start.size    .min, ps->start.size    .max);
      float gravity  = random_float(ps->start.gravity .min, ps->start.gravity .max);
      float drag     = random_float(ps->start.drag    .min, ps->start.drag    .max);
      float t_color  = random_float(0.0f, 1.0f);

      // Pick random value per axis between from and to
      Vector3 rotation = {
         random_float(ps->start.rotation.from.x, ps->start.rotation.to.x),
         random_float(ps->start.rotation.from.y, ps->start.rotation.to.y),
         random_float(ps->start.rotation.from.z, ps->start.rotation.to.z),
      };
      Particle_Spawn_Result result = {0};

      switch (ps->spawn_shape.value) {

      case PARTICLE_SPAWN_SHAPE_DIRECTION:
      default: {
         result.direction = ps->spawn_shape.direction;
         break;
      }
      case PARTICLE_SPAWN_SHAPE_CONE: {
         result = particle_cone_spawn(ps->spawn_shape.direction, ps->spawn_shape.half_angle);
         break;
      }
      case PARTICLE_SPAWN_SHAPE_SPHERE: {
         //
         // Random direction uniform sphere so particles spread in all directions
         // if we want flat emission (ground effect) we need to zero out the y component
         // Actually just any flatten effect on any plane is just a projection away
         // TODO: uniform distribution wrt solid angle
         //
         float theta = random_float(0.0f, 2.0f * PI);
         float phi = random_float(0.0f, PI);
         Vector3 direction = {
             sinf(phi) * cosf(theta),
             sinf(phi) * sinf(theta),
             cosf(phi),
         };
         result.direction = direction;
         break;
      }
      };

      result.direction = Vector3RotateByQuaternion(result.direction, ps->transform.rotation);

      *p = (Particle) {
         .position = result.position,
         .velocity = mul(speed, result.direction),
         .spawned_position = ps->transform.position,

         .gravity  = gravity,
         .drag     = drag,
         .size = {
            .start   = vector3(size),
            .current = p->size.start,
         },

         .color = {
            .start   = lerp(ps->start.color.from, ps->start.color.to, t_color),
            .current = p->color.start,
         },

         .rotation = {
            .start   = rotation,
            .current = rotation,
         },

         .age      = starting_age,
         .lifetime = lifetime,
         .alive    = true,
      };

      return p;
   }

   trace_debug("If path is taken triggers we probably hit max capacity (%d) on particles per particle_system", MAX_PARTICLES);
   static Particle stub = {0};
   // silently drop by returning stub particle if full
   return &stub;
}


// We take Particle_Transform_Mode instead of using the one in Particle_System *ps because we might wanna recurse with a different mode
Transform particle_transform(Particle_Transform_Mode mode, const Particle *p, const Particle_System *ps, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   Transform transform = transform_identity;

   Vector3 emitter_world_position = ps->is_simulation_space_local ? ps->transform.position : p->spawned_position;
   Vector3 particle_world_position = mul(ps->transform.scale, p->position);
   if (ps->is_simulation_space_local) {
      particle_world_position = Vector3RotateByQuaternion(particle_world_position, ps->transform.rotation);
   }
   particle_world_position = add(emitter_world_position, particle_world_position);

   switch (mode) {
   // Any billboardind needs to be done considering camera and particle world-space coordinates
   case PARTICLE_TRANSFORM_MODE_BILLBOARD:
   default: {
      // Rotation
      Quaternion face = billboard_rotation(true, particle_world_position, camera_position, camera_forward, camera_right, camera_up);
      // Quaternion spin = QuaternionFromAxisAngle(camera_forward, p->rotation.current.z * DEG2RAD);
      float pitch = p->rotation.current.y, yaw = p->rotation.current.x, roll = p->rotation.current.z;
      Quaternion spin = QuaternionFromEuler(pitch, yaw, roll);
      Quaternion rotation = QuaternionMultiply(face, spin);

      // Transform
      transform = (Transform) {
          .translation = p->position,
          .rotation = rotation,
          .scale = p->size.current,
      };
      break;
   }

   case PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD: {
      float speed = length(p->velocity);
      const bool point_aligned = true;

      static bool use_stretched_priority_camera = true;
      static f64 time_previous = 0;
      if (time_previous != time_delta() && is_button_pressed(BUTTON_8)) {
         use_stretched_priority_camera = !use_stretched_priority_camera;
         trace_info("time_previous = %f, use_stretched_priority_camera = %d", time_previous, use_stretched_priority_camera);
      }
      time_previous = time_delta();

      Quaternion face = stretched_billboard_rotation(point_aligned, use_stretched_priority_camera, particle_world_position, p->velocity, camera_position, camera_forward, camera_right, camera_up);
      {
         // float pitch = p->rotation.current.y, yaw = p->rotation.current.x, roll = p->rotation.current.z;
         // Quaternion spin = QuaternionFromEuler(pitch, yaw, roll);
         // Quaternion rotation = QuaternionMultiply(face, spin);
      }
      Quaternion rotation = face;
      float stretch =  ps->transform_mode.length_scale + ps->transform_mode.speed_scale*speed;
      Vector3 scale =  p->size.current;
      // scale.y       *= stretch;
      scale.y = p->size.current.y * ps->transform_mode.length_scale
                +           speed * ps->transform_mode.speed_scale;


      transform = (Transform) {
         .translation = p->position,
         .rotation    = rotation,
         .scale       = scale,
      };
      break;
   }
   }

   transform.position = particle_world_position;
   transform.scale    = mul(ps->transform.scale, transform.scale);

   return transform;
}

Transform overload particle_transform(const Particle *p, const Particle_System *ps, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   return particle_transform(ps->transform_mode.value, p, ps, camera_position, camera_forward, camera_right, camera_up);
}

void internal update_particle(Particle *p, const Particle_System *ps, float dt) {
   if (!p || !p->alive) {
      return;
   }

   p->age += dt;

   if (p->age >= p->lifetime) {
      p->alive = false;
      return;
   }

   float t = p->age / p->lifetime;

   // Velocity update (before position)
   // Gravity
   p->velocity = add(p->velocity, mul(default_gravity, p->gravity * dt));

   // Drag
   p->velocity = mul(powf(1.0f - p->drag, dt), p->velocity);

   // Position from velocity (after velocity is updated by forces)
   p->position = add(p->position, mul(p->velocity, dt));

   // color, rotation, size over lifetime
   p->color.current = mul(
      p->color.start,
      sample_color_gradient(ps->over_lifetime.color.values, ps->over_lifetime.color.timings, ps->over_lifetime.color.count, t)
   );

   // over_lifetime.rotation is radians per second, additive per frame
   p->rotation.current = add(p->rotation.current, mul(ps->over_lifetime.rotation, dt));

   p->size.current = lerp(p->size.start, mul(p->size.start, ps->over_lifetime.size), t);
}


void update_particle_system(Particle_System *ps, Vector3 emitter_position, float dt) {
   // Simulate live particles. Do this first because if we spawn particles and then update
   // Every new particle would start with at least 'dt' of age which would could synchronization problems.
   {
      ps->particle_count = 0;
      for (uint i = 0; i < MAX_PARTICLES; i++) {
         Particle *p = &ps->particles[i];
         if (p->alive) {
            update_particle(p, ps, dt);
            ps->particle_count += 1;
         } else {
            *p = (Particle){0};
         }
      }
   }

   // Advance emitter clock
   {
      ps->emitter_age += dt;
      if (ps->looping && ps->emitter_age >= ps->duration) {
         ps->emitter_age -= ps->duration;
         // -= instead of = 0 so we don't lose overshoot
         // e.g. duration = 1s and dt pushed us to 1.003s then emitter_age becomes 0.003s
         // keeping that tiny remainder means emission stays perfectly timed
      }
   }

   // Emit new particles
   {
      bool emitting = ps->looping || (ps->emitter_age < ps->duration);
      if (emitting) {
         // Accumulator is needed to avoid spawning 0 new particles every frame
         ps->spawn_accumulator += ps->spawn_rate * dt;
         while (ps->spawn_accumulator >= 1.0f) {
            ps->spawn_accumulator -= 1.0f;

            float starting_age = ps->spawn_accumulator / ps->spawn_rate; // convert back to seconds
            // trace_info("starting_age = %f", starting_age);
            // starting_age = 0; // @remove
            Particle *p = spawn_particle(ps, emitter_position, starting_age);

            // Not updating when just spawned will cause popping related to over_lifetime fields.
            // Also, we update with no time passed(dt=0) because it was just born.
            update_particle(p, ps, 0);
         }
      }
   }

}


void setup_particle_render_state(void) {
   // true ? nullptr : glEnable(GL_FRAMEBUFFER_SRGB);

   // No backface culling quads are single sided
   // but we want both sides visible if camera goes behind
   glDisable(GL_CULL_FACE);

   glEnable(GL_BLEND);
   // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
   glBlendEquation(GL_FUNC_ADD); // GL_FUNC_SUBTRACT


   // Particles don't write depth
   const bool depth_fiddling = true;
   if (depth_fiddling) {
      glDepthMask(GL_FALSE);
      glEnable(GL_DEPTH_TEST);
      glDepthFunc(GL_LESS);
   }


   // trace_info("simulation_speed = %f", simulation_speed);
   int values[]            = { GL_SRC_ALPHA,   GL_ONE_MINUS_SRC_ALPHA,   GL_ZERO,   GL_ONE,   GL_SRC_COLOR,   GL_ONE_MINUS_SRC_COLOR,   GL_DST_COLOR,   GL_ONE_MINUS_DST_COLOR,   GL_DST_ALPHA,   GL_ONE_MINUS_DST_ALPHA,   GL_CONSTANT_COLOR,   GL_ONE_MINUS_CONSTANT_COLOR,   GL_CONSTANT_ALPHA,   GL_ONE_MINUS_CONSTANT_ALPHA };
   ZString values_string[] = {"GL_SRC_ALPHA", "GL_ONE_MINUS_SRC_ALPHA", "GL_ZERO", "GL_ONE", "GL_SRC_COLOR", "GL_ONE_MINUS_SRC_COLOR", "GL_DST_COLOR", "GL_ONE_MINUS_DST_COLOR", "GL_DST_ALPHA", "GL_ONE_MINUS_DST_ALPHA", "GL_CONSTANT_COLOR", "GL_ONE_MINUS_CONSTANT_COLOR", "GL_CONSTANT_ALPHA", "GL_ONE_MINUS_CONSTANT_ALPHA"};
   static int current_1 = 0;
   static int current_2 = 1;
   bool pressed_button = false;

   if (is_button_pressed(BUTTON_1)) {
      current_1 = (current_1 + 1) % count_of(values);
      pressed_button = true;
   }
   if (is_button_pressed(BUTTON_2)) {
      current_2 = (current_2 + 1) % count_of(values);
      pressed_button = true;
   }

   if (is_button_pressed(BUTTON_3)) {
      current_1 = 0; current_2 = 1;
      pressed_button = true;
   }

   if (is_button_pressed(BUTTON_4)) {
      current_1 = 3; current_2 = 1;
      pressed_button = true;
   }

   if (pressed_button) {
      trace_info("glBlendFunc(%s, %s)", values_string[current_1], values_string[current_2]);
   }

   glBlendFunc(values[current_1], values[current_2]);
}


void draw_particle_system(Particle_System *ps, Particle_System_Render_Resources *ps_resources, Vector3 position, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   if (0 == ps->spawn_rate) {
      trace_warn("Potentially uninitiated particle system. Refusing to render.");
      return;
   }

   if (!ps_resources->loaded) {
      ps_resources->loaded = true;
      // quad = create_model_from_mesh(generate_quad_mesh(1.0f, 1.0f));
      ps_resources->model = create_model_from_mesh(
         generate_quad_mesh(1.0f, 1.0f),
         ps->texture_path, nullptr, nullptr, nullptr
      );

      // first node owns the mesh
      ps_resources->nodes[0] = create_scene_node(&ps_resources->model, transform_identity);
      // hide the first one too until a particle claims it
      update_color_tint(ps_resources->nodes[0], vector4(0.f));


      // rest share the same renderable, start invisible
      for (uint i = 1; i < MAX_PARTICLES; i++) {
         ps_resources->nodes[i] = create_scene_node(ps_resources->nodes[0], transform_identity);
         update_color_tint(ps_resources->nodes[i], vector4(0.f));
      }

   }

   static float simulation_speed = 1.;

   if (is_button_pressed(BUTTON_RIGHT)) {
      simulation_speed = 1.;
      trace_info("simulation_speed = %f", simulation_speed);
   } else if (is_button_pressed(BUTTON_LEFT)) {
      simulation_speed = 0.05;
      trace_info("simulation_speed = %f", simulation_speed);
   }


   update_particle_system(ps, position, time_delta() * simulation_speed);
   // Sort particles by distance to camera, farthest first
   for (uint i = 0; i < MAX_PARTICLES - 1; i++) {
      for (uint j = i + 1; j < MAX_PARTICLES; j++) {
         Particle *a = &ps->particles[i];
         Particle *b = &ps->particles[j];
         if (!a->alive || !b->alive) {
            continue;
         }
         float da = Vector3DistanceSqr(a->position, camera_position);
         float db = Vector3DistanceSqr(b->position, camera_position);
         if (da < db) {
            Particle tmp = *a;
            *a = *b;
            *b = tmp;
         }
      }
   }

   for (uint i = 0; i < MAX_PARTICLES; i++) {
      Particle *p = &ps->particles[i];
      Scene_Node node = ps_resources->nodes[i];

      if (!p->alive) {
         // XXX: hide dead slot zero alpha makes it invisible
         update_color_tint(node, vector4(1., 0., 0., 0.));
         Transform t = transform_identity;
         t.scale = vector3(0);
         update_transform(node, t);
         continue;
      }

      // DIRTY
      update_rendering_mode(node, 2);

      Transform transform = particle_transform(p, ps, camera_position, camera_forward, camera_right, camera_up);
      update_transform(node, transform);

      update_color_tint(node, p->color.current);
   }
}


void draw_quad_test(Vector3 unit_position, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   static bool loaded = false;
   static Model quad = {0};
   static Scene_Node quad_node;


   static Texture flare_texture = {0};

   if (!loaded) {
      loaded = true;
      // quad = create_model_from_mesh(generate_quad_mesh(1.0f, 1.0f));

      flare_texture = create_texture_from_filepath("res/textures/vfx/Flare00.PNG");
      quad = create_model_from_mesh(
         generate_quad_mesh(1.0f, 1.0f),
         "res/textures/vfx/Flare00.PNG", nullptr, nullptr, nullptr
      );

      quad = create_cube_model(
         "res/textures/vfx/Flare00.PNG",
         nullptr,
         nullptr,
         nullptr
      );
      

      quad_node = create_scene_node(&quad);

      update_color_tint(quad_node, vector4(1.f));
      update_rendering_mode(quad_node, 2);
   }


   bind_texture(flare_texture, 3);

   {
      // Rotation
      Quaternion face = billboard_rotation(true, unit_position, camera_position, camera_forward, camera_right, camera_up);

      // Transform
      Transform t = {
         .translation = unit_position,
         .rotation = transform_identity.rotation,
         .scale = vector3(100.),
      };
      
      update_transform(quad_node, t);
   }
}


void draw_spark_vfx(Vector3 unit_position, Vector3 unit_direction, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   if (length(unit_direction) < 1e-5f) {
      unit_direction = vector3(1, 0, 0);
   }
   unit_direction = normalize(unit_direction);
   trace_debug("unit_direction = %f, %f, %f", unit_direction.x, unit_direction.y, unit_direction.z);

   static struct {
      Particle_System particle_system;
      Particle_System_Render_Resources render_resources;

   } vfx[1] = {0};

   static bool loaded = false;
   if (!loaded) {
      loaded = true;
      Color particle_color = vector4(0.);
      particle_color = mul(2.0, vector4(255/255., 80/255., 25/255., 1.));

      vfx[count_of(vfx)-1].particle_system = (Particle_System) {
         // emitter
         .duration = 5.,
         .looping = true,

         // spawn
         .spawn_rate = 50,
         .start = {
            .lifetime = {
               .min = 3,
               .max = 8
            },
            .gravity = {
               .min = 2, .max = 6
               // .min = 0, .max = 0
            },
            .speed = {
               .min = 5,
               .max = 9,
            },
            .size = {
               .min = 0.01,
               .max = 0.15
            },
            .color = {
               .from = particle_color,
               .to   = particle_color
            },
            .rotation = {
               .from = vector3(0),
               .to   = vector3(0),
            },
         },
         .over_lifetime = {
            .color = {
               .values  = { vector4(1, 1, 1, 1), vector4(1, 1, 1, 0)},
               .timings = { 0.0,                 0.9,               },
               .count   = 2,
            },
            .size = vector3(1),
         },
         .transform_mode = {
            .speed_scale = 0.05,
            .length_scale = 2,
            .value = PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD,
            // .value = PARTICLE_TRANSFORM_MODE_BILLBOARD,
         },
         .spawn_shape = {
            .direction  = normalize(vector3(1, 0, 0)),
            .half_angle = DEG2RAD * 25,
            .value      = PARTICLE_SPAWN_SHAPE_CONE,
         },
         .is_simulation_space_local = false,
         .texture_path = "res/textures/vfx/Flame02_Rotated.png",

         .transform = {
            .rotation = QuaternionFromVector3ToVector3(vector3(1, 0, 0), normalize(vector3(1, 0,1))),
            .scale = vector3(10.),
         },
      };

      // @remove
      auto debug_particle_system = (Particle_System) {
         // emitter
         .duration = 5.,
         .looping = true,

         // spawn
         .spawn_rate = 3,
         .start = {
            .lifetime = {
               .min = 40,
               .max = 40,
            },
            .gravity = {
               .min = 0.25, .max = 0.25,
               // .min = 2 + 0.25, .max = 2 + 0.25,
               // .min = 0, .max = 0
            },
            .speed = {
               .min = 1,
               .max = 1
            },
            .size = {
               .min = 1,
               .max = 1
            },
            .color = {
               .from = particle_color,
               .to   = particle_color
            },
            .rotation = {
               .from = vector3(0),
               .to   = vector3(0),
            },
         },
         .over_lifetime = {
            .color = {
               .values  = {},
               .timings = {},
               .count   = 0
            },
            .size = vector3(1),
         },
         .transform_mode = {
            .speed_scale = 0.9,
            .length_scale = 2,
            .value = PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD,
         },
         .is_simulation_space_local = false,
         // .texture_path = "res/textures/vfx/Flame02.png",
         .texture_path = "res/textures/vfx/up-arrow2.png",
      };
   }

   setup_particle_render_state();
   for (int idx = 0; idx < count_of(vfx); idx++) {
      Particle_System *ps = &vfx[idx].particle_system;
      if (QuaternionEquals(ps->transform.rotation, QuaternionZeros)) {
          ps->transform.rotation = transform_identity.rotation;
      }
      ps->transform.position = unit_position;
      auto rotation = QuaternionFromVector3ToVector3(vector3(1, 0, 0), normalize(mul(-1, unit_direction)));
      ps->transform.rotation = rotation;
      draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
   }
}


void draw_vfx(Vector3 unit_position, Vector3 unit_direction, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {


   static struct {
      Particle_System particle_system;
      Particle_System_Render_Resources render_resources;

   } vfx[3] = {0};

   static bool loaded = false;
   if (!loaded) {
      loaded = true;
      Color particle_color = vector4(0.);
      // particle_color = vector4(50/255., 72/255., 103/255., 1.);
      particle_color = mul(2.0, vector4(50/255., 72/255., 103/255., 1.));

      vfx[count_of(vfx)-1].particle_system = (Particle_System){
         // emitter
         .duration = 1.,
         .looping = true,

         // spawn
         .spawn_rate = 1,
         .start = {
            .lifetime = {
               .min = 1.,
               .max = 1.
            },
            .size = {
               .min = 7 * 5,
               .max = 7 * 5
            },
            .color = {
               .from = particle_color,
               .to   = particle_color
            },
            .rotation = {
               .from = vector3(0),
               .to   = vector3(0),
            },
         },
         .over_lifetime = {
            .color = {
               .values  = {vector4(1, 1, 1, 0), vector4(1), vector4(1, 1, 1, 0)},
               .timings = {1.5/100., 52.9/100., 97.6/100.},
               .count   = 3
            },
            .size = vector3(1.),
         },
         .texture_path = "res/textures/vfx/Swirl01.png",
      };

      particle_color = vector4(17/255., 24/255., 34/255., 1.);
      vfx[1].particle_system = (Particle_System) {
         // emitter
         .duration = 1.,
         .looping = true,
         // .gravity = 1000.,

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
         .texture_path = "res/textures/vfx/Swirl01.png",
      };

      particle_color = vector4(0, 0, 0, 1.);
      vfx[0].particle_system = (Particle_System) {

         .spawn_accumulator = 1.0f, // "prewarm" trigger spawn immediately
         // emitter
         .duration = 1.,
         .looping = true,

         // spawn
         .spawn_rate = 1,
         .start = {
            .lifetime = {
               .min = 1.,
               .max = 1.
            },
            .size = {
               .min = 7 * 10,
               .max = 7 * 10
            },
            .speed = {
               .min = 1.,
               .max = 1.
            },
            .color = {
               .from = particle_color,
               .to = particle_color
            },
         },
         .over_lifetime = {
            .color = {
               .values  = {},
               .timings = {},
               .count   = 0
            },
            .size = vector3(1.f),
         },
         .particles = {0},
         .texture_path = "res/textures/vfx/Flare00.png",
      };
   }

   setup_particle_render_state();
   for (int idx = 0; idx < count_of(vfx); idx++) {
      // draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
   }

   draw_spark_vfx(unit_position, unit_direction, camera_position, camera_forward, camera_right, camera_up);
}



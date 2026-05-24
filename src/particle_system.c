constexpr Vector3 default_gravity = {0, -9.81f, 0};

typedef struct {
   //
   // These are in System Space
   //
   Vector3 position;
   Vector3 velocity;

   // World space emitter transform at the time of spawn is not local space.
   // Updated always is local
   Transform emitter_transform;

   float drag;    // Drag per second. Default to 0.25f // 0=no drag, 1=instant stop per second
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
      #define PARTICLE_MAX_KEYS 8

      struct {
         Color values [PARTICLE_MAX_KEYS];
         float timings[PARTICLE_MAX_KEYS];
         uint  count;
      } color;

      Vector3 rotation;

      struct {
          Vector3 values[PARTICLE_MAX_KEYS];
          float timings[PARTICLE_MAX_KEYS];
          int   count;
      } size;

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
   bool is_local_simulation_space;
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
      .size = { .values = {{0}}, .timings = {0}, .count = 0 },
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
   .is_local_simulation_space = false,
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
   int instance_rendering_mode;
   float hdr_intensity, hdr_alpha_compose, hdr_alpha_coefficient_intensity;
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

Vector3 sample_size_curve(const Vector3 *values, const float *timings, uint count, float t) {
   if (count < 1)
      return vector3(1);
   if (t <= timings[0])
      return values[0];
   if (t >= timings[count - 1])
      return values[count - 1];

   for (uint i = 0; i < count - 1; i++) {
      float t0 = timings[i];
      float t1 = timings[i + 1];
      if (t >= t0 && t <= t1) {
         float local_t = (t - t0) / (t1 - t0);
         local_t = smoothstep(0.0f, 1.0f, local_t);
         return lerp(values[i], values[i + 1], local_t);
      }
   }

   return values[count - 1];
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
       .direction = normalize(Vector3RotateByQuaternion(local_direction, rotation)),
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

      Vector4 color = lerp(ps->start.color.from, ps->start.color.to, t_color);

      // Pick random value per axis between from and to
      Vector3 rotation = {
         random_float(ps->start.rotation.from.x, ps->start.rotation.to.x),
         random_float(ps->start.rotation.from.y, ps->start.rotation.to.y),
         random_float(ps->start.rotation.from.z, ps->start.rotation.to.z),
      };
      Particle_Spawn_Result result = {0};

      switch (ps->spawn_shape.value) {

      // Change this to ray instead or line segment
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

      // result.direction = normalize(Vector3RotateByQuaternion(result.direction, ps->transform.rotation));

      *p = (Particle) {
         .position = result.position,
         .velocity = mul(speed, result.direction),
         .emitter_transform = ps->transform,

         .gravity  = gravity,
         .drag     = drag,
         .size = {
            .start   = vector3(size),
            .current = vector3(size),
         },

         .color = {
            .start   = color,
            .current = color,
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
Transform calculate_particle_transform(Particle_Transform_Mode mode, const Particle *p, const Particle_System *ps, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   const Quaternion spin = QuaternionFromEuler(p->rotation.current.pitch, p->rotation.current.yaw, p->rotation.current.roll);
   const Transform particle_local_transform = {
      .position = p->position,
      .scale    = p->size.current,
      // .rotation = transform_identity.rotation,
      .rotation = spin,
   };

   const Transform emitter_transform = p->emitter_transform;
   // In unity, if simulation is not local live particles doesn't get rotated/translated by current emitter position.
   // But for some reason they scale by current emitter scale!? I believe it makes more sense to only scale if simulation is local
   Transform particle_transform = TransformCombine(emitter_transform, particle_local_transform);

   switch (mode) {
   // Any billboardind needs to be done considering camera and particle world-space coordinates
   case PARTICLE_TRANSFORM_MODE_BILLBOARD:
   default: {
      Quaternion face = billboard_rotation(true, particle_transform.position, camera_position, camera_forward, camera_right, camera_up);
      Quaternion rotation = QuaternionMultiply(face, spin);
      particle_transform.rotation = rotation;
      break;
   }

   case PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD: {
      const bool point_aligned = true;

      static bool use_stretched_priority_camera = false;
      {
         static f64 time_previous = 0;
         // HACK: Running once perframe using delta with is fixed per frame
         if (time_previous != time_delta() && is_button_pressed(BUTTON_8)) {
            use_stretched_priority_camera = !use_stretched_priority_camera;
            trace_info("time_previous = %f, use_stretched_priority_camera = %d", time_previous, use_stretched_priority_camera);
         }
         time_previous = time_delta();
      }


      const Vector3 world_position = particle_transform.position;
      const Vector3 world_velocity = rotate(mul(p->velocity, emitter_transform.scale), emitter_transform.rotation);
      Quaternion face = stretched_billboard_rotation(point_aligned, use_stretched_priority_camera, world_position, world_velocity, camera_position, camera_forward, camera_right, camera_up);

      // TODO: Enable rotation with streatch and test if this is correct
      if (false) {
         float pitch = p->rotation.current.y, yaw = p->rotation.current.x, roll = p->rotation.current.z;
         Quaternion spin = QuaternionFromEuler(pitch, yaw, roll);
         Quaternion rotation = QuaternionMultiply(face, spin);
      }

      float speed = length(world_velocity);

      Quaternion rotation = face;
      Vector3 scale =  particle_transform.scale;
      // scale.y *= (ps->transform_mode.length_scale + speed * ps->transform_mode.speed_scale);
      scale.y = scale.y * ps->transform_mode.length_scale
                + speed * ps->transform_mode.speed_scale;

      particle_transform.scale    = scale;
      particle_transform.rotation = rotation;

      break;
   }
   }

   return particle_transform;
}


Transform overload calculate_particle_transform(const Particle *p, const Particle_System *ps, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   return calculate_particle_transform(ps->transform_mode.value, p, ps, camera_position, camera_forward, camera_right, camera_up);
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

   if (ps->is_local_simulation_space) {
      p->emitter_transform = ps->transform;
   }

   float t = p->age / p->lifetime;

   // Simulation is in local space but position will be rotated by spawned_transform at render.
   // counter-rotate now so it comes out pointing world-down after that transform is applied.
   const Vector3 gravity = rotate(default_gravity, QuaternionInvert(p->emitter_transform.rotation));

   // Gravity
   p->velocity = add(p->velocity, mul(gravity, p->gravity * dt));

   // Drag
   p->velocity = mul(powf(1.0f - p->drag, dt), p->velocity);

   // Position from velocity (after velocity is updated by forces)
   p->position = add(p->position, mul(p->velocity, dt));

   //
   // Overlifetime color, rotation, size.
   //
   p->color.current = mul(
      p->color.start,
      sample_color_gradient(ps->over_lifetime.color.values, ps->over_lifetime.color.timings, ps->over_lifetime.color.count, t)
   );


   // over_lifetime.rotation is radians per second, additive per frame
   p->rotation.current = add(p->rotation.current, mul(ps->over_lifetime.rotation, dt));

   p->size.current = mul(
      p->size.start,
      sample_size_curve(ps->over_lifetime.size.values, ps->over_lifetime.size.timings, ps->over_lifetime.size.count, t)
   );
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
            // starting_age = 0;
            Particle *p = spawn_particle(ps, emitter_position, starting_age);

            // Not updating when just spawned will cause popping related to over_lifetime fields.
            // Also, we update with no time passed(dt=0 / starting_age) because it was just born.
            // update_particle(p, ps, starting_age);
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

   if (QuaternionEquals(ps->transform.rotation, QuaternionZeros)) {
       ps->transform.rotation = transform_identity.rotation;
   }

   if (!ps_resources->loaded) {
      ps_resources->loaded = true;
      // quad = create_model_from_mesh(generate_quad_mesh(1.0f, 1.0f));
      ps_resources->model = create_model_from_mesh(
         generate_quad_mesh(1.0f, 1.0f),
         ps->texture_path, nullptr, nullptr, nullptr
      );

      const float hdr_linear_intensity            = powf(2.0f, ps_resources->hdr_intensity);
      const float hdr_alpha_compose               = saturate(ps_resources->hdr_alpha_compose);
      const float hdr_alpha_coefficient_intensity = powf(2.0f, ps_resources->hdr_alpha_coefficient_intensity);

      trace_info("ps_resources->hdr_intensity = %f hdr_linear_intensity = %f for %s", ps_resources->hdr_intensity, hdr_linear_intensity, ps->texture_path);

      for (uint i = 0; i < MAX_PARTICLES; i++) {
         if (0 == i) {
            // First node owns mesh
            ps_resources->nodes[i] = create_scene_node(&ps_resources->model);
            update_renderable_render_state(ps_resources->nodes[i], RENDER_STATE_VFX);
         } else {
            // Instances from first
            ps_resources->nodes[i] = create_scene_node(ps_resources->nodes[0]);
         }

         // Hide until a particle claims it
         update_color_tint(ps_resources->nodes[i], vector4(0.f));
         update_custom_data(ps_resources->nodes[i], vector4(hdr_linear_intensity, hdr_alpha_compose, hdr_alpha_coefficient_intensity, 0), vector4(0));
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

   uint live_count = 0;
   for (uint i = 0; i < MAX_PARTICLES; i++) {
      if (ps->particles[i].alive) {
         Particle tmp = ps->particles[live_count];
         ps->particles[live_count] = ps->particles[i];
         ps->particles[i] = tmp;
         live_count++;
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
      update_rendering_mode(node, ps_resources->instance_rendering_mode);

      Transform transform = calculate_particle_transform(p, ps, camera_position, camera_forward, camera_right, camera_up);
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

void draw_impact_vfx(Vector3 unit_position, Vector3 unit_direction, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   static struct {
      Particle_System particle_system;
      Particle_System_Render_Resources render_resources;
   } vfx[4] = {0};


   static bool loaded = false;
   if (!loaded) {
      loaded = true;
      for (uint idx = 0; idx < count_of(vfx); idx++) {
         vfx[idx].particle_system = particle_system_default;
         vfx[idx].particle_system.duration = 5;
         vfx[idx].particle_system.transform.scale = vector3(20);
         vfx[idx].render_resources.instance_rendering_mode = 3 + 2;
      }
      Particle_System *ps = &vfx[0].particle_system;
      auto *resources = &vfx[0].render_resources;

      ps->duration   = 0.2;
      ps->looping    = true;
      ps->spawn_rate = 10;

      ps->start.lifetime.min = 0.25;
      ps->start.lifetime.max = 0.25;

      ps->start.size.min = 7;
      ps->start.size.max = 7;

      auto color = mul(1./255., vector3(255, 204, 140));
      ps->start.color.from = vector4(color, 1);
      ps->start.color.to   = vector4(color, 1);
      ps->transform.scale  = vector3(5);
      ps->over_lifetime.size = (type_of(ps->over_lifetime.size)) {
         .values  = { vector3(1.0f/3.0f), vector3(0.8), vector3(1.0) },
         .timings = { 0.0f, 0.9f, 1.0f },
         .count   = 3
      };

      // ps->texture_path = "res/textures/vfx/Flare00.png";
      ps->texture_path = "./res/textures/vfx/glow_point1_blue 1_1.png";
   }
   // auto tex = create_texture("./res/textures/vfx/glow_point1_blue 1_1.png");
   // debug_print_texture(tex);
   // wait_for_enter_on_terminal();

   for (int idx = 0; idx < count_of(vfx); idx++) {
      Particle_System *ps = &vfx[idx].particle_system;
      if (!file_exists(ps->texture_path)) {
         continue;
      }

      ps->transform.position = add(unit_position, vector3(0, 0, 0));


      Vector3 particle_direction = mul(-1, unit_direction);

      // Tilt a bit. cross(direction, world_up) gives the right axis to rotate around
      Vector3 right = normalize(cross(particle_direction, vector3(0, 1, 0)));
      particle_direction = rotate(particle_direction, QuaternionFromAxisAngle(right, DEG2RAD * 45));
      auto target_rotation = QuaternionFromVector3ToVector3(vector3(1, 0, 0), normalize(particle_direction));

      // rotation_speed controls how fast it catches up
      const float rotation_speed = 5.0f;
      ps->transform.rotation = QuaternionSlerp(ps->transform.rotation, target_rotation, clamp(rotation_speed * time_delta(), 0.0f, 1.0f));

      draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
   }
}


void draw_spark_vfx(Vector3 unit_position, Vector3 unit_direction, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   if (length(unit_direction) < 1e-5f) {
      unit_direction = vector3(1, 0, 0);
   } else {
      unit_direction = normalize(unit_direction);
   }

   trace_debug("unit_direction = %f, %f, %f", unit_direction.x, unit_direction.y, unit_direction.z);

   static struct {
      Particle_System particle_system;
      Particle_System_Render_Resources render_resources;

   } vfx[1] = {0};


   static bool loaded = false;
   if (!loaded) {
      loaded = true;
      for (uint idx = 0; idx < count_of(vfx); idx++) {
         vfx[idx].particle_system = particle_system_default;
         vfx[idx].render_resources.instance_rendering_mode = 3;
         vfx[idx].render_resources.hdr_intensity = 7;
      }

      Color particle_color = vector4(0.);
      particle_color = mul(2.0, vector4(255/255., 80/255., 25/255., 1.));
      particle_color.w = 1;

      vfx[count_of(vfx)-1].particle_system = (Particle_System) {
         .texture_path = "res/textures/vfx/Flame02_Rotated.png",
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
               .values  = { vector4(1, 1, 1, 0), vector4(1, 1, 1, 1), vector4(1, 1, 1, 0)},
               .timings = { 0.0,                 0.05,                 0.9,               },
               .count   = 3,
            },
            // .size = particle_system_default.over_lifetime.size,
            .size = {
               .values  = {vector3(1), vector3(0)},
               .timings = {0, 1.f},
               .count   = 2,
            },
         },
         .transform_mode = {
            .speed_scale = 0.065,
            .length_scale = 1.85,
            .value = PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD,
            // .value = PARTICLE_TRANSFORM_MODE_BILLBOARD,
         },
         .spawn_shape = {
            .direction  = normalize(vector3(1, 0, 0)),
            .half_angle = DEG2RAD * (25/2.),
            .value      = PARTICLE_SPAWN_SHAPE_CONE,
         },
         .is_local_simulation_space = false,

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
         .spawn_rate = 0.7,
         .start = {
            .lifetime = {
               .min = 10,
               .max = 10,
            },
            .gravity = {
               // .min = 0.25, .max = 0.25,
               // .min = 0.05, .max = 0.05,
               .min = 0, .max = 0,
            },
            .speed = {
               .min = 0.5,
               .max = 0.5
            },
            .size = {
               .min = 1,
               .max = 1
            },
            .color = {
               .from = vector4(1),
               .to   = vector4(1),
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
            .size = {
               .values  = {},
               .timings = {},
               .count   = 0
            },
         },
         .transform_mode = {
            // .speed_scale = 0.9,
            // .length_scale = 2,
            .speed_scale  = 0.75,
            .length_scale = 0,
            .value = PARTICLE_TRANSFORM_MODE_STRETCHED_BILLBOARD,
            // .value = PARTICLE_TRANSFORM_MODE_BILLBOARD,
         },
         .spawn_shape = {
            .direction  = normalize(vector3(0, 0, 1)),
            // .half_angle = DEG2RAD * 25,
            .value      = PARTICLE_SPAWN_SHAPE_DIRECTION,
         },
         // .texture_path = "res/textures/vfx/Flame02.png",
         .texture_path = "res/textures/vfx/up-arrow2.png",

         .is_local_simulation_space = true,
         .transform = {
            .scale = vector3(20.),
         },
      };
      // vfx[count_of(vfx)-1].particle_system = debug_particle_system;
   }

   // setup_particle_render_state();
   // for (int idx = 0; idx < count_of(vfx); idx++) {
   //    Particle_System *ps = &vfx[idx].particle_system;
   //    ps->transform.position = add(unit_position, vector3(30, 0, 0) );
   //    auto particle_direction = mul(-1, unit_direction);
   //    auto rotation = rotation_from_direction(particle_direction);
   //    ps->transform.rotation = rotation;
   //    draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
   // }
   for (int idx = 0; idx < count_of(vfx); idx++) {
      Particle_System *ps = &vfx[idx].particle_system;
      ps->transform.position = add(unit_position, vector3(0, 0, 0));


      auto particle_direction = mul(-1, unit_direction);

      // Tilt a bit. cross(direction, world_up) gives the right axis to rotate around
      Vector3 right = normalize(cross(particle_direction, vector3(0, 1, 0)));
      particle_direction = rotate(particle_direction, QuaternionFromAxisAngle(right, DEG2RAD * 45));
      auto target_rotation = QuaternionFromVector3ToVector3(vector3(1, 0, 0), normalize(particle_direction));

      // rotation_speed controls how fast it catches up
      const float rotation_speed = 5.0f;
      ps->transform.rotation = QuaternionSlerp(ps->transform.rotation, target_rotation, clamp(rotation_speed * time_delta(), 0.0f, 1.0f));

      draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
   }
}


void draw_vfx(Vector3 unit_position, Vector3 unit_direction, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {

   static struct {
      Particle_System particle_system;
      Particle_System_Render_Resources render_resources;
   } vfx[3] = {0};

   static bool loaded = false;
   const bool is_local_simulation_space = true;
   if (!loaded) {
      loaded = true;
      for (uint idx = 0; idx < count_of(vfx); idx++) {
         vfx[idx].particle_system = particle_system_default;
         vfx[idx].render_resources.instance_rendering_mode = 3;
         vfx[idx].render_resources.hdr_intensity = 2.616925 * 2;
      }
      Color particle_color = vector4(0.);
      // particle_color = vector4(50/255., 72/255., 103/255., 1.);
      particle_color = vector4(50/255., 72/255., 103/255., 1.);
      particle_color.w = 1.f;

      vfx[count_of(vfx)-1].render_resources.instance_rendering_mode += 1;
      vfx[count_of(vfx)-1].render_resources.hdr_intensity = 2.616925 * 2;
      vfx[count_of(vfx)-1].render_resources.hdr_alpha_compose = 1.;
      vfx[count_of(vfx)-1].render_resources.hdr_alpha_coefficient_intensity = 1.15;
      vfx[count_of(vfx)-1].particle_system = (Particle_System) {
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
               .min = 5,
               .max = 5
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
         },
         .transform_mode = particle_system_default.transform_mode,
         .spawn_shape = particle_system_default.spawn_shape,
         .transform = {
            .scale = vector3(7),
            .rotation =  transform_identity.rotation,
         },
         .texture_path = "res/textures/vfx/Swirl01.png",
         .is_local_simulation_space = is_local_simulation_space,
      };

      particle_color = vector4(17/255., 24/255., 34/255., 1.);
      particle_color.w = 1.f;
      vfx[1].render_resources.hdr_intensity = 6;
      vfx[1].particle_system = (Particle_System) {
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
               .min = 6,
               .max = 6
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
         },
         .texture_path = "res/textures/vfx/Swirl01.png",

         .transform_mode = particle_system_default.transform_mode,
         .spawn_shape = particle_system_default.spawn_shape,
         .transform = {
            .scale = vector3(7),
            .rotation =  transform_identity.rotation,
         },
         .is_local_simulation_space = is_local_simulation_space,
      };

      particle_color   = vector4(0, 0, 0, 1.);
      particle_color.w = 1.f;
      vfx[0].render_resources.hdr_intensity = 0;
      vfx[0].particle_system = (Particle_System) {
         .texture_path = "res/textures/vfx/Flare00.png",
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
               .min = 10,
               .max = 10
            },
            .speed = {
               .min = 0.,
               .max = 0.
            },
            .color = {
               .from = particle_color,
               .to   = particle_color
            },
         },
         .over_lifetime = {
            .color = {
               .values  = {},
               .timings = {},
               .count   = 0
            },
         },
         .transform_mode = particle_system_default.transform_mode,
         .spawn_shape = particle_system_default.spawn_shape,
         .transform = {
            .scale = vector3(7),
            .rotation =  transform_identity.rotation,
         },
         .is_local_simulation_space = is_local_simulation_space,
      };
   }


   // setup_particle_render_state();
   for (int idx = 0; idx < count_of(vfx); idx++) {
      auto ps = &vfx[idx].particle_system;
      ps->transform.position = add(vector3(30, 0, 30), unit_position);

      if (true || 2 == idx) {
         draw_particle_system(&vfx[idx].particle_system, &vfx[idx].render_resources, unit_position, camera_position, camera_forward, camera_right, camera_up);
      }
   }

   draw_spark_vfx(unit_position, unit_direction, camera_position, camera_forward, camera_right, camera_up);
   draw_impact_vfx(unit_position, unit_direction, camera_position, camera_forward, camera_right, camera_up);
}



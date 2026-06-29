typedef enum { UNIT_ACTION_IDLE = 0, UNIT_ACTION_MOVING, UNIT_ACTION_ATTACKING } Unit_Action;

typedef struct {
   Vector2 position;
   Vector2 direction;
   Vector2 target;

   struct {
      float current, max;
   } health;

   float radius;
   float move_speed;
   Unit_Action action;

   struct {
      float cast_time;        // delay from pressing attack until the attack actually spawns, the wind-up.
      float duration;         // how long the attack animation plays before returning to idle. cast_time < duration always.
      float cooldown;         // how long you must wait after spawning before you can press E again
      float elapsed;          // how long we have been in UNIT_ACTION_ATTACKING this swing
      float cooldown_elapsed; // counts down from cooldown to 0 when <= 0 you can attack again
   } attack;
} Unit;

typedef struct {
   Scene_Node node;       // filled by sdf shader
   Model model;           // shared quad model
} Health_Bar_Visual;

typedef struct {
   Scene_Node node;
   Scene_Node hitbox_node;
   Model model;
   Model hitbox_model;
   Health_Bar_Visual health_bar;
} Unit_Visual;


typedef struct {
   struct {
      Vector2 current;
      Vector2 previous;
      Vector2 target;
   } position;
   float speed;
   float radius;
   bool active;
   bool hit;
} Projectile;


typedef struct {
   Scene_Node node;
   Model model;
} Projectile_Visual;

#define MAX_PROJECTILES 16

typedef struct {
   Projectile items[MAX_PROJECTILES];
   Projectile_Visual visuals[MAX_PROJECTILES];
   int count;
} Projectile_Pool;

// Can just increate + 0 to + 1 and create another if for custom_fragment_shader.glsl
#define INSTANCE_RENDERING_MODE_HEALTH INSTANCE_RENDERING_MODE_CUSTOM + 0
void update_health_rendering(Scene_Node node, f32 health_current, f32 health_max, f32 bar_aspect_ratio) {
   Vector4 custom_1 = {health_current, health_max, bar_aspect_ratio}, custom_2 = {0};
   update_rendering_mode(node, INSTANCE_RENDERING_MODE_HEALTH);
   update_custom_data(node, custom_1, custom_2);
}


void update_unit(Unit *u, float dt, Projectile_Pool *pool) {
   u->attack.cooldown_elapsed -= dt;

   if (UNIT_ACTION_ATTACKING == u->action) {
      float previous = u->attack.elapsed;
      u->attack.elapsed += dt;

      if (previous < u->attack.cast_time && u->attack.elapsed >= u->attack.cast_time) {
         u->attack.cooldown_elapsed = u->attack.cooldown;
         for (int i = 0; i < MAX_PROJECTILES; i += 1) {
            if (!pool->items[i].active) {
               pool->items[i] = (Projectile){
                  .position = {
                    .current  = u->position,
                    .previous = u->position,
                    .target   = add(u->position, mul(150.f, u->direction)),
                  },
                  .speed = 25.f,
                  .radius = 5.f,
                  .active = true,
               };
               break;
            }
         }
      }

      if (u->attack.elapsed >= u->attack.duration) {
         u->action = UNIT_ACTION_IDLE;
         u->attack.elapsed = 0.f;
      }
      return;
   }

   if (UNIT_ACTION_IDLE == u->action) {
      return;
   }

   Vector2 to_target = sub(u->target, u->position);
   float distance = length(to_target);
   if (distance < 0.05f) {
      u->position = u->target;
      u->action = UNIT_ACTION_IDLE;
      return;
   }
   

   Vector2 direction = normalize(to_target);
   u->position = add(u->position, mul(min(u->move_speed * dt, distance), direction));
   u->direction = direction;
}


void update_projectile(Projectile *p, float dt) {
   if (!p->active) {
      return;
   }

   if (length(sub(p->position.current, p->position.target)) < 0.001f) {
      p->active = false;
      return;
   }

   p->position.previous = p->position.current;
   Vector2 direction = normalize(sub(p->position.target, p->position.current));
   p->position.current = add(p->position.current, mul(p->speed * dt, direction));
}


bool projectile_hits_unit(const Projectile *p, const Unit *u) {
   Vector2 ab = sub(p->position.current, p->position.previous);
   float ab_len2 = dot(ab, ab);
   if (ab_len2 == 0.0f) {
      return false;
   }

   float t = clamp(dot(sub(u->position, p->position.previous), ab) / ab_len2, 0.0f, 1.0f);
   Vector2 closest = add(p->position.previous, mul(t, ab));
   float distance = length(sub(u->position, closest));

   return distance <= (p->radius + u->radius);
}


void render_unit(const Unit *u, Unit_Visual *v) {
   static float turn_speed = 9.0f;

   Transform t = transform_identity;

   float target_angle         = atan2f(u->direction.x, u->direction.y);
   Quaternion target_rotation = QuaternionFromAxisAngle((Vector3){0, 1, 0}, target_angle);

   Transform current = get_transform(v->node);
   t.scale    = current.scale;
   t.position = vector3(u->position.x, 0.0f, u->position.y);
   t.rotation = QuaternionSlerp(current.rotation, target_rotation, min(turn_speed * time_delta(), 1.0f));

   update_transform(v->node, t);

   Transform ht = transform_identity;
   ht.position  = t.position;
   ht.scale     = vector3(u->radius);
   update_transform(v->hitbox_node, ht);

   switch (u->action) {
   case UNIT_ACTION_MOVING:
      set_animation_speed(v->node, 0.85f);
      play_animation(v->node, 18);
      break;
   case UNIT_ACTION_ATTACKING:
      play_animation(v->node, 0);
      break;
   case UNIT_ACTION_IDLE:
   default:
      play_animation(v->node, 10);
      break;
   }
}


void render_projectile_pool(const Projectile_Pool *pool) {
   for (int i = 0; i < MAX_PROJECTILES; i += 1) {
      const Projectile *p = &pool->items[i];
      const Projectile_Visual *v = &pool->visuals[i];

      if (!p->active) {
         continue;
      }

      Transform t = transform_identity;
      t.scale     = vector3(p->radius);
      t.position  = vector3(p->position.current.x, 0.0f, p->position.current.y);
      update_transform(v->node, t);
      if (p->hit) {
         update_color_tint(v->node, vector4(1, 0, 0, 0.95));
      } else {
         update_color_tint(v->node, vector4(1));
      }
   }
}

void render_health_bar(const Unit *u, Health_Bar_Visual *v, Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   float ratio = clamp(u->health.current / u->health.max, 0.0f, 1.0f);
   float bar_width = u->radius * 2.5f; // slightly wider than unit
   float bar_height = bar_width * (1./8.23076 /* number taken from league aspectration from the health part of the status bar */);
   float bar_y = 45.f;

   Vector3 bar_position = vector3(u->position.x, bar_y, u->position.y);

   Quaternion facing = billboard_rotation(false, bar_position, camera_position, camera_forward, camera_right, camera_up);

   

   Transform transform = transform_identity;
   transform.position = bar_position;
   transform.rotation = facing;
   transform.scale = vector3(bar_width, bar_height, 1.0f);
   update_transform(v->node, transform);
   // update_color_tint(v->background, (Vector4){0.15f, 0.15f, 0.15f, 0.9f});

   Vector4 fill_color;
   if (ratio > 0.5f) {
      float t = (ratio - 0.5f) * 2.0f;
      fill_color = vector4(1.0f - t, 1.0f, 0.0f, 1.0f);
   } else {
      float t = ratio * 2.0f;
      fill_color = vector4(1.0f, t, 0.0f, 1.0f);
   }

   update_color_tint(v->node, fill_color);
   update_health_rendering(v->node, u->health.current, u->health.max, bar_width/bar_height);
   // update_color_tint(v->background, (Vector4){0.15f, 0.15f, 0.15f, 0.9f});
}

#include "particle_system.c"

Scene_Node draw_cube(void) {
   static bool loaded = false;
   static Model cube_model = {0};
   static Scene_Node node = {0};

   if (!loaded) {
      loaded = true;
      cube_model        = create_cube_model();
      node = create_scene_node(&cube_model);
      update_rendering_mode(node, 2);
      update_color_tint(node, vector4(.98, .01, .23, 1));
   }

   return node;
}


Scene_Node draw_model(ZString path, int which) {
   constexpr int this_many_per_frame = 100;
   if (which >= this_many_per_frame) {
      trace_warnf("You're drawing WAY to much, fuck you. We ain't gonna draw no more.");
   }

   static bool loaded    [this_many_per_frame] = {false};
   static Model model    [this_many_per_frame] = {0};
   static Scene_Node node[this_many_per_frame] = {0};

   if (!loaded[which]) {
      loaded[which] = true;
      model [which] = create_model(path);
      auto this_model = &model[which];

      trace_model(&model[which]);
      for (isz idx = 0; idx < this_model->materials.count; idx++) {
         auto* m = &this_model->materials.items[idx];
         if (false && m && m->height) {
            {
               auto in_cone  = m->height;
               auto out_cone = "replace-my-name.conemap.png";
               generate_cone_map_relaxed(in_cone, &out_cone, nullptr, nullptr);
               m->height  = out_cone;
            }
         }
         m->height = "full.png";
      }

      node[which]  = create_scene_node(&model[which]);
   }

   return node[which];
}


void draw_only_league_map(Projection_Application *app) {
   static bool loaded = false;
   static Model league_map = {0};
   static Scene_Node league_map_node = {0};

   if (!loaded) {
      loaded = true;
      minimize_window();

      league_map = create_model("res/models/league/map/LeagueMap.fbx");
      league_map_node = create_scene_node(&league_map);
      update_position(league_map_node, vector3(0, -100, 0));
      update_color_tint(league_map_node, vector4(vector3(20), 1.));
      update_scale(league_map_node, 100);
   }
}

void draw_scene_league(Projection_Application *app) {

   static bool loaded = false;
   static Unit vayne = {0};
   static Unit_Visual vayne_v = {0};
   static Projectile_Pool bolt_pool = {0};
   static Model health_bar_model = {0};
   static Model league_map = {0};
   static Scene_Node league_map_node = {0};
   (void) league_map;
   (void) league_map_node;


   auto per_frame = app->per_frame;
   Vector2 spherical = {per_frame.camera.phi, per_frame.camera.theta};
   Ray mouse_ray = compute_mouse_ray(cursor_position().x, cursor_position().y, get_window_size().x, get_window_size().y, PI / 3.f, per_frame.camera.aspect, per_frame.camera.position, spherical);


   if (!loaded) {
      loaded = true;
      minimize_window();
      const bool load_map = false;

      if (load_map) {
         league_map = create_model("res/models/league/map/LeagueMap.fbx");

         league_map_node = create_scene_node(&league_map);
         update_position(league_map_node, vector3(0, 0, 0));
         update_scale(league_map_node, 800);
      }

      // Game state
      vayne.move_speed     = 39.5f;
      vayne.health.max     = 1200.;
      vayne.health.current = 600.;
      vayne.radius         = 10.f;
      vayne.direction  = (Vector2){0, 1};
      vayne.attack     = (typeof(vayne.attack)){
          .cast_time   = .3f,
          .duration    = .9f,
          .cooldown    = .4f,
      };


      // Visuals
      vayne_v.model        = create_model("res/models/league/vayne/Vayne.fbx");
      vayne_v.hitbox_model = create_torus_model(255, 255, 255, 200);

      vayne_v.node         = create_scene_node(&vayne_v.model, transform_identity);
      update_scale(vayne_v.node, 0.0135);
      vayne_v.hitbox_node  = create_scene_node(&vayne_v.hitbox_model, transform_identity);

      health_bar_model = create_model_from_mesh(generate_quad_mesh(1.0f, 1.0f)); // unit scale, actual size set via transform.scale

      vayne_v.health_bar.model      = health_bar_model;
      vayne_v.health_bar.node = create_scene_node(&health_bar_model, transform_identity);
      update_renderable_render_state(vayne_v.health_bar.node, RENDER_STATE_VFX);

      for (int i = 0; i < MAX_PROJECTILES; i += 1) {
         bolt_pool.visuals[i].node = create_scene_node(vayne_v.hitbox_node);
      }

      play_animation(vayne_v.node, 10);
      restore_window();
   }

   // Input to Game State
   Vector3 ground_hit = {0};
   if (is_button_pressed(BUTTON_MOUSE_RIGHT)) {
      if (raycast_ground(mouse_ray, &ground_hit)) {
         // TODO: We could check if it was in the middle of atacking to make it the auto uncancellable after a certain percentage of aa cast time has been played out.
         vayne.target = vector2(ground_hit.x, ground_hit.z);
         vayne.action = UNIT_ACTION_MOVING;
      }
   }

   if (is_button_pressed(BUTTON_E) && vayne.attack.cooldown_elapsed <= 0.f) {
      if (raycast_ground(mouse_ray, &ground_hit)) {
         vayne.direction      = normalize(sub(((Vector2){ground_hit.x, ground_hit.z}), vayne.position));
         vayne.action         = UNIT_ACTION_ATTACKING;
         vayne.attack.elapsed = 0.f;
      }
   }

   // Game update
   update_unit(&vayne, time_delta(), &bolt_pool);
   for (int i = 0; i < MAX_PROJECTILES; i += 1) {
      update_projectile(&bolt_pool.items[i], time_delta());
   }

   for (int i = 0; i < MAX_PROJECTILES; i += 1) {
      if (bolt_pool.items[i].active && projectile_hits_unit(&bolt_pool.items[i], &vayne)) {
         bolt_pool.items[i].hit = true;
         vayne.health.current -= .1f;
         bolt_pool.items[i].active = true;
      } else {
         bolt_pool.items[i].hit = false;
      }
   }

   // Render
   render_unit(&vayne, &vayne_v);
   render_projectile_pool(&bolt_pool);
   Vector3 forward, right, up;
   camera_basis(spherical, &forward, &right, &up);
   render_health_bar(&vayne, &vayne_v.health_bar, per_frame.camera.position, forward, right, up);

   Vector3 vayne_position_vector3 = vector3(vayne.position.x, 0.0f, vayne.position.y);
   Vector3 vayne_direction_vector3 = vector3(vayne.direction.x, 0.0f, vayne.direction.y);
   draw_vfx(
      vayne_position_vector3,
      vayne_direction_vector3,
      per_frame.camera.position, forward, right, up
   );

   auto cube_node = draw_cube();
   update_position(cube_node, add(25, vayne_position_vector3));
   update_scale(cube_node, 10);

   // auto sibnek = draw_model("res/models/sibenik-cathedral-vray-fbx/source/sibenik_cathedral_vray.fbx", 0);
   // auto sibnek = draw_model("res/models/sibenik-cathedral-vray-fbx/source/sibenik.fbx", 0);
   auto sibnek = draw_model("res/models/sibenik-cathedral-vray-fbx/source/sinebik.fbx", 0);
   update_scale(sibnek, 160.);
   // auto sibnek_rotation = QuaternionFromAxisAngle(vector3(0, 0, 1), DEG2RAD * 270.);
   auto sibnek_rotation = transform_identity.rotation;
   update_rotation(sibnek, sibnek_rotation);
}


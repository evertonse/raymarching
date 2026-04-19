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
      float cast_time;
      float duration;
      float cooldown;
      float elapsed;
      float cooldown_elapsed;
   } attack;
} Unit;

typedef struct {
   Scene_Node node;
   Scene_Node hitbox_node;
   Model model;
   Model hitbox_model;
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
} Projectile_Visual;

#define MAX_PROJECTILES 16

typedef struct {
   Projectile items[MAX_PROJECTILES];
   Projectile_Visual visuals[MAX_PROJECTILES];
   int count;
} Projectile_Pool;


void update_unit(Unit *u, float dt, Projectile_Pool *pool) {
   u->attack.cooldown_elapsed -= dt;

   if (u->action == UNIT_ACTION_ATTACKING) {
      float previous = u->attack.elapsed;
      u->attack.elapsed += dt;

      if (previous < u->attack.cast_time && u->attack.elapsed >= u->attack.cast_time) {
         u->attack.cooldown_elapsed = u->attack.cooldown;
         for (int i = 0; i < MAX_PROJECTILES; i++) {
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

   if (u->action == UNIT_ACTION_IDLE) {
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
   t.scale     = vector3(0.01f);
   t.position  = vector3(u->position.x, 0.0f, u->position.y);

   float target_angle         = atan2f(u->direction.x, u->direction.y);
   Quaternion target_rotation = QuaternionFromAxisAngle((Vector3){0, 1, 0}, target_angle);
   Transform current          = get_transform(v->node);
   t.rotation                 = QuaternionSlerp(current.rotation, target_rotation, min(turn_speed * time_delta(), 1.0f));
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
   for (int i = 0; i < MAX_PROJECTILES; i++) {
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


void draw_scene_league(Projection_Application *app) {
   static bool loaded = false;
   static Unit vayne = {0};
   static Unit_Visual vayne_v = {0};
   static Projectile_Pool bolt_pool = {0};

   auto per_frame = app->per_frame;
   Vector2 spherical = {per_frame.camera.phi, per_frame.camera.theta};
   Ray mouse_ray = compute_mouse_ray(cursor_position().x, cursor_position().y, get_window_size().x, get_window_size().y, PI / 3.f, per_frame.camera.aspect, per_frame.camera.position, spherical);

   if (!loaded) {
      loaded = true;
      minimize_window();

      // Game state
      vayne.move_speed = 39.5f;
      vayne.health     = (typeof(vayne.health)){600.f, 600.f};
      vayne.radius     = 10.f;
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
      vayne_v.hitbox_node  = create_scene_node(&vayne_v.hitbox_model, transform_identity);

      for (int i = 0; i < MAX_PROJECTILES; i++) {
         bolt_pool.visuals[i].node = create_scene_node(vayne_v.hitbox_node);
      }

      play_animation(vayne_v.node, 10);
      restore_window();
   }

   // Input to Game State
   Vector3 ground_hit = {0};
   if (is_button_pressed(BUTTON_MOUSE_RIGHT)) {
      if (raycast_ground(mouse_ray, &ground_hit)) {
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
   for (int i = 0; i < MAX_PROJECTILES; i++) {
      update_projectile(&bolt_pool.items[i], time_delta());
   }

   for (int i = 0; i < MAX_PROJECTILES; i++) {
      if (bolt_pool.items[i].active && projectile_hits_unit(&bolt_pool.items[i], &vayne)) {
         bolt_pool.items[i].hit = true;
         vayne.health.current -= 50.f;
         bolt_pool.items[i].active = true;
      } else {
         bolt_pool.items[i].hit = false;
      }
   }

   // Render
   render_unit(&vayne, &vayne_v);
   render_projectile_pool(&bolt_pool);
}

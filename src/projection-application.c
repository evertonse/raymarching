#include "raymath.h"
#include <minwindef.h>
#include <stdlib.h>
typedef struct {
    Vector3 position; f32 pad0;
    Vector3 ambient;  f32 pad1;
    Vector3 diffuse;  f32 pad2;
    Vector3 specular; f32 pad3;
} Light;


typedef struct {
   Application app; // must be first
   Vector3_List list;

   Shader         shader, light_shader;
   Countdown      shader_countdown_to_reload;
   Vertex_Array   chosen_mesh_va, cube_va, sphere_va;
   Texture        diffuse_texture, cube_texture;
   struct {
      Texture diffuse, specular, specular_colored, emissive;
   } wood_box;

   Joint_List joint_list;

   Framebuffer    fb;

   Rectanglei32   destination;
   Camera         camera;
   Mesh           sphere_mesh;

   Uniform_Buffer ub, per_frame_buffer;
   Buffer buffer;
   struct {
      Model model;
      Transform transform;
      struct {
         Vertex_Array *items;
         isz count;
      } vas;

      struct {
         struct {
            Texture diffuse, specular, specular_colored, emissive;
         } *items;
         isz count;
      } textures;

      struct {
         Buffer vertex_joints;             // Allocated and set once. Updated never again
      } animation;
   } backpack, boy, girl;
   Buffer geometry_to_world_matrices; // Allocated once. Updated eveyframe.

   struct {
      float16 model;
      float16 view;
      float16 pespective;

      Light light;

      struct {
         Vector3 position; f32 pad0;
         f32 theta, phi, pad1, pad2; // Spherical Coordinates
      } camera;

      f32 elapsed_time, delta_time;
   } per_frame STD140_ALIGN;

} Projection_Application;


static const char *shader_paths[] = {
};

// __attribute__((overloadable)) // TODO: Check this out on clang extensions plus builtin vecto3 types
void projection_update_shaders(Projection_Application *app) {

   Shader *shader_slots[] = {
      &app->shader,
      // &app->light_shader
   };

   for (int i = 0; i < count_of(shader_slots); ++i) {
      Shader *s = shader_slots[i];
      const char *path = s->path;

      bool need_reload = shader_needs_reload(*s);
      bool valid = is_valid_shader(*s);
      if (!valid) {
         *s = create_shader(path, 0);
      } else {
         if (need_reload) {
            *s = reload_shader(*s);
         }
      }
      if (!is_valid_shader(*s)) {
         trace_error("Shader %s failed to compile", path);
         return;
      }
   }
}


static void draw_va(Projection_Application *app, Vertex_Array *va, Vector3 position, Vector3 scale, Vector4 rotation) {
   Matrix translation_matrix = MatrixTranslate(position.x, position.y, position.z);
   Matrix scale_matrix       = MatrixScale(scale.x, scale.y, scale.z);
   Matrix rotation_matrix    = MatrixRotate((Vector3){rotation.x, rotation.y, rotation.z}, rotation.w);
   Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

   update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));
   bind_buffer(&va->vb.buffer, BUFFER_TYPE_STORAGE, 3);

   glBindVertexArray(va->handle);
   // glDrawElements(GL_TRIANGLES, va->ib.count, GL_UNSIGNED_INT, NULL);

   glEnable(GL_DEPTH_TEST);

   glDrawElementsBaseVertex(GL_TRIANGLES,
      va->ib.count,               // How many indices
      GL_UNSIGNED_INT,            // Index type
      (void *)(0 * size_of(u32)), // Where indices start
      0                           // Base vertex offset
   );
}

static void init_model_and_its_gpu_data(typeof(((Projection_Application *)0)->boy) *bundle, ZString filepath) {
   bundle->model = create_model(filepath);

   bundle->textures.count = bundle->model.materials.count;
   bundle->textures.items = calloc(bundle->textures.count, size_of(bundle->textures.items[0]));

   // TODO: mo' textures
   for (isz index = 0; index < bundle->model.materials.count; index += 1) {
      auto material = bundle->model.materials.items[index];
      auto texture = &bundle->textures.items[index];
      if (material.diffuse) {
         texture->diffuse  = create_texture_from_filepath(material.diffuse);
      }
      if (material.specular) {
         texture->specular = create_texture_from_filepath(material.specular);
      }
   }

   bundle->vas.items = nullptr;
   bundle->vas.count = 0;
   {
      for (isz idx = 0; idx < bundle->model.meshes.count; idx += 1) {
         Mesh *mesh = &bundle->model.meshes.items[idx];
         bundle->vas.count += mesh->surfaces.count;
      }
      bundle->vas.items = malloc(bundle->vas.count * size_of(bundle->vas.items[0]));
   }

   {
      isz bundle_va_index = 0;
      for (isz mesh_index = 0; mesh_index < bundle->model.meshes.count; mesh_index += 1) {
         Mesh *mesh = &bundle->model.meshes.items[mesh_index];

         create_vertex_arrays_from_mesh(mesh, &bundle->vas.items[bundle_va_index]);
         bundle_va_index += mesh->surfaces.count;
      }
   }
   {
      assert(bundle->model.meshes.count == 1);
      auto joints_data  = bundle->model.meshes.items[0].vertices.joints;
      auto joints_count = bundle->model.meshes.items[0].vertices.count;
      auto joints_size  = joints_count * size_of(joints_data[0]);
      bundle->animation.vertex_joints = create_buffer(
         BUFFER_USAGE_STATIC,
         joints_data,
         joints_size
      );
      assert(bundle->animation.vertex_joints.size == joints_size);
   }
   bundle->transform.scale       = (Vector3){5., 5., 5.};
   bundle->transform.rotation    = (Vector4){-1., 0, 0, PI/2.};
   bundle->transform.translation = (Vector3){30., 16., 20.};
}

static void update_and_draw_model_and_its_gpu_data(typeof(((Projection_Application *)0)->boy) *bundle, Projection_Application *app) {
   if (!is_valid_shader(app->shader)) {
      return;
   }
   auto animation = &bundle->model.animations.items[0];
   animation->time_current += time_delta();
   // TODO: Check why a frame before the animation ends, we fuck up somehow making the model disappear;
   animation->time_end = 17.0;
   animation->time_current = min(animation->time_current, animation->time_end);
   // animation->time_curent += 0.025; // for debugging do not relyu on actual passing time because time it's warped in debug space;
   if (animation->time_current >= animation->time_end) {
      trace_info("Animation about to restart curr %f begin %f end %f", animation->time_current, animation->time_begin, animation->time_end);
      animation->time_current = animation->time_begin;
   }
   auto list = joint_matrices(&bundle->model, animation->time_current);
   isz  list_data_size = (size_of(list.matrices[0])*list.count);

   bool valid = is_valid_buffer(app->geometry_to_world_matrices);
   bool needs_resize = valid && app->geometry_to_world_matrices.size < list_data_size;
   bool needs_allocation = !valid || needs_resize;

   if (needs_resize) {
      destroy_buffer(&app->geometry_to_world_matrices);
   }
   if (needs_allocation) {
      app->geometry_to_world_matrices = create_buffer(BUFFER_USAGE_SUBDATA, nullptr, list_data_size);
   }

   update_buffer(&app->geometry_to_world_matrices, list.matrices, 0, list_data_size);
   bind_buffer(&app->geometry_to_world_matrices, BUFFER_TYPE_STORAGE, 12);
   bind_buffer(&bundle->animation.vertex_joints, BUFFER_TYPE_STORAGE, 9);

   upload_uniform_bool(app->shader, "is_light", false);

   upload_uniform_int(app->shader, "has_animation", -69);

   {
      isz bundle_va_index = 0;
      for (isz mesh_index = 0; mesh_index < bundle->model.meshes.count; mesh_index += 1) {
         Mesh *mesh = &bundle->model.meshes.items[mesh_index];
         for (isz surface_index = 0; surface_index < mesh->surfaces.count; surface_index += 1) {
            auto surface = mesh->surfaces.items[surface_index];
            if (surface.material_index > -1) {
               upload_uniform_bool(app->shader, "has_emissive", false);
               upload_uniform_bool(app->shader, "has_specular", false);
               auto texture = bundle->textures.items[surface.material_index];
               if (is_valid_texture(texture.diffuse)) {
                  bind_texture(texture.diffuse, 3);
               }

               if (is_valid_texture(texture.specular)) {
                  bind_texture(texture.specular, 4);
                  upload_uniform_bool(app->shader, "has_specular", true);
               }

               if (is_valid_texture(texture.emissive)) {
                  bind_texture(texture.emissive, 5);
                  upload_uniform_bool(app->shader, "has_emissive", true);
               }
            }
            draw_va(app, &bundle->vas.items[bundle_va_index++], bundle->transform.translation, bundle->transform.scale, bundle->transform.rotation);
         }
      }
   }

   upload_uniform_int(app->shader, "has_animation", -1);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

void projection_init(Projection_Application *app) {
   app->chosen_mesh_va = create_vertex_array_from_mesh(&chosen_mesh);
   app->cube_va        = create_vertex_array_from_mesh(&cube_mesh);
   app->sphere_mesh    = generate_sphere_mesh(0.5, 2*32, 2*32);
   app->sphere_va      = create_vertex_array_from_mesh(&app->sphere_mesh);

   app->shader       = shader_invalid;
   app->light_shader = shader_invalid;

   // TODO: Investigate why loading 2 shaders bugs all the paths

   {
      auto main =  "res/shaders/main.glsl";
      auto light = "res/shaders/light.glsl";
      app->shader = create_shader(light, 0);
   }


   app->shader_countdown_to_reload = create_countdown(0.19, true);

   // app->fb = create_framebuffer(1600, 800);
   // app->fb = create_framebuffer_multisample(1600, 800, 16);
   app->fb = create_framebuffer_multisample_with_renderbuffers(1600, 800, 16);

   app->destination = (Rectanglei32) {
      .x = 100/4.,
      .y = 100/4.,
      .width  = app->fb.color.width/2.,
      .height = app->fb.color.height/2.
   };

   isz ub_binding = 2;
   app->per_frame_buffer = create_uniform_buffer(size_of(app->per_frame), ub_binding + 2);
   free(app->per_frame_buffer.cpu_mem);
   app->buffer = create_buffer(BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE, nullptr, size_of(app->per_frame));

   app->diffuse_texture           = create_texture_from_filepath(chosen_texture_path);
   app->wood_box.specular         = create_texture_from_filepath("res/textures/specular_container2.png");
   app->wood_box.specular_colored = create_texture_from_filepath("res/textures/specular_container2_colored.png");
   app->wood_box.diffuse          = create_texture_from_filepath("res/textures/diffuse_container2.png");
   app->wood_box.emissive         = create_texture_from_filepath("res/textures/matrix_emissive.jpg");
   app->cube_texture              = create_texture_from_filepath("res/textures/ocean6.png");
   trace_info("va.handle = %d\n", app->chosen_mesh_va.handle);


   // ZString scene_filepath = "res/models/medieval-sword-pack-10-low-poly-game-ready/source/Medieval Sword pack 1_0 (Oakeshott Classification) .fbx";
   // ZString scene_filepath = "res/models/survival-guitar-backpack/source/Survival_BackPack_2.fbx";
   // ZString scene_filepath = "res/models/akm-free-lowpoly/source/AK.fbx";
   // ZString scene_filepath = "res/models/Fantasy_gradient_sword/Fantasy_gradient_sword.fbx";
   // ZString scene_filepath = "res/models/backpack/backpack.obj"; // Works
   // ZString scene_filepath = "res/models/Fantasy-blender-retextured/Sword.fbx"; // Works

   if (false) {
      ZString model_filepath = "res/models/backpack/backpack.obj";
      init_model_and_its_gpu_data(&app->backpack, model_filepath);
   }

   {
      // ZString animation_filepath = "res/models/boy/boy_animation_textured.fbx";
      ZString model_filepath = "res/models/mari/source/Mari.fbx";

      // ZString model_filepath = "res/models/trees/TreeLareg0.fbx";
      init_model_and_its_gpu_data(&app->boy, model_filepath);
   }

   if (false) {
      ZString model_filepath = "res/models/mari/source/Mari.fbx";
      init_model_and_its_gpu_data(&app->girl, model_filepath);
   }

   assert_msg(
         is_valid_framebuffer_and_its_textures(app->fb)
      && is_valid_framebuffer                 (app->fb)
      && is_valid_vertex_array                (app->chosen_mesh_va)
      && is_valid_vertex_array                (app->cube_va)
      && is_valid_texture                     (app->diffuse_texture)
      && is_valid_texture                     (app->cube_texture)
      && is_valid_uniform_buffer              (app->per_frame_buffer)
      && is_valid_buffer                      (app->buffer)
      ,"Something wanst valid upon creation"
   );
}

void draw_old_way(Projection_Application *app, Shader shader, Camera camera) {

   bind_vertex_array(app->chosen_mesh_va);
   bind_texture(app->diffuse_texture, 3);

   {
      Vector3 positions[] = {
         (Vector3){  0.0f,  0.0f,  0.0f  },
         (Vector3){  0.02f,  0.05f, -10.15f },
         (Vector3){ -1.5f, -2.2f, -2.5f  },
         (Vector3){ -3.8f, -2.0f, -12.3f },
         (Vector3){  2.4f, -0.4f, -3.5f  },
         (Vector3){ -1.7f,  3.0f, -7.5f  },
         (Vector3){  1.3f, -2.0f, -2.5f  },
         (Vector3){  1.5f,  2.0f, -2.5f  },
         (Vector3){  1.5f,  0.2f, -1.5f  },
         (Vector3){ -1.3f,  1.0f, -1.5f  }
      };



      // Vector3 scale    = gui_vector3("Model Scale");
      static f32 scale_single    =  12.4;
      static f32 rotation_single =  0;
      gui_float("Model Scale", &scale_single);
      gui_float("Model Rotation", &rotation_single);
      if (is_button_pressed(BUTTON_R)) {
         scale_single    =  12.4;
         rotation_single =  0;
         camera = (typeof(camera)){0};
         camera.position.y = 3.f; // just a bit off the ground
         camera.position.z = -3.f; // just a bit behind both near plane
      }

      GLint model_location = glGetUniformLocation(shader.handle, "model");
      for (isz i = 0; i < count_of(positions); i += 1) {
         Vector3 position = positions[i];
         (void)position;
         if (9 == i) {
            break;
         }


         position = (Vector3){i* scale_single * 2., 0., 0.};
         // Vector3 scale = vector3_gui();
         // Vector3 scale = { 12.3f, 12.3f, 12.3f };
         Matrix translation_matrix = MatrixTranslate(position.x, position.y, position.z);
         Matrix scale_matrix       = MatrixScale(scale_single, scale_single, scale_single);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, rotation_single);

         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

         update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));

         *(Vector4*)app->buffer.mapped_ptr = (Vector4){68.0, 70., 71., 72.};
         bind_buffer(&app->per_frame_buffer.buffer, BUFFER_TYPE_UNIFORM, 4);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(app->chosen_mesh_va));
         bind_buffer(&app->chosen_mesh_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);
         glDrawElements(GL_TRIANGLES, app->chosen_mesh_va.ib.count, GL_UNSIGNED_INT, NULL);

      }
      {

         const f32 scale_single    = 2.4;
         auto translation = app->per_frame.light.position;
         auto scale       = (Vector3){scale_single, scale_single, scale_single};
         auto rotation    = (Vector4){ 0., 1., 0., 0};
         upload_uniform_bool(app->shader, "is_light", true);
         draw_va(app, &app->sphere_va, translation, scale, rotation);
      }

      if (true) {
         const f32 scale_single    = 10.4;
         Matrix translation_matrix = MatrixTranslate(12, 40, -40);
         Matrix scale_matrix       = MatrixScale(scale_single, scale_single, scale_single);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, time_elapsed());
         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));
         upload_uniform_bool(app->shader, "is_light", false);
         update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         bind_texture(app->cube_texture, 3);
         bind_buffer(&app->cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(app->cube_va.handle);
         assert(is_valid_vertex_array(app->cube_va));
         glDrawElements(GL_TRIANGLES, app->cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

      {
         Matrix translation_matrix = MatrixTranslate(0, 0, -2);
         Matrix scale_matrix       = MatrixScale(100000, 1/100., 100000);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, 0);
         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

         upload_uniform_bool(app->shader, "is_light", false);
         update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         bind_texture(app->cube_texture, 3);
         bind_buffer(&app->cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(app->cube_va.handle);
         assert(is_valid_vertex_array(app->cube_va));
         glDrawElements(GL_TRIANGLES, app->cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

      {
         upload_uniform_bool(app->shader, "has_specular", true);
         upload_uniform_bool(app->shader, "has_emissive", true);
         bind_texture(app->wood_box.diffuse,  3);
         bind_texture(app->wood_box.specular, 4);
         bind_texture(app->wood_box.emissive, 5);
         static Vertex_Array learnopengl_cube = {0};
         if (!is_valid_vertex_array(learnopengl_cube)) {
            learnopengl_cube = create_cube_vertex_array();
         }
         draw_va(app, &learnopengl_cube, (Vector3){110., 36., 41.}, (Vector3){20, 20, 20}, (Vector4){1, 1, 1, time_elapsed() * PI/2.});
         bind_texture(app->wood_box.specular_colored, 4);
         draw_va(app, &learnopengl_cube, (Vector3){50., 36., 30.}, (Vector3){10, 20, 20}, (Vector4){1, 1, 1, time_elapsed() * 0.1});
      }

      update_and_draw_model_and_its_gpu_data(&app->boy, app);

      draw_text("Fuck your mother");
   }
}



void projection_update(Projection_Application *app, f64 dt) {

   update_countdown(&app->shader_countdown_to_reload, projection_update_shaders(app));
   if (!is_valid_shader(app->shader)) {
      return;
   }

   const  bool   please_sync = true;
   static GLsync sync = nullptr;
   if (please_sync) {

      if (!sync) {
         trace_warn("Sync object is null");
      }

      wait_sync_point(sync);
   }

   static Vector3 light_position = {110.0f,  16.f, 4.0f};
   gui_vector3("Light Position", &light_position);

   static bool light_move_by_itself = true;
   gui_check_box("Light Move?", &light_move_by_itself);
   if (light_move_by_itself) {
      const float slow_down_time = 0.34;
      light_position.x = 150.0f * (sin(time_elapsed() * slow_down_time)/2. + 0.5);
   }


   {  //  Update the main uniform buffer
      auto per_frame = &app->per_frame;
      Vector3 direction = spherical_to_cartesian(camera.rotation.x, camera.rotation.y);
      Matrix view = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});

      *per_frame   =  (typeof(app->per_frame)) {
         .model           = MatrixToFloatV(MatrixIdentity()),
         .pespective      = MatrixToFloatV(MatrixPerspective(PI/3., (f64)app->fb.color.width/app->fb.color.height, 0.1, 100.0)),
         .view            = MatrixToFloatV(view),
         .light = {
            .position  = light_position,
            .ambient     = {0.59f,  0.55f,  0.99f },
            .diffuse     = {0.77f,  0.55f,  0.50f },
            .specular    = {0.55f,  0.99f,  0.75f },
         },
         .camera = {
            .position = camera.position,
            .theta    = camera.rotation.x,
            .phi      = camera.rotation.y,
         },
         .elapsed_time    = time_elapsed(),
         .delta_time      = time_delta()
      };

      assert(size_of(typeof(app->per_frame)) == size_of(app->per_frame));

      update_buffer(&app->per_frame_buffer.buffer, &app->per_frame, 0, size_of(app->per_frame));
      bind_buffer(&app->per_frame_buffer.buffer, BUFFER_TYPE_UNIFORM, 4);
      *(Vector4*)app->buffer.mapped_ptr = (Vector4){69.0, 70., 71., 72.};
   }


   bind_framebuffer(app->fb);
   if (!is_valid_framebuffer_and_its_textures(app->fb)) {
      debug_framebuffer_state(app->fb);
      debug_depth_testing();
      debug_culling_state();
      trace_error("Framebuffer is not valid");
   }
   clear_framebuffer(app->fb);
   {
      assert_msg(is_valid_texture(app->fb.depth), "");
      //
      // TODO: use these and measure time
      // clear_framebuffer_depth();
      // clear_framebuffer_color();
      //

      // NOTE: This are the usual culprits of weird, missing or outta order triangle redering.
      {
         glDisable   (GL_CULL_FACE);
         // glEnable   (GL_CULL_FACE);
         glCullFace  (GL_FRONT);  // Instead of GL_BACK
         glFrontFace (GL_CW);    // Instead of GL_CCW
         glEnable    (GL_DEPTH_TEST);
         glDepthFunc (GL_LESS);
         glDepthMask (GL_TRUE);
         glClearDepth(1.0);
         glDepthRange(0.0, 1.0);
      }

      // Enable polygon offset to mitigate z-fighting
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1.0f, 1.0f);

      glClearColor(0.21f, 0.2f, 0.2f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      // Wireframe mode
      // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      // back to its default using glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }

   // exit(EXIT_SUCCESS);
   Shader shader = app->shader;
   bind_shader(shader);

   upload_uniform_bool(app->shader, "has_specular", false);
   upload_uniform_bool(app->shader, "has_emissive", false);

   {


      auto shader = app->shader;
      {
         upload_uniform_vec3(shader, "camera_position", camera.position);

         upload_uniform_bool(shader, "is_light", false);

         GLint view_location = glGetUniformLocation(shader.handle, "view");
         // Vector3 direction = spherical_to_cartesian((f32)glfwGetTime(), (f32)glfwGetTime() + PI/2.);
         Vector3 direction = spherical_to_cartesian(camera.rotation.x, camera.rotation.y);
         Matrix  view = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});
         // printf("vec3(%f, %f, %f)\n", direction.x, direction.y, direction.z);
         // Matrix view = MatrixViewFromSpherical(camera.position, -camera.rotation.y, -camera.rotation.x);
         glUniformMatrix4fv(view_location, 1, GL_FALSE, MatrixToFloat(view));
      }

      {
         GLint spherical_location = glGetUniformLocation(shader.handle, "spherical");
         glUniform2f(spherical_location, camera.rotation.y, camera.rotation.x);
      }

      {
         GLint loc = glGetUniformLocation(shader.handle, "perspective");
         Matrix perspective = MatrixPerspective(PI/3., (f64)app->fb.color.width/app->fb.color.height, 0.1, 100.0);
         // perspective.m11 *= -1; // Force to be "left-handed" just like the NDC
         // Matrix perspective = MatrixFrustum(-5., 5.,  -5., 5.,  -5., 5.);
         glUniformMatrix4fv(loc, 1, GL_FALSE, MatrixToFloat(perspective));
      }
   }


   static Scene_Node scene_nodes[10] = {0};
   static bool scene_loaded = false;
   static Scene_Node alleyana = {0};
   unused(alleyana);

   if (!scene_loaded) {
      scene_loaded = true;
      ZString model_filepath = "";

      Model boy_model = create_model("res/models/boy/boy_animation_textured.fbx");
      Model luster_model = create_model("res/models/Lust-Watcher-of-Realms/source/Lust-Watcher-of-Realms.fbx");
      static Model alleyana_model = {0};

      alleyana_model = create_model("res/models/alleyana/source/Alleyana.fbx");
      // alleyana_model = create_model("res/models/alleyana-no-content/source/untitled.obj");
      // alleyana_model = create_model("res/models/alleyana-no-content/source/Alleyana.fbx");

      Transform alleyana_transform = transform_identity;
      alleyana_transform.scale = mul(alleyana_transform.scale, 20.);
      alleyana_transform.translation = add(alleyana_transform.translation, ((Vector3){-20., 0, 0}));
      alleyana = create_scene_node(&alleyana_model, alleyana_transform);

      model_filepath = "res/models/mari/source/Mari.fbx";
      // model_filepath = "res/models/boy/boy_animation_textured.fbx";
      begin_profile();
      {
         Model m  = create_model(model_filepath);
         Transform transform = transform_identity;
         scene_nodes[0] = create_scene_node(&m, transform);
         set_animation_time(scene_nodes[0], 0.0);
         play_animation(scene_nodes[0]);

         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[2] = create_scene_node(&boy_model, transform);
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[4] = create_scene_node(scene_nodes[2], transform);
         set_animation_time (scene_nodes[4], 0.65);
         set_animation_speed(scene_nodes[4], 1.65);

         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[3] = create_scene_node_new_cmd(scene_nodes[0], transform);
         set_animation_time(scene_nodes[3], 0.25);
         set_animation_speed(scene_nodes[3], 0.1);
         play_animation(scene_nodes[3]);
      }
      end_profile(model_filepath);



      model_filepath = "res/models/backpack/backpack.obj";
      begin_profile();
      if (true) {
         Model m  = create_model(model_filepath);
         Transform transform = transform_identity;
         transform.scale = mul(transform.scale, 5.);
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[5] = create_scene_node(&m, transform);
         play_animation(scene_nodes[5]);
      }
      end_profile(model_filepath);


      {
         Transform transform = transform_identity;
         transform.scale = mul(transform.scale, 0.25);
         transform.translation = add(transform.translation, ((Vector3){-45., 5., 0}));
         scene_nodes[8] = create_scene_node(&luster_model, transform);
      }


      if (false) { // Testing somethings
         glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
         glDepthMask(GL_TRUE);
         glDisable(GL_BLEND);
         glDepthFunc(GL_LESS);
      }

      // TODO: Create a destroy function
      // destroy_model(&m);

   }

   const bool draw_with_manager = true;
   if (draw_with_manager) {

      // update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));


      // TODO: make sure scene_node with 0 index is invalid
      play_animation(scene_nodes[3]);
      play_animation(scene_nodes[0]);
      play_animation(scene_nodes[2]);
      play_animation(scene_nodes[4]);
      play_animation(scene_nodes[8]);



      if (is_button_pressed(BUTTON_B)) {
         set_animation_speed(scene_nodes[4], 0.65);
         set_animation_speed(scene_nodes[8], 0.65);
         play_animation_identity(alleyana);
      } else if (is_button_pressed(BUTTON_N)) {
         set_animation_speed(scene_nodes[4], 1.65);
         set_animation_speed(scene_nodes[8], 1.65);
         play_animation(alleyana);
      }

      static Texture tex = {0};
      if (!is_valid_texture(tex)) {
         tex = create_texture_from_filepath("res/models/alleyana/textures/mn_vonr_00_body_d.png");
      }

      bind_texture(tex, 3);
      draw_indirect(tex, app->shader);
   }

   // draw_old_way(app, shader, camera);


   if (please_sync) {
      sync = sync_point(sync);
   }

   if (!is_window_minimized()) {
      Framebuffer final_fb = app->fb;
      if (app->fb.color.samples > 1) {
         // compiler says possible undeifned is not use temp
         // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
         final_fb = resolve_multisample_framebuffer(app->fb);
         // blit_framebuffer_to_swapchain(fb_resolved);
      }
      blit_framebuffer_to_swapchain_rect(final_fb, app->destination);
   }
}


static Projection_Application projection_application = {
   .app = create_application(projection_init, projection_update)
};


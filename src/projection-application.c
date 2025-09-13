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

   Rectangle_I32   destination;
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
         f32 theta, phi, aspect, pad2; // Spherical Coordinates
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

   // const struct {isz width, height;} resolution = {2560, 1080};
   // const struct {isz width, height;} resolution = {1152, 486};
   const struct {isz width, height;} resolution = {1600, 900};

   const isz samples = 16;
   // create_framebuffer_multisample,create_framebuffer
   app->fb = create_framebuffer_multisample_with_renderbuffers(resolution.width, resolution.height, samples);

   // const f64 rectangle_shrink_factor = 0.45;

   isz ub_binding = 2;
   app->per_frame_buffer = create_uniform_buffer(size_of(app->per_frame), ub_binding + 2);
   free(app->per_frame_buffer.cpu_mem);

   app->diffuse_texture           = create_texture_from_filepath(chosen_texture_path);
   app->wood_box.specular         = create_texture_from_filepath("res/textures/specular_container2.png");
   app->wood_box.specular_colored = create_texture_from_filepath("res/textures/specular_container2_colored.png");
   app->wood_box.diffuse          = create_texture_from_filepath("res/textures/diffuse_container2.png");
   app->wood_box.emissive         = create_texture_from_filepath("res/textures/matrix_emissive.jpg");
   app->cube_texture              = create_texture_from_filepath("res/textures/ocean6.png");

   assert_msg(
         is_valid_framebuffer_and_its_textures(app->fb)
      && is_valid_framebuffer                 (app->fb)
      && is_valid_vertex_array                (app->chosen_mesh_va)
      && is_valid_vertex_array                (app->cube_va)
      && is_valid_texture                     (app->diffuse_texture)
      && is_valid_texture                     (app->cube_texture)
      && is_valid_uniform_buffer              (app->per_frame_buffer)
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


      draw_text("Fuck your mother");
   }
}


void draw_scene(Projection_Application *app) {
   static Scene_Node scene_nodes[10] = {-1};
   static Scene_Node sophias[3] = {-1};
   static bool scene_loaded = false;
   static Scene_Node alleyana = {0};

   static Scene_Node box_node  = {-1};
   static Scene_Node sphere_node = {-1};

   if (!scene_loaded) {
      scene_loaded = true;
      ZString model_filepath = "";

      Model boy_model    = create_model("res/models/boy/boy_animation_textured.fbx");
      Model luster_model = create_model("res/models/Lust-Watcher-of-Realms/source/Lust-Watcher-of-Realms.fbx");

      // Model box_model = create_model("res/models/box/box.fbx");
      Model box_model = create_cube_model(nullptr, nullptr, nullptr, nullptr);
      assert(1 == box_model.materials.count);
      box_model.materials.items[0].diffuse = "res/textures/brickwall.jpg";
      box_model.materials.items[0].normal  = "res/textures/brickwall_normal.jpg";

      Model sphere_model = create_sphere_model(1.0, 2*32, 2*32,
            nullptr, nullptr, nullptr, "res/textures/tileable/Cone_Map_1k_normals.png"
      );

      static Model alleyana_model = {0};

      alleyana_model = create_model("res/models/alleyana/source/Alleyana.fbx");
      // alleyana_model = create_model("res/models/alleyana-no-content/source/untitled.obj");
      // alleyana_model = create_model("res/models/alleyana-no-content/source/Alleyana.fbx");

      Transform alleyana_transform = transform_identity;
      alleyana_transform.scale = mul(alleyana_transform.scale, 20.);
      alleyana_transform.translation = add(alleyana_transform.translation, ((Vector3){-20., 0, 0}));
      alleyana = create_scene_node(&alleyana_model, alleyana_transform);


      Transform box_transform   = alleyana_transform;
      box_transform.scale       = (Vector3){200., 200., 5.};
      box_transform.translation = add(box_transform.translation, ((Vector3){-120., -10., 120.}));
      box_transform.rotation = QuaternionFromAxisAngle(vector3(1.), PI/2.);
      box_node = create_scene_node(&box_model, box_transform);

      auto box_padding = 100;
      auto box_scale   = 70;
      box_transform.scale       = (Vector3){box_scale, box_scale, box_scale};
      box_model.materials.items[0].normal  = "res/textures/tileable/Cone_Map_1k_normals.png";
      box_transform.translation = add(box_transform.translation, ((Vector3){-box_padding, 1., 0}));
      create_scene_node(&box_model, box_transform);

      box_model.materials.items[0].normal  = "res/textures/tileable/sofa.png";
      box_transform.translation = add(box_transform.translation, ((Vector3){-box_padding, 1., 0}));
      create_scene_node(&box_model, box_transform);

      box_model.materials.items[0].normal  = "res/textures/tileable/face.jpg";
      box_transform.translation = add(box_transform.translation, ((Vector3){-box_padding, 1., 0}));
      create_scene_node(&box_model, box_transform);

      box_model.materials.items[0].normal  = "res/textures/tileable/base_height_conv_to_nmap.png";
      box_transform.translation = add(box_transform.translation, ((Vector3){-box_padding, 1., 0}));
      create_scene_node(&box_model, box_transform);

      box_model.materials.items[0].normal  = "res/textures/Rock/Cliff_Mossy_B_Normal.png";
      box_transform.translation = add(box_transform.translation, ((Vector3){-box_padding, 1., 0}));
      create_scene_node(&box_model, box_transform);



      Transform sphere_transform = box_transform ;
      sphere_transform.scale       = (Vector3){18., 18., 18.};
      sphere_transform.translation = add(sphere_transform.translation, ((Vector3){-150., 10, 0}));
      sphere_node = create_scene_node(&sphere_model, sphere_transform);


      Model sophia_model     = create_model("res/models/sophia-doll-victory-dance/source/sophia doll victory dance.fbx");
      Model sophia_big_model = create_model("res/models/sophia-doll-victory-dance/source/Sophia Doll VictoryDance.Fbx");

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
         transform.scale  = mul(transform.scale, 0.5);
         scene_nodes[2] = create_scene_node(&boy_model, transform);
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[4] = create_scene_node(scene_nodes[2], transform);
         set_animation_time (scene_nodes[4], 0.65);
         set_animation_speed(scene_nodes[4], 1.65);

         transform.scale  = mul(transform.scale, 2.0);
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
         transform.scale = mul(transform.scale, 1.5);
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         transform.translation = add(transform.translation, ((Vector3){25., 15., 0}));
         scene_nodes[5] = create_scene_node(&m, transform);
         play_animation(scene_nodes[5]);

         transform.scale = mul(((Vector3){1., 1., 1.}), 1.5);
         transform.translation = add(transform.translation, ((Vector3){45., 5., 0}));
         sophias[0] = create_scene_node(&sophia_model, transform);
         set_animation_time_percentage(sophias[0], 0.0);
         set_animation_speed(sophias[0], 0.3);

         transform.translation = add(transform.translation, ((Vector3){25., 5., 0}));
         sophias[1] = create_scene_node(&sophia_big_model, transform);
         set_animation_time_percentage(sophias[1], 0.5);

         transform.translation = add(transform.translation, ((Vector3){25., 5., 0}));
         sophias[2] = create_scene_node(sophias[0], transform);
         set_animation_time_percentage(sophias[2], 0.9);

      }
      end_profile(model_filepath);


      {
         Transform transform = transform_identity;
         transform.scale = mul(transform.scale, 0.25);
         transform.translation = add(transform.translation, ((Vector3){-45., 5., 0}));
         scene_nodes[8] = create_scene_node(&luster_model, transform);
      }




      // TODO: Create a destroy function
      // destroy_model(&m);

   }

   const bool draw_with_manager = true;
   if (draw_with_manager) {

      // update_buffer(&app->per_frame_buffer.buffer, MatrixToFloat(model), offset_of(typeof(app->per_frame), model), size_of(app->per_frame.model));

      auto box_rotation = QuaternionFromAxisAngle(vector3(1.), time_elapsed());
      update_transform(box_node,    box_rotation);
      update_transform(sphere_node, box_rotation);

      // TODO: make sure scene_node with 0 index is invalid
      play_animation(scene_nodes[3]);
      play_animation(scene_nodes[0]);
      play_animation(scene_nodes[2]);
      play_animation(scene_nodes[4]);
      play_animation(scene_nodes[8]);

      play_animation(sophias[0]);
      play_animation(sophias[1]);
      play_animation(sophias[2]);

      // set_global_animation_speed(1.65); // TODO: make this into reality


      if (is_button_pressed(BUTTON_B)) {
         set_animation_speed(scene_nodes[4], 0.65);
         set_animation_speed(scene_nodes[8], 0.65);
         play_animation_identity(alleyana);
      } else if (is_button_pressed(BUTTON_N)) {
         set_animation_speed(scene_nodes[4], 1.65);
         set_animation_speed(scene_nodes[8], 1.65);
      }
      play_animation(alleyana);

      static Texture tex = {0};
      if (!is_valid_texture(tex)) {
         tex = create_texture_from_filepath("res/models/alleyana/textures/mn_vonr_00_body_d.png");
      }

      bind_texture(tex, 3);
   }
}

void draw_scene_few(Projection_Application *app) {
   static Scene_Node alleyana = {-1};
   static bool scene_loaded = false;
   static Buffer tangent_buffer = {0};

   if (!scene_loaded) {
      scene_loaded = true;
      // Model alleyana_model = create_model("res/models/alleyana/source/Alleyana.fbx");
      // Model luster_model      = create_model("res/models/Lust-Watcher-of-Realms/source/Lust-Watcher-of-Realms.fbx");
      // Model luster_model      = create_model("res/models/Lust-Watcher-of-Realms/source/Lust-Watcher-of-Realms.fbx");

      Model mari_model       = create_model("res/models/mari/source/Mari.fbx");
      // Model sophia_model  = create_model("res/models/sophia-doll-victory-dance/source/sophia doll victory dance.fbx");
      Model alleyana_model    = mari_model;
      if (mari_model.meshes.items[0].vertices.tangents) {
         tangent_buffer = create_buffer(
         BUFFER_USAGE_STATIC,
         mari_model.meshes.items[0].vertices.tangents,
         mari_model.meshes.items[0].vertices.count * size_of(mari_model.meshes.items[0].vertices.tangents[0])
      );
      }

      // Model alleyana_model   = create_model("res/models/alleyana/source/Alleyana-No-Textures.fbx");
      // Model alleyana_model   = create_model("res/models/alleyana-no-content/source/Alleyana.fbx");

      // alleyana_model.meshes.items[0].surfaces.items[0].indices_count = 26916;
      // alleyana_model.meshes.items[0].surfaces.items[1].indices_count = 35166;
      // alleyana_model.joints.count = 105;
      // recalc_mesh_normals(&alleyana_model.meshes.items[0]);
      trace_model(&alleyana_model);


      Transform base_transform = transform_identity;
      base_transform.translation = add(base_transform.translation, ((Vector3){-20., 0, -60}));
      // base_transform.scale = vector3(20.);
      base_transform.scale = vector3(0.8);

      base_transform.translation = add(base_transform.translation, ((Vector3){-20., 0, 0}));
      alleyana = create_scene_node(&alleyana_model, base_transform);

      play_animation(alleyana);
   }

   bool const draw_with_manager = true;
   bool play = true;
   double speed = 1.0;

   {
      speed = 0.0;
      if (is_button_pressed(BUTTON_RIGHT)) {
         speed = 0.15;
      } else if (is_button_pressed(BUTTON_LEFT)) {
         speed = -0.15;
      }

      if (is_button_pressed(BUTTON_SHIFT)) {
         speed *= 10.0 * 3;
      }
   }

   {
      if (is_button_pressed(BUTTON_UP)) {
         set_animation_time_percentage(alleyana, 0.0);
      }
      if (is_button_pressed(BUTTON_DOWN)) {
         set_animation_time_percentage(alleyana, 0.999);
      }

   }

   set_animation_speed(alleyana, speed);



   if (play) {
      play_animation(alleyana);
   }

   bind_buffer(&tangent_buffer, BUFFER_TYPE_STORAGE, BINDING_VERTEX_TANGENT);

}

void draw_scene_few2(Projection_Application *app) {
   static Scene_Node sophia = {-1};
   static Scene_Node mari = {-1};
   static Scene_Node alleyana = {-1};
   static Scene_Node lust = {-1};
   static bool scene_loaded = false;

   if (!scene_loaded) {
      scene_loaded = true;
      Model alleyana_model   = create_model("res/models/alleyana/source/Alleyana.fbx");
      // Model sophia_model  = create_model("res/models/sophia-doll-victory-dance/source/sophia doll victory dance.fbx");
      Model sophia_big_model = create_model("res/models/sophia-doll-victory-dance/source/Sophia Doll VictoryDance.Fbx");
      Model mari_model       = create_model("res/models/mari/source/Mari.fbx");
      Model lust_model       = create_model("res/models/Lust-Watcher-of-Realms/source/Lust-Watcher-of-Realms.fbx");
      const float spacing = 29.;

      Transform base_transform = transform_identity;
      base_transform.translation = add(base_transform.translation, ((Vector3){-spacing, 0, -60}));
      base_transform.scale = vector3(20.);

      base_transform.translation = add(base_transform.translation, ((Vector3){-spacing, 0, 0}));
      alleyana = create_scene_node(&alleyana_model, base_transform);

      base_transform.translation = add(base_transform.translation, ((Vector3){-spacing, 0, 0}));
      base_transform.scale = vector3(2.);
      sophia = create_scene_node(&sophia_big_model, base_transform);

      base_transform.translation = add(base_transform.translation, ((Vector3){-spacing, 0, 0}));
      base_transform.scale = vector3(1.);
      mari = create_scene_node(&mari_model, base_transform);

      base_transform.translation = add(base_transform.translation, ((Vector3){-spacing, 0, 0}));
      base_transform.scale = vector3(0.22);
      lust = create_scene_node(&lust_model, base_transform);

      play_animation(sophia);
      play_animation(mari);
      play_animation(alleyana);
      play_animation(lust);
   }

   bool const draw_with_manager = true;
   bool play = true;
   double speed = 1.0;

   {
      speed = 0.0;
      if (is_button_pressed(BUTTON_RIGHT)) {
         speed = 0.15;
      } else if (is_button_pressed(BUTTON_LEFT)) {
         speed = -0.15;
      }

      if (is_button_pressed(BUTTON_SHIFT)) {
         speed *= 10.0 * 3;
      }
   }

   {
      if (is_button_pressed(BUTTON_UP)) {
         set_animation_time_percentage(sophia,   0.0);
         set_animation_time_percentage(mari,     0.0);
         set_animation_time_percentage(alleyana, 0.0);
         set_animation_time_percentage(lust, 0.0);
      }
      if (is_button_pressed(BUTTON_DOWN)) {
         set_animation_time_percentage(sophia,   0.999);
         set_animation_time_percentage(mari,     0.999);
         set_animation_time_percentage(alleyana, 0.999);
         set_animation_time_percentage(lust, 0.999);
      }

   }

   if (play) {
      set_animation_speed(sophia,   speed);
      set_animation_speed(mari,     speed);
      set_animation_speed(alleyana, speed);
      set_animation_speed(lust, speed);
      play_animation(sophia);
      play_animation(mari);
      play_animation(alleyana);
      play_animation(lust);
   }

}


void projection_update(Projection_Application *app, f64 dt) {

   update_countdown(&app->shader_countdown_to_reload, projection_update_shaders(app));
   if (!is_valid_shader(app->shader)) {
      return;
   }

   const  bool   please_sync = false;
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
            .aspect   = (float)app->fb.color.width/app->fb.color.height
         },
         .elapsed_time    = time_elapsed(),
         .delta_time      = time_delta()
      };

      assert(size_of(typeof(app->per_frame)) == size_of(app->per_frame));

      update_buffer(&app->per_frame_buffer.buffer, &app->per_frame, 0, size_of(app->per_frame));
      bind_buffer(&app->per_frame_buffer.buffer, BUFFER_TYPE_UNIFORM, 4);
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


      glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
      if (false) { // Testing somethings
         // glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);
         glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
         glDepthMask(GL_TRUE);
         glDepthFunc(GL_LESS);
      }
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendEquation(GL_FUNC_ADD);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      glEnable(GL_DEPTH_TEST);

      glDisable(GL_CULL_FACE);
      glFrontFace (GL_CW);     // Instead of GL_CCW
      glCullFace  (GL_BACK);   // Instead of GL_BACK

      if (false) {
         glDepthFunc (GL_LESS);
         glDepthMask (GL_TRUE);
         glCullFace  (GL_FRONT);  // Instead of GL_BACK
         glFrontFace (GL_CCW);     // Instead of GL_CCW
         glClearDepth(1.0);
         glDepthRange(0.0, 1.0);
      }

      // Enable polygon offset to mitigate z-fighting
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(0.1f, 0.1f);

      glClearColor(0.21f, 0.2f, 0.2f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   }

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


   draw_scene_few2(app);
   // draw_scene_few(app);
   draw_scene(app);

   if (is_button_pressed(BUTTON_K)) {
      set_redererer_mode(RENDERER_MODE_WIREFRAME);
      draw_indirect((Texture){0}, app->shader);
   } else {
      set_redererer_mode(RENDERER_MODE_FILL);
      draw_indirect((Texture){0}, app->shader);
   }

   if (is_button_pressed(BUTTON_J)) {
      glEnable(GL_CULL_FACE);
   } else {
      glDisable(GL_CULL_FACE);
   }

   // draw_old_way(app, shader, camera);


   if (please_sync) {
      sync = sync_point(sync);
   }

   if (!is_window_minimized()) {
      Framebuffer final_fb = app->fb;
      if (app->fb.color.samples > 1) {
         // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
         final_fb = resolve_multisample_framebuffer(app->fb);
      }


      Framebuffer dst_fb = default_framebuffer, src_fb = final_fb;

      // const f64 rectangle_shrink_factor = 0.80;
      const f64 rectangle_shrink_factor = 1.0;
      Rectangle_I32 destination = {
         // .x = 100/4.,
         // .y = 100/4.,
         // .width = 600, .height = 400,
         .width = src_fb.color.width   * rectangle_shrink_factor,
         .height = src_fb.color.height * rectangle_shrink_factor,
      };

      Rectangle_I32 source = destination;
      blit_framebuffer(dst_fb, src_fb,
         destination,
         source
      );
      // glBlitNamedFramebuffer(
      //       final_fb.handle, 0,
      //       0, 0, 800, 675,
      //       0, 0, 800, 675,
      //       GL_COLOR_BUFFER_BIT,
      //       GL_NEAREST
      // );
   }
}


static Projection_Application projection_application = {
   .app = create_application(projection_init, projection_update)
};


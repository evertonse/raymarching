#include "raymath.h"
typedef struct {
    Vector3 position; f32 pad0;
    Vector3 ambient;  f32 pad1;
    Vector3 diffuse;  f32 pad2;
    Vector3 specular; f32 pad3;
} Light;


typedef struct {
   Application app; // must be first

   Shader         shader, light_shader;
   Countdown      shader_countdown_to_reload;
   Vertex_Array   va, cube_va, sphere_va;
   Texture        diffuse_texture, cube_texture;
   struct{
      Texture diffuse, specular, specular_colored, emissive;
   } wood_box;

   Framebuffer    fb;

   Rectanglei32   destination;
   Camera         camera;
   Mesh           sphere_mesh;

   Uniform_Buffer ub, per_frame_buffer;
   Buffer buffer;
   struct{
      Model model;
      struct {
         Vertex_Array *items;
         isz count;
      } vas;
   } backpack;

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


// __attribute__((overloadable)) // TODO: Check this out on clang extensions plus builtin vecto3 types
void projection_update_shaders(Projection_Application *app) {
   static const char *shader_paths[] = {
      "res/shaders/default.glsl",
      "res/shaders/light.glsl",
   };

   Shader *shader_slots[] = {
      &app->shader,
      &app->light_shader
   };

   for (int i = 0; i < count_of(shader_slots); ++i) {
      Shader *s = shader_slots[i];
      const char *path = shader_paths[i];

      bool need_reload = shader_needs_reload(*s);

      bool valid = is_valid_shader(*s);

      if (!valid || need_reload) {
         if (valid && need_reload) {
            *s = reload_shader(*s);
         } else {
            *s = create_shader(path, 0);
         }

         if (!is_valid_shader(*s)) {
            trace_error("Shader %s failed to compile", path);
            return;
         }
      }
   }
}

   // upload_uniform_bool(app->shader, "is_light", true);
static void draw_va(Projection_Application *app, Vertex_Array *va, Vector3 position, Vector3 scale, Vector4 rotation) {
   Matrix translation_matrix = MatrixTranslate(position.x, position.y, position.z);
   Matrix scale_matrix       = MatrixScale(scale.x, scale.y, scale.z);
   Matrix rotation_matrix    = MatrixRotate((Vector3){rotation.x, rotation.y, rotation.z}, rotation.w);
   Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

   update_buffer(&app->per_frame_buffer, MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));
   bind_buffer_as_type(&va->vb.buffer, BUFFER_TYPE_STORAGE, 3);

   glBindVertexArray(va->handle);
   glDrawElements(GL_TRIANGLES, va->ib.count, GL_UNSIGNED_INT, NULL);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

void projection_init(Projection_Application *app) {
   app->va          = create_vertex_array_from_mesh(&chosen_mesh);
   app->cube_va     = create_vertex_array_from_mesh(&cube_mesh);
   app->sphere_mesh = generate_sphere_mesh(0.5, 2*32, 2*32);
   app->sphere_va   = create_vertex_array_from_mesh(&app->sphere_mesh);

   projection_update_shaders(app);

   app->shader_countdown_to_reload = create_countdown(0.12, true);


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
   app->ub  = create_uniform_buffer(size_of(Matrix)*2, ub_binding);
   app->per_frame_buffer = create_uniform_buffer(size_of(app->per_frame), ub_binding + 2);
   free(app->per_frame_buffer.cpu_mem);
   app->buffer = create_buffer_extended(BUFFER_TYPE_UNIFORM, BUFFER_USAGE_PERSISTENT, nullptr, size_of(app->per_frame), 5);

   app->shader = shader_invalid;

   app->diffuse_texture           = create_texture_from_filepath(chosen_texture_path);
   app->wood_box.specular         = create_texture_from_filepath("res/textures/specular_container2.png");
   app->wood_box.specular_colored = create_texture_from_filepath("res/textures/specular_container2_colored.png");
   app->wood_box.diffuse          = create_texture_from_filepath("res/textures/diffuse_container2.png");
   app->wood_box.emissive         = create_texture_from_filepath("res/textures/matrix_emissive.jpg");
   app->cube_texture              = create_texture_from_filepath("res/textures/ocean6.png");
   trace_info("va.handle = %d\n", app->va.handle);


   ZString backpack_filepath = "res/models/backpack/backpack.obj";
   app->backpack.model = create_model(backpack_filepath);
   trace_info("Loaded model");
   if (true) {

      { // Setting up Vertex_Array array
         app->backpack.vas.items = malloc(app->backpack.model.meshes.count * size_of(app->backpack.vas.items[0]));
         app->backpack.vas.count = app->backpack.model.meshes.count;
         for (isz idx = 0; idx < app->backpack.model.meshes.count; idx += 1) {
            Mesh* mesh = &app->backpack.model.meshes.items[idx];
            trace_struct(*mesh);

            mesh->uvs_count = mesh->vertices_count;
            mesh->normals_count = mesh->vertices_count;

            if (idx == 1) {
               // debug_break();
            }

            app->backpack.vas.items[idx] = create_vertex_array_from_mesh(mesh);
            trace_struct(app->backpack.vas.items[idx]);
            assert_msg(is_valid_vertex_array(app->backpack.vas.items[idx]), "%d-th vertex array is fucked", idx);
         }
      }
   }

   assert_msg(
         is_valid_framebuffer_and_its_textures(app->fb)
      && is_valid_framebuffer(app->fb)
      && is_valid_vertex_array(app->va)
      && is_valid_vertex_array(app->cube_va)
      && is_valid_texture(app->diffuse_texture)
      && is_valid_texture(app->cube_texture)
      && is_valid_uniform_buffer(app->ub)
      && is_valid_uniform_buffer(app->per_frame_buffer)
      && is_valid_buffer(app->buffer)
      ,"Something wanst valid upon creation"
   );

}


void projection_update(Projection_Application *app, f64 dt) {

   static GLsync sync = nullptr;
   wait_sync_point(sync);
   app->ub.offset = 0; // reset for next frame

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
      Matrix  view      = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});

      *per_frame   =  (typeof(app->per_frame)) {
         .model           = MatrixToFloatV(MatrixIdentity()),
         .pespective      = MatrixToFloatV(MatrixPerspective(PI/3., (f64)app->fb.color.width/app->fb.color.height, 0.1, 100.0)),
         .view            = MatrixToFloatV(view),
         .light = {
            .position  = light_position,
            .ambient     = {0.89f,  0.85f,  0.99f },
            .diffuse     = {0.99f,  0.85f,  0.80f },
            .specular    = {0.88f,  0.99f,  0.75f },
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

      update_buffer(&app->per_frame_buffer, &app->per_frame, size_of(app->per_frame), 0);
      *(Vector4*)app->buffer.mapped_ptr = (Vector4){69.0, 70., 71., 72.};
   }



   update_countdown(&app->shader_countdown_to_reload, projection_update_shaders(app));

   bind_framebuffer(app->fb);
   {
      glEnable(GL_DEPTH_TEST);
      //
      // TODO: use these and measure time
      // clear_framebuffer_depth();
      // clear_framebuffer_color();
      //
      glClearColor(0.21f, 0.2f, 0.2f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   }

   // Wireframe mode
   // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   // back to its default using glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   Shader shader = app->shader;
   bind_shader(shader);
   upload_uniform_bool(app->shader, "has_specular", false);
         upload_uniform_bool(app->shader, "has_emissive", false);

   {
      upload_uniform_vec3(shader, "camera_position", &camera.position);

      upload_uniform_bool(shader, "is_light", false);

      GLint view_location = glGetUniformLocation(shader.handle, "view");
      // Vector3 direction = spherical_to_cartesian((f32)glfwGetTime(), (f32)glfwGetTime() + PI/2.);
      Vector3 direction = spherical_to_cartesian(camera.rotation.x, camera.rotation.y);
      Matrix  view = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});
      // printf("vec3(%f, %f, %f)\n", direction.x, direction.y, direction.z);
      // Matrix view = MatrixViewFromSpherical(camera.position, -camera.rotation.y, -camera.rotation.x);
      glUniformMatrix4fv(view_location, 1, GL_FALSE, MatrixToFloat(view));
   // Send to GPU
   }

   {
      GLint spherical_location = glGetUniformLocation(shader.handle, "spherical");
      glUniform2f(spherical_location, camera.rotation.y, camera.rotation.x);

      push_uniform(&app->ub, DATA_TYPE_VEC3, &camera.position, 1);
      update_buffer(&app->ub.buffer, app->ub.cpu_mem, app->ub.offset, 0);

   }

   {
      GLint loc = glGetUniformLocation(shader.handle, "perspective");
      Matrix perspective = MatrixPerspective(PI/3., (f64)app->fb.color.width/app->fb.color.height, 0.1, 100.0);
      // perspective.m11 *= -1; // Force to be "left-handed" just like the NDC
      // Matrix perspective = MatrixFrustum(-5., 5.,  -5., 5.,  -5., 5.);
      glUniformMatrix4fv(loc, 1, GL_FALSE, MatrixToFloat(perspective));
   }


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



      static Storage_Buffer sb1 = {0};
      if (true) {
         isz binding = 3;
         isz offset = 0;
         isz size = chosen_mesh.vertices_count*size_of(*(chosen_mesh.vertices));
         // bind_buffer_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding);
         // glBindBuffer(GL_SHADER_STORAGE_BUFFER, va.vb.buffer.handle);
         // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding, size, offset);
         // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, binding+1);

         if (sb1.buffer.size == 0) {
            trace_warn("initialzing storage_buffer\n");
            sb1 = create_storage_buffer(size, binding, chosen_mesh.vertices, false);
         }
      }

      bind_vertex_array(app->va);
      bind_texture(app->diffuse_texture, 3);

      // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, 5);

      // void bind_buffer_slice_as_type(Buffer* buf, Buffer_Type type, isz binding, isz size, isz offset) {

      // bind_buffer_slice_as_type(&pica_buffer, BUFFER_TYPE_STORAGE, 3, size_of(pica), 0);
      // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, 3, mesh->vertices_count*size_of(*mesh->vertices), 0);
      bind_buffer_as_type(&sb1.buffer, BUFFER_TYPE_STORAGE, 3);
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
      for (isz i = 0; i < count_of(positions); i++) {
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

         update_buffer(&app->per_frame_buffer,    MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));
         // update_buffer(&app->buffer, MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));
         // glFinish();

         *(Vector4*)app->buffer.mapped_ptr = (Vector4){68.0, 70., 71., 72.};
         // isz _ = update_buffer_mapped_ptr(app->per_frame_buffer, MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));
         bind_buffer_as_type(&app->per_frame_buffer.buffer, BUFFER_TYPE_UNIFORM, 4);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(app->va));
         glDrawElements(GL_TRIANGLES, app->va.ib.count, GL_UNSIGNED_INT, NULL);

      }

      // Vector2 position = cursor_position();
      // Vector3Unproject(Vector3 screen_, Matrix projection, Matrix view);


      {

         const f32 scale_single    = 5.4;
         Matrix translation_matrix = MatrixTranslate(app->per_frame.light.position.x, app->per_frame.light.position.y, app->per_frame.light.position.z);
         Matrix scale_matrix       = MatrixScale(scale_single, scale_single, scale_single);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, 0);
         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));
         upload_uniform_bool(app->shader, "is_light", true);
         update_buffer(&app->per_frame_buffer,    MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));
         bind_buffer_as_type(&app->sphere_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(app->sphere_va.handle);
         glDrawElements(GL_TRIANGLES, app->sphere_va.ib.count, GL_UNSIGNED_INT, NULL);
      }


      if (true) {
         const f32 scale_single    = 10.4;
         Matrix translation_matrix = MatrixTranslate(12, 40, -40);
         Matrix scale_matrix       = MatrixScale(scale_single, scale_single, scale_single);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, time_elapsed());
         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));
         upload_uniform_bool(app->shader, "is_light", false);
         update_buffer(&app->per_frame_buffer,    MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         bind_texture(app->cube_texture, 3);
         bind_buffer_as_type(&app->cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(app->cube_va.handle);
         assert(is_valid_vertex_array(app->cube_va));
         glDrawElements(GL_TRIANGLES, app->cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

      if (true) {
         Matrix translation_matrix = MatrixTranslate(0, 0, -2);
         Matrix scale_matrix       = MatrixScale(100000, 1/100., 100000);
         Matrix rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, 0);
         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

         upload_uniform_bool(app->shader, "is_light", false);
         update_buffer(&app->per_frame_buffer,    MatrixToFloat(model), size_of(app->per_frame.model), offset_of(typeof(app->per_frame), model));

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         bind_texture(app->cube_texture, 3);
         bind_buffer_as_type(&app->cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

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


      for (isz idx = 0; idx < app->backpack.vas.count; idx += 1) {
         draw_va(app, &app->backpack.vas.items[idx], (Vector3){120., 36., 30.}, (Vector3){10, 10, 10}, (Vector4){1, 1, 1, 0});
      }

      draw_text("Fuck your mother");
   }


   sync = sync_point(sync);

   if (!is_window_minimized()) {
      Framebuffer fb_resolved = app->fb;
      if (app->fb.color.samples > 1) {
         // compiler says possible undeifned is not use temp
         // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
         fb_resolved = resolve_multisample_framebuffer(app->fb);
         // blit_framebuffer_to_swapchain(fb_resolved);
      }
      blit_framebuffer_to_swapchain_rect(fb_resolved, app->destination);
   }
}


static Projection_Application projection_application = {
   .app = create_application(projection_init, projection_update)
};


#include "raymath.h"
typedef struct {
   Application app; // must be first

   Shader         shader, light_shader;
   Countdown      shader_countdown_to_reload;
   Vertex_Array   va, cube_va, sphere_va;
   Texture        diffuse_texture, cube_texture;

   Framebuffer    fb;

   Rectanglei32   destination;
   Camera         camera;
   Mesh           sphere_mesh;

   Uniform_Buffer ub, ub2;
   struct {
      alignas(16) Matrix model;
      alignas(16) Matrix view;
      alignas(16) Matrix pespective;
      alignas(16) Vector3 camera_position; f32 pad0;
      alignas(16) Vector3 light_position;  f32 pad1;
      alignas(16) Vector3 light_color;     f32 pad2;
      alignas(16) f32 theta, phi; f32 elapsed_time, delta_time;
   } ub_data STD140_ALIGN;

} Projection_Application;





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

      bool want_reload = shader_needs_reload(*s);

      bool valid = is_valid_shader(*s);

      if (!valid || want_reload) {
         if (valid && want_reload) {
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


void projection_init(Projection_Application *app) {
   app->va          = create_vertex_array_from_mesh(&chosen_mesh);
   app->cube_va     = create_vertex_array_from_mesh(&cube_mesh);
   app->sphere_mesh = generate_sphere_mesh(0.5, 32, 32);
   app->sphere_va   = create_vertex_array_from_mesh(&app->sphere_mesh);
   projection_update_shaders(app);

   app->shader_countdown_to_reload = create_countdown(0.12, true);

   app->destination = (Rectanglei32) {
      .x = 100,
      .y = 100,
      .width  = 800,
      .height = 600
   };

   // app->fb = create_framebuffer(1600, 800);
   // app->fb = create_framebuffer_multisample(1600, 800, 16);
   app->fb = create_framebuffer_multisample_with_renderbuffers(1600, 800, 16);

   isz ub_binding = 2;
   app->ub  = create_uniform_buffer(size_of(Matrix)*2, ub_binding);
   app->ub2 = create_uniform_buffer(size_of(app->ub_data), ub_binding + 2);
   free(app->ub2.cpu_mem);

   app->shader = shader_invalid;

   app->diffuse_texture = create_texture_from_filepath(chosen_texture_path);
   app->cube_texture    = create_texture_from_filepath("res/textures/ocean6.png");
   trace_info("va.handle = %d\n", app->va.handle);

   assert_msg(
         is_valid_framebuffer_and_its_textures(app->fb)
      && is_valid_framebuffer(app->fb)
      && is_valid_vertex_array(app->va)
      && is_valid_vertex_array(app->cube_va)
      && is_valid_texture(app->diffuse_texture)
      && is_valid_texture(app->cube_texture)
      && is_valid_uniform_buffer(app->ub)
      && is_valid_uniform_buffer(app->ub2)
      ,"Something wanst valid upon creation"
   );

}


void projection_update(Projection_Application *app, f64 dt) {

   app->ub.offset = 0; // reset for next frame

   {  //  Update the main uniform buffer
      Vector3 direction = spherical_to_cartesian(camera.rotation.x, camera.rotation.y);
      Matrix  view      = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});
      app->ub_data = (typeof(app->ub_data)) {
         .model           = MatrixIdentity(),
         .pespective      = MatrixPerspective(PI/3., (f64)app->fb.color.width/app->fb.color.height, 0.1, 100.0),
         .view            = view,
         .camera_position = camera.position,
         .light_position  = {1.0f,  100.f, 3.0f},
         .light_color     = {0.89f, 0.85f, 1.0f},
         .theta           = camera.rotation.x,
         .phi             = camera.rotation.y,
         .elapsed_time    = time_elapsed(),
         .delta_time      = time_delta()
      };

      isz ub_offset = update_buffer(app->ub2.buffer, &app->ub_data, size_of(app->ub_data), 0);
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
   {
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

   {  // Time uniform
      GLint loc = glGetUniformLocation(shader.handle, "u_time");
      glUniform1f(loc, (f32)glfwGetTime());
   }

   {
      GLint spherical_location = glGetUniformLocation(shader.handle, "spherical");
      glUniform2f(spherical_location, camera.rotation.y, camera.rotation.x);

      // GLint position_location = glGetUniformLocation(shader.handle, "camera_position");
      push_uniform(&app->ub, DATA_TYPE_VEC3, &camera.position, 1);
      update_buffer(app->ub.buffer, app->ub.cpu_mem, app->ub.offset, 0);

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
      bind_texture(app->diffuse_texture, 4);

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

         f32 scale_single    =  12.4;
         f32 rotation_single =  0;
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
         auto translation_matrix = MatrixTranslate(position.x, position.y, position.z);
         auto scale_matrix       = MatrixScale(scale_single, scale_single, scale_single);
         auto rotation_matrix    = MatrixRotate((Vector3){ 0., 1., 0.}, rotation_single);

         Matrix model = mul(translation_matrix, mul(rotation_matrix, scale_matrix));

         isz ub_offset = update_buffer(app->ub2.buffer, MatrixToFloat(model), size_of(app->ub_data.model), offset_of(typeof(app->ub_data), model));

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(app->va));
         glDrawElements(GL_TRIANGLES, app->va.ib.count, GL_UNSIGNED_INT, NULL);
      }

      // Vector2 position = cursor_position();
      // Vector3Unproject(Vector3 screen_, Matrix projection, Matrix view);


      upload_uniform_bool(app->shader, "is_light", true);

      glBindVertexArray(app->sphere_va.handle);
      glDrawElements(GL_TRIANGLES, app->sphere_va.ib.count, GL_UNSIGNED_INT, NULL);

      if (true) {
         Matrix model = MatrixRotate((Vector3){ 1., 1., 1.}, PI/3.);
         // model = MatrixMultiply(MatrixTranslate(0, -0.50, 0), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         glBindTextureUnit(4, app->cube_texture.handle);
         bind_buffer_as_type(&app->cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(app->cube_va.handle);
         assert(is_valid_vertex_array(app->cube_va));
         glDrawElements(GL_TRIANGLES, app->cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

   }

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


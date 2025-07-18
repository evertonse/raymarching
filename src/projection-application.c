typedef struct {
   Application app; // must be first

   Shader         shader;
   Vertex_Array   va, cube_va;
   Texture        diffuse_texture, cube_texture;

   Framebuffer    fb;
   Uniform_Buffer ub;

   Rectanglei32   destination;
   Camera         camera;
} Projection_Application;


void projection_init(Projection_Application *app) {
   app->va = create_vertex_array_from_mesh(&chosen_mesh);
   app->cube_va = create_vertex_array_from_mesh(&cube_mesh);

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
   app->ub = create_uniform_buffer(size_of(Matrix)*2, ub_binding);

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
      ,"Something wanst valid upon creation"
   );

}

void projection_update(Projection_Application *app, f64 dt) {

   app->ub.offset = 0; // reset for next frame


   static const char *shader_path = "res/shaders/default.glsl";
   bool want_reload = shader_needs_reload(app->shader);
   if (!is_valid_shader(app->shader) || want_reload) {
      if (is_valid_shader(app->shader) && want_reload) {
         app->shader = reload_shader(app->shader);
      } else {
         app->shader = create_shader(shader_path, 0);
      }
      if (!is_valid_shader(app->shader)) {
         trace_error("bitch ass shader didnt work");
         return;
         // TODO: Load some default known to work shader program
         // shader = create_shader_from_vertex_and_fragment_memory(vs_src, fs_src);
      }

   }

   bind_framebuffer(app->fb);
   {
      glEnable(GL_DEPTH_TEST);
      glClearColor(0.2f, 0.2f, 0.3f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   }

   // Wireframe mode
   // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   // back to its default using glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   // Step 5: Set MVP (identity for simplicity)
   Shader shader = app->shader;
   bind_shader(shader);

   {
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
      // glUniform3f(position_location, camera.position.x, camera.position.y, camera.position.z);
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

      glBindVertexArray(app->va.handle);
      glBindTextureUnit(4, app->diffuse_texture.handle); // matches binding = 4

      // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, 5);

      // void bind_buffer_slice_as_type(Buffer* buf, Buffer_Type type, isz binding, isz size, isz offset) {

      // bind_buffer_slice_as_type(&pica_buffer, BUFFER_TYPE_STORAGE, 3, size_of(pica), 0);
      // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, 3, mesh->vertices_count*size_of(*mesh->vertices), 0);
      bind_buffer_as_type(&sb1.buffer, BUFFER_TYPE_STORAGE, 3);

      GLint model_location = glGetUniformLocation(shader.handle, "model");
      for (isz i = 0; i < count_of(positions); i++) {
         if (9 == i ) {
            break;
         }

         Vector3 position = positions[i];
         // Matrix model = MatrixRotate((Vector3){0., (float)(i % 2 == 0)*1., 1.}, (f32)glfwGetTime() / 10.);
         Matrix  model = MatrixRotate((Vector3){ 1., 1., 1.}, i);
         // model = MatrixMultiply(MatrixTranslate(position.x, position.y, position.z), model);
         model = MatrixMultiply(MatrixTranslate(i/2., 0., i/2.), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(app->va));
         glDrawElements(GL_TRIANGLES, app->va.ib.count, GL_UNSIGNED_INT, NULL);
      }


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

   Framebuffer fb_resolved = app->fb;
   if (app->fb.color.samples > 1) {
      // compiler says possible undeifned is not use temp
      // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
      fb_resolved = resolve_multisample_framebuffer(app->fb);
      // blit_framebuffer_to_swapchain(fb_resolved);
   }
   blit_framebuffer_to_swapchain_rect(fb_resolved, app->destination);
}

Projection_Application projection_application = {
   .app = create_application(projection_init, projection_update)
};

void render_mesh_to_framebuffer(const Mesh *mesh) {
   static Vertex_Array va = {0};
   static Vertex_Array cube_va = {0};


#if 0
   static Vertex* gpu_vertices = nullptr;
   gpu_vertices = realloc(gpu_vertices, mesh->vertices_count * size_of(Vertex));
   for (u32 i = 0; i < mesh->vertices_count; ++i) {
      gpu_vertices[i].position_v3 = mesh->vertices[i];
      gpu_vertices[i].normal_v3 = mesh->normals[i];
      gpu_vertices[i].uv_v2 = mesh->uvs[i];
   }
   if (!is_valid_vertex_array(va)) {
      va = create_vertex_array(gpu_vertices, mesh->vertices_count, mesh->indices, mesh->indices_count);
   }

#else
   if (!is_valid_vertex_array(va)) {
      va = create_vertex_array_from_mesh(mesh);
   }
#endif

   if (!is_valid_vertex_array(cube_va)) {
      cube_va = create_vertex_array_from_mesh(&cube_mesh);
   }

   static Framebuffer fb = {0};
   if (!is_valid_framebuffer(fb)) {
      // fb = create_framebuffer(1600, 800);
      // fb = create_framebuffer_multisample(1600, 800, 16);
      fb = create_framebuffer_multisample_with_renderbuffers(1600, 800, 16);
   }

   static Uniform_Buffer ub = {0};
   if (!is_valid_buffer(ub.buffer)) {
      isz ub_binding = 2;
      ub = create_uniform_buffer(size_of(Matrix)*2, ub_binding);
   }
   ub.offset = 0; // reset for next frame


   static Shader shader = shader_invalid;
   static const char *shader_path = "res/shaders/default.glsl";
   bool want_reload = shader_needs_reload(shader);
   if (!is_valid_shader(shader) || want_reload) {
      if (is_valid_shader(shader) && want_reload) {
         shader = reload_shader(shader);
      } else {
         shader = create_shader(shader_path, 0);
      }
      if (!is_valid_shader(shader)) {
         // TODO: Load some default known to work shader program
         // shader = create_shader_from_vertex_and_fragment_memory(vs_src, fs_src);
      }

   }

   static Texture diffuse_texture = {0};
   if (!is_valid_texture(diffuse_texture)) {
      diffuse_texture = create_texture_from_filepath(chosen_texture_path);
   }

   static Texture cube_texture = {0};
   if (!is_valid_texture(cube_texture)) {
      cube_texture = create_texture_from_filepath("res/textures/ocean6.png");
   }

   // Step 4: Render setup
   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
   glViewport(0, 0, fb.color.width, fb.color.height);


   {
      glEnable(GL_DEPTH_TEST);
      glClearColor(0.2f, 0.2f, 0.3f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   }

   // Wireframe mode
   // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   // back to its default using glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   // Step 5: Set MVP (identity for simplicity)
   glUseProgram(shader.handle);

   {
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
      push_uniform(&ub, DATA_TYPE_VEC3, &camera.position, 1);
      update_buffer(ub.buffer, ub.cpu_mem, ub.offset, 0);
      // glUniform3f(position_location, camera.position.x, camera.position.y, camera.position.z);
   }

   {
      GLint loc = glGetUniformLocation(shader.handle, "perspective");
      Matrix perspective = MatrixPerspective(PI/3., (f64)fb.color.width/fb.color.height, 0.1, 100.0);
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
         isz size = mesh->vertices_count*size_of(*mesh->vertices);
         // bind_buffer_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding);
         // glBindBuffer(GL_SHADER_STORAGE_BUFFER, va.vb.buffer.handle);
         // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding, size, offset);
         // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, binding+1);

         if (sb1.buffer.size == 0) {
            trace_warn("initialzing storage_buffer\n");
            sb1 = create_storage_buffer(size, binding, mesh->vertices, false);
         }
      }

      static Storage_Buffer sb2 = {0};
      if (true) {
         isz binding = 5;
         isz offset = 0;
         isz size = mesh->indices_count*size_of(u32);
         // bind_buffer_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding);
         // glBindBuffer(GL_SHADER_STORAGE_BUFFER, va.vb.buffer.handle);
         // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding, size, offset);
         // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, binding+1);

         if (sb2.buffer.size == 0) {
            trace_warn("initialzing storage_buffer\n");
            assert(mesh->indices && mesh->indices_count > 0);
            sb2 = create_storage_buffer(size, binding, mesh->indices, false);
         }
      }

      glBindVertexArray(va.handle);
      glBindTextureUnit(4, diffuse_texture.handle); // matches binding = 4

      // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, 5);

      // void bind_buffer_slice_as_type(Buffer* buf, Buffer_Type type, isz binding, isz size, isz offset) {
      float pica = 69.f;

      static Buffer pica_buffer = {0};
      if (pica_buffer.handle == 0) {
         pica_buffer = create_buffer(&pica, size_of(pica));
      }

      // bind_buffer_slice_as_type(&pica_buffer, BUFFER_TYPE_STORAGE, 3, size_of(pica), 0);
      // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, 3, mesh->vertices_count*size_of(*mesh->vertices), 0);
      bind_buffer_as_type(&sb1.buffer, BUFFER_TYPE_STORAGE, 3);
      bind_buffer_as_type(&sb2.buffer, BUFFER_TYPE_STORAGE, 5);

      GLint model_location = glGetUniformLocation(shader.handle, "model");
      for (isz i = 0; i < count_of(positions); i++) {
         if (9 == i ) {
            break;
         }

         Vector3 position = positions[i];
         // Matrix model = MatrixRotate((Vector3){0., (float)(i % 2 == 0)*1., 1.}, (f32)glfwGetTime() / 10.);
         Matrix model = MatrixRotate((Vector3){ 1., 1., 1.}, i);
         // model = MatrixMultiply(MatrixTranslate(position.x, position.y, position.z), model);
         model = MatrixMultiply(MatrixTranslate(i/2., 0., i/2.), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(va));
         glDrawElements(GL_TRIANGLES, va.ib.count, GL_UNSIGNED_INT, NULL);
      }


      if (true) {
         Matrix model = MatrixRotate((Vector3){ 1., 1., 1.}, PI/3.);
         // model = MatrixMultiply(MatrixTranslate(0, -0.50, 0), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         glBindTextureUnit(4, cube_texture.handle);
         bind_buffer_as_type(&cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(cube_va.handle);
         assert(is_valid_vertex_array(cube_va));
         glDrawElements(GL_TRIANGLES, cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

   }

   // Step 7: Cleanup
   glBindVertexArray(0);
   glUseProgram(0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   Rectanglei32 destination = {
      .x = 100,
      .y = 100,
      .width = 800, .height = 600
   };

   Framebuffer fb_resolved = fb;
   if (fb.color.samples > 1) {
      // compiler says possible undeifned is not use temp
      // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
      fb_resolved = resolve_multisample_framebuffer(fb);
      // blit_framebuffer_to_swapchain(fb_resolved);
   }
   blit_framebuffer_to_swapchain_rect(fb_resolved, destination);
}

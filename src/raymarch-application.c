
typedef struct {
   Application app; // must be first
   Shader      compute_shader;
   Camera      camera;
   Texture     compute_shader_texture;
   Framebuffer compute_framebuffer;
} Raymarching_Application;

// static const char *compute_shader_path = "src/compute.glsl";
// static const char *compute_shader_path = "src/shaders/Tunnel-Cylinders.glsl";


constexpr f64 shader_needs_reload_timer_default = 1.1; // Seconds
static f64 shader_needs_reload_timer = shader_needs_reload_timer_default;

static const char *compute_shader_path = "./src/assets/shaders/shadertoy/base.glsl";
void raymarching_application_init(Raymarching_Application* app) {
   isz window_width = app->app.window.width, window_height = app->app.window.height;
   assert(window_width*window_height != 0);

   app->compute_shader = shader_invalid;
   app->compute_shader = create_shader(compute_shader_path, COMPUTE_SHADER);
   if (!is_valid_shader(app->compute_shader)) {
      fprintf(stderr, "Compute shader failed. Fix it and press 'R' to reload.\n");
   }

   app->compute_shader_texture = create_texture(app->app.window.width, app->app.window.height);
   app->compute_framebuffer = create_framebuffer_from_texture(app->compute_shader_texture);
   trace_info("Compute texture handle =%d\n", app->compute_shader_texture.handle);
   assert(is_valid_framebuffer(app->compute_framebuffer) && is_valid_texture(app->compute_shader_texture));
}

void raymarching_application_update(Raymarching_Application *app, f64 dt) {
   Camera camera = app->app.camera;
   isz window_width = app->app.window.width, window_height = app->app.window.height;
   assert(window_width * window_height != 0);
   bool minimized = app->app.window.is_minimized;

   shader_needs_reload_timer -= app->app.time.delta;
   // Resize texture only if need and is not minimized
   if ((window_width != app->compute_shader_texture.width || window_height != app->compute_shader_texture.height) && !minimized) {
      destroy_texture(&app->compute_shader_texture);
      app->compute_shader_texture = create_texture(window_width, window_height);
      trace_info("texture=%d\n", app->compute_shader_texture);
      attach_texture_to_framebuffer(&app->compute_framebuffer, app->compute_shader_texture);
   }

   if (shader_needs_reload_timer <= 0) {
      if (shader_needs_reload(app->compute_shader)) {
         auto old_handle = app->compute_shader.handle;
         app->compute_shader = reload_shader(app->compute_shader);
         if (old_handle == app->compute_shader.handle) {
            // title.reload = "(reload failed)";
         } else {
            // title.reload = "";
         }
      }
      shader_needs_reload_timer = shader_needs_reload_timer_default;
   }

   if (is_valid_shader(app->compute_shader)) {
      bind_shader(app->compute_shader);

      { // Time uniform
         GLint loc = glGetUniformLocation(app->compute_shader.handle, "iTime");
         glUniform1f(loc, (f32)app->app.time.elapsed);
      }

      { // Resolution uniform
         GLint loc = glGetUniformLocation(app->compute_shader.handle, "iResolution");
         glUniform3f(loc, (f32)app->compute_shader_texture.width, (f32)app->compute_shader_texture.height, app->compute_shader_texture.width / (f32)app->compute_shader_texture.height);
      }

      { // Position uniform
         GLint loc = glGetUniformLocation(app->compute_shader.handle, "iPosition");
         glUniform3f(loc, camera.position.x, camera.position.y, camera.position.z);
      }

      { // Position uniform
         GLint loc = glGetUniformLocation(app->compute_shader.handle, "iRotation");
         glUniform3f(loc, camera.rotation.x, camera.rotation.y, camera.rotation.z);
      }

      { // Position uniform
         GLint loc = glGetUniformLocation(app->compute_shader.handle, "iZoom");
         glUniform1f(loc, camera.zoom);
      }

      { // Mouse uniform
         GLint mouse_loc = glGetUniformLocation(app->compute_shader.handle, "iMouse");
         f64 mouse_x, mouse_y;
         glfwGetCursorPos(app->app.window.handle, &mouse_x, &mouse_y);
         glUniform4f(mouse_loc, (f32)mouse_x, (f32)mouse_y, is_button_down(BUTTON_MOUSE_LEFT) ? 1.f : 0.0f, is_button_down(BUTTON_MOUSE_RIGHT) ? 1.f : 0.0f);
      }

      bind_texture_as_image(app->compute_framebuffer.color, 0, TEXTURE_ACCESS_WRITE);

      const GLuint work_group_size = 16;
      const GLuint work_group_size_x = work_group_size;
      const GLuint work_group_size_y = work_group_size;

      GLuint num_groups_x = (app->compute_shader_texture.width + work_group_size_x - 1) / work_group_size_x;
      GLuint num_groups_y = (app->compute_shader_texture.height + work_group_size_y - 1) / work_group_size_y;
      dispatch_compute_shader(app->compute_shader, num_groups_x, num_groups_y, 1);
      // Ensure all writes to the image are complete
      shader_image_acess_barrier();

      blit_framebuffer_to_swapchain(app->compute_framebuffer);
   }
}

Raymarching_Application raymarching_application = {
   .app = create_application(raymarching_application_init, raymarching_application_update)
};


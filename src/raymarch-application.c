
typedef struct {
   Application app; // must be first
   Shader      compute_shader;
   Camera      camera;
   Texture     compute_shader_texture;
   Framebuffer compute_framebuffer;
} Raymarching_Application;



constexpr f64 shader_needs_reload_timer_default = 1.1; // Seconds
static f64 shader_needs_reload_timer = shader_needs_reload_timer_default;

static const char *compute_shader_path = "./src/assets/shaders/shadertoy/base.glsl";
void raymarching_application_init(Raymarching_Application* app) {
   isz window_width = get_window_width(), window_height = get_window_height();
   assert(window_width*window_height != 0);

   app->compute_shader = shader_invalid;
   app->compute_shader = create_shader(compute_shader_path, COMPUTE_SHADER);
   if (!is_valid_shader(app->compute_shader)) {
      fprintf(stderr, "Compute shader failed. Fix it and press 'R' to reload.\n");
   }

   app->compute_shader_texture = create_texture(window_width, window_height);
   app->compute_framebuffer = create_framebuffer_from_texture(app->compute_shader_texture);
   trace_info("Compute texture handle =%d\n", app->compute_shader_texture.handle);
   assert(is_valid_framebuffer(app->compute_framebuffer) && is_valid_texture(app->compute_shader_texture));
}

void raymarching_application_update(Raymarching_Application *app, f64 dt) {
   Camera camera = app->app.camera;
   isz window_width = get_window_width(), window_height = get_window_height();


   bool minimized = is_window_minimized();

   // Resize texture only if need and is not minimized
   bool needs_resize =
         (window_width != app->compute_shader_texture.width || window_height != app->compute_shader_texture.height)
      && (!minimized)
   ;
   if (needs_resize) {
      destroy_texture(&app->compute_shader_texture);
      app->compute_shader_texture = create_texture(window_width, window_height);
      trace_info("%s texture = %ld\n", __func__, app->compute_shader_texture.handle);
      attach_texture_to_framebuffer(&app->compute_framebuffer, app->compute_shader_texture);
   }

   shader_needs_reload_timer -= time_delta();
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

      {  // Time uniform
         upload_uniform_float(app->compute_shader, "iTime",(f32)time_elapsed());
      }

      {  // Resolution uniform
         Vector3 resolution = {(f32)app->compute_shader_texture.width, (f32)app->compute_shader_texture.height, app->compute_shader_texture.width / (f32)app->compute_shader_texture.height };
         upload_uniform_vec3(app->compute_shader, "iResolution", resolution);
      }

      {  // Position uniform
         Vector3 camera_position = { camera.position.x, camera.position.y, camera.position.z };
         upload_uniform_vec3(app->compute_shader, "iPosition", camera_position);
      }

      {  // Rotation uniform
         Vector3 camera_rotation = { camera.rotation.x, camera.rotation.y, camera.rotation.z };
         upload_uniform_vec3(app->compute_shader, "iRotation", camera_rotation);
      }

      { // Zoom uniform
         upload_uniform_float(app->compute_shader, "iZoom", camera.zoom);
      }

      { // Mouse uniform
         Vector2 mouse_position = cursor_position();
         Vector4 mouse_data = { mouse_position.x, mouse_position.x, is_button_held(BUTTON_MOUSE_LEFT), is_button_held(BUTTON_MOUSE_RIGHT)};
         upload_uniform_vec4(app->compute_shader, "iMouse", mouse_data);
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

      auto fb = app->compute_framebuffer;
      blit_framebuffer_to_swapchain(fb);
   }
}

Raymarching_Application raymarching_application = {
   .app = create_application(raymarching_application_init, raymarching_application_update)
};



Framebuffer apply_postprocess(Framebuffer hdr_fb, Framebuffer geometry_framebuffer, Texture ambient_occlusion) {

   static Countdown shader_reload = {0};

   typedef struct {
      bool loaded;

      Shader shader;

      Framebuffer output_fb;

      int width;
      int height;

   } Postprocess_State;

   static Postprocess_State s = {0};


   // Validation
   if (!is_valid_framebuffer(hdr_fb)) {
      trace_error("%s: invalid framebuffer", __func__);
      return hdr_fb;
   }

   if (texture_multisamples(hdr_fb.color) > 1) {
      trace_error("%s: framebuffer is multisampled. Resolve first.", __func__);
      return hdr_fb;
   }

   const int w = hdr_fb.color.width;
   const int h = hdr_fb.color.height;

   // Init / Resize
   bool need_init = !s.loaded || s.width != w || s.height != h;

   if (need_init) {

      destroy_shader(&s.shader);

      destroy_framebuffer(&s.output_fb);

      memset(&s, 0, size_of(s));

      shader_reload = create_countdown(0.25, true);

      s.width = w;
      s.height = h;

      // Final output should be LDR (RGBA8) after tonemap + gamma
      auto format = TEXTURE_FORMAT_RGBA8;
      // auto format = TEXTURE_FORMAT_RGBA32F;
      // auto format = TEXTURE_FORMAT_R11G11B10F;
      Texture out_texture = create_texture(w, h, nullptr, format, TEXTURE_TYPE_2D, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);

      s.output_fb = create_framebuffer_from_texture(out_texture);

      if (!is_valid_framebuffer(s.output_fb)) {
         trace_error("%s: failed creating output framebuffer", __func__);
         return hdr_fb;
      }

      s.shader = create_shader("res/shaders/src/postprocess/postprocess.glsl");

      if (!is_valid_shader(s.shader)) {
         trace_error("%s: failed creating shader", __func__);
         destroy_framebuffer(&s.output_fb);
         memset(&s, 0, size_of(s));
         return hdr_fb;
      }

      s.loaded = true;

      trace_info("%s: initialized (%dx%d)", __func__, w, h);
   }

   // Hot reload
   update_countdown(&shader_reload, { reload_shader_if_needed(&s.shader); });

   // Dispatch
   bind_shader(s.shader);

   // Binding original HDR scene
   bind_texture(hdr_fb.color, BINDING_FRAMEBUFFER_HDR_SCENE_TEXTURE);
   // Binding final output image
   bind_texture_as_image(s.output_fb.color, BINDING_LDR_SCENE_IMAGE, TEXTURE_ACCESS_WRITE);


   Texture depth_buffer        = manager.geometry_framebuffer.depth;
   Texture direct_light_buffer = manager.geometry_framebuffer.colors[FRAMEBUFFER_ATTACHMENTH_COLOR];
   Texture normal_buffer       = manager.geometry_framebuffer.colors[FRAMEBUFFER_ATTACHMENTH_NORMAL];
   Texture position_buffer     = manager.geometry_framebuffer.colors[FRAMEBUFFER_ATTACHMENTH_POSITION];

   if (is_valid_framebuffer(geometry_framebuffer)
      && is_valid_texture(depth_buffer)
      && is_valid_texture(normal_buffer)
      && is_valid_texture(position_buffer)
      && is_valid_texture(ambient_occlusion)
   ) {
      bind_texture(depth_buffer,        BINDING_FRAMEBUFFER_DEPTH_TEXTURE);
      bind_texture(direct_light_buffer, BINDING_FRAMEBUFFER_DIRECT_LIGHT_TEXTURE);
      bind_texture(normal_buffer,       BINDING_FRAMEBUFFER_NORMAL_TEXTURE);
      bind_texture(position_buffer,     BINDING_FRAMEBUFFER_POSITION_TEXTURE);
      bind_texture(ambient_occlusion,   BINDING_AMBIENT_OCCLUSION_TEXTURE);
   } else {
      trace_error("Geometry Framebuffer Invalid");
   }

   dispatch_compute_shader_2d(s.shader, w, h);
   shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);

   return s.output_fb;
}

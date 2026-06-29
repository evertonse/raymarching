
Texture create_blurred_texture(Texture input_texture) {
   typedef struct {
      bool         loaded;
      Shader_Index shader;
      Texture      output;
   } State;

   static State s = {0};

   if (!is_valid_texture(input_texture)) {
      trace_warn("%s: input_texture invalid", __func__);
      return texture_invalid;
   }

   const int w = input_texture.width;
   const int h = input_texture.height;

   // Init / Resize
   const int resize_factor = 1;
   bool need_init = !s.loaded || (s.output.width * resize_factor) != w || (s.output.height * resize_factor) != h;

   if (need_init) {
      destroy_shader(&s.shader);

      destroy_texture(&s.output);

      memset(&s, 0, size_of(s));

      // We might wanna write screen space indirect data here
      auto format = TEXTURE_FORMAT_RGBA32F;
      // auto format = TEXTURE_FORMAT_RGBA8;
      // auto format = TEXTURE_FORMAT_R11G11B10F;
      s.output = create_texture(w / resize_factor, h / resize_factor, nullptr, input_texture.format, input_texture.type, input_texture.filter, input_texture.wrap);
      s.shader = create_managed_shader("res/shaders/src/blur.glsl");

      if (!is_valid_shader(s.shader)) {
         trace_error("%s: failed creating shader", __func__);
         memset(&s, 0, size_of(s));
         // TODO: handle memory?
         return texture_invalid;
      }

      s.loaded = true;
      trace_info("%s: initialized (%dx%d)", __func__, s.output.width, s.output.height);
   }

   // Dispatch
   bind_shader(s.shader);
   bind_texture(input_texture, BINDING_BLUR_INPUT_TEXTURE);
   bind_texture_as_image(s.output, BINDING_BLUR_IMAGE, TEXTURE_ACCESS_WRITE);
   dispatch_compute_shader_2d(shader_from_index(s.shader), s.output.width, s.output.height);
   shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);
   return s.output;
}


Texture create_ambient_occlusion_texture(Texture direct_light_buffer, Texture normal_buffer, Texture position_buffer, Texture depth_buffer) {
   typedef struct {
      bool         loaded;
      Shader_Index shader;
      Texture      output;
   } State;

   static State s = {0};

   if (!is_valid_texture(normal_buffer)) {
      trace_warn("%s: Normal Buffer invalid", __func__);
      return texture_invalid;
   }

   if (!is_valid_texture(position_buffer)) {
      trace_warn("%s: Position Buffer invalid", __func__);
      return texture_invalid;
   }

   if (!is_valid_texture(depth_buffer)) {
      trace_warn("%s: Depth Buffer invalid", __func__);
      return texture_invalid;
   }

   if (  (depth_buffer.width  != position_buffer.width ) || (normal_buffer.width  != position_buffer.width )
      || (depth_buffer.height != position_buffer.height) || (normal_buffer.height != position_buffer.height)
   ) {
      trace_warn("%s: Invalid dimensions, they should all match\nnormal=%dx%d, position=%dx%d, depth=%dx%d,",
         __func__,
         normal_buffer.width, normal_buffer.height, position_buffer.width, position_buffer.height, depth_buffer.width, depth_buffer.height
      );
   }

   const int w = normal_buffer.width;
   const int h = normal_buffer.height;

   // Init / Resize
   // TODO: What if resize factor is float instead how do we check being aware of floating point arithmetic and making sure aspect ratio is preserved.
   const int resize_factor = 2;
   bool need_init = !s.loaded || (s.output.width * resize_factor) != w || (s.output.height * resize_factor) != h;

   if (need_init) {

      destroy_shader(&s.shader);

      destroy_texture(&s.output);

      memset(&s, 0, size_of(s));

      // We might wanna write screen space indirect data here
      auto format = TEXTURE_FORMAT_RGBA32F;
      // auto format = TEXTURE_FORMAT_RGBA8;
      // auto format = TEXTURE_FORMAT_R11G11B10F;
      s.output = create_texture(w / resize_factor, h / resize_factor, nullptr, format, TEXTURE_TYPE_2D_MIPMAPPED, TEXTURE_FILTER_ANISOTROPIC_4X, TEXTURE_WRAP_CLAMP_EDGE);
      s.shader = create_managed_shader("res/shaders/src/ambient_occlusion/ambient_occlusion.glsl");

      if (!is_valid_shader(s.shader)) {
         trace_error("%s: failed creating shader", __func__);
         memset(&s, 0, size_of(s));
         return texture_invalid;
      }
      s.loaded = true;

      trace_info("%s: initialized (%dx%d)", __func__, s.output.width, s.output.height);
   }

   // Dispatch
   bind_shader(s.shader);

   // TODO: Also Receive a color buffer for screen spacee diffuse indirect.
   //       passing normal_buffer for now.
   bind_texture(direct_light_buffer, BINDING_FRAMEBUFFER_DIRECT_LIGHT_TEXTURE  );
   bind_texture(normal_buffer,       BINDING_FRAMEBUFFER_NORMAL_TEXTURE );
   bind_texture(position_buffer,     BINDING_FRAMEBUFFER_POSITION_TEXTURE);
   bind_texture(depth_buffer,        BINDING_FRAMEBUFFER_DEPTH_TEXTURE  );

   bind_texture_as_image(s.output, BINDING_AMBIENT_OCCLUSION_IMAGE, TEXTURE_ACCESS_WRITE);
   dispatch_compute_shader_2d(shader_from_index(s.shader), s.output.width, s.output.height);
   shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);

   generate_mipmaps(&s.output);
   // Blurred create an internal texture that is the same settings in all but size. So if we can generate mips so can blur texture.
   Texture blurred = create_blurred_texture(s.output);
   generate_mipmaps(&blurred);
   return blurred;
}

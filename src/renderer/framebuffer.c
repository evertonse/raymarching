
typedef struct {
   GLuint handle;
   union {
      Texture color;
      Texture colors[8];
   };
   Texture depth;
   bool is_default_framebuffer; // hacky
} Framebuffer;


// Gets updated in renderer
Framebuffer default_framebuffer = {
    .is_default_framebuffer = true,
};

bool inline is_valid_framebuffer(Framebuffer fb) {
   if (fb.is_default_framebuffer) {
      return true;
   }
   if (0 == fb.handle) {
      return false;
   }

   // Ensure color attachments (if valid) have consistent sample counts
   int expected_samples = -1;

   for (int i = 0; i < 8; i++) {
      if (is_valid_texture(fb.colors[i])) {
         if (expected_samples < 0) {
            expected_samples = texture_multisamples(fb.colors[i]);
         } else if (expected_samples != texture_multisamples(fb.colors[i])) {
            return false; // mismatch
         }
      }
   }

   // Depth attachment must match samples
   if (is_valid_texture(fb.depth)) {
      if (expected_samples < 0) {
         expected_samples = texture_multisamples(fb.depth);
      } else if (expected_samples != texture_multisamples(fb.depth)) {
         return false; // mismatch
      }
   }

#if defined(RENDERER_DEBUG)
   GLint current_fb;
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fb);

   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
   glBindFramebuffer(GL_FRAMEBUFFER, current_fb);

   if (status != GL_FRAMEBUFFER_COMPLETE) {
      return false;
   }
#endif

   return true;
}


bool inline is_blit_compatible(
   Framebuffer src, Framebuffer dst,
   int src_width, int src_height,
   int dst_width, int dst_height
) {
   if (!is_valid_framebuffer(src) || !is_valid_framebuffer(dst)) {
      return false;
   }

   int src_samples = texture_multisamples(src.color);
   int dst_samples = texture_multisamples(dst.color);

   // Multisample compatibility
   // Allowed if:
   //   Equal sample counts
   //   One is 0 (single-sample) and the other > 0
   bool allowed =
         (src_samples == dst_samples)
      || ((src_samples == 0) && (dst_samples >  0))
      || ((src_samples >  0) && (dst_samples == 0))
      ;

   if (!allowed) {
      return false;
   }

   // If one is multisampled, no resizing allowed
   if ((src_samples > 0 || dst_samples > 0) &&
       (src_width != dst_width || src_height != dst_height)) {
      return false;
   }

   return true;
}


// This call is probably sorta expensive
inline bool is_framebuffer_missing_attachment(Framebuffer fb) {
   if (fb.handle == 0) {
      trace_error("Checking for missing attachment on a zero handle framebuffer. Oops.");
      return false;
   }

   GLint current_fb;
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fb);

   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

   glBindFramebuffer(GL_FRAMEBUFFER, current_fb);

   return status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
}



inline bool is_valid_framebuffer_and_its_textures(Framebuffer fb) {
   if (!is_valid_framebuffer(fb)) {
      trace_error("Framebuffer %d is invalid or incomplete\n", fb.handle);
      return false;
   }

   if (!is_valid_texture(fb.color)) {
      trace_error("Color texture %d is invalid\n", fb.color.handle);
      return false;
   }

   if (!is_valid_texture(fb.depth)) {
      trace_error("Depth texture %d is invalid\n", fb.depth.handle);
      return false;
   }

   return true;
}

// TODO: Change this name, something more descriptive
inline bool validate_framebuffer(Framebuffer fb) {
   const bool verbose = true;
   if (fb.handle == 0) {
      if (verbose) {
        printf("Framebuffer handle is 0\n");
      }
      return false;
   }

   // Save current framebuffer
   GLint current_fb;
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &current_fb);

   // Bind our framebuffer
   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);

   // Check completeness
   GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

   if (status != GL_FRAMEBUFFER_COMPLETE) {
      if (verbose) {
         trace_info("Framebuffer %d incomplete: ", fb.handle);
         switch (status) {
         case GL_FRAMEBUFFER_UNDEFINED:
            trace_info("GL_FRAMEBUFFER_UNDEFINED\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER\n");
            break;
         case GL_FRAMEBUFFER_UNSUPPORTED:
            trace_info("GL_FRAMEBUFFER_UNSUPPORTED\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE\n");
            break;
         case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            trace_info("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS\n");
            break;
         default:
            trace_info("Unknown error: 0x%04X\n", status);
            break;
         }
      }
      glBindFramebuffer(GL_FRAMEBUFFER, current_fb);
      return false;
   }

   // Check color attachment
   GLint color_attachment;
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color_attachment);

   if (color_attachment != (GLint)fb.color.handle) {
      if (verbose) {
         trace_error("Color attachment mismatch: expected %d, got %d\n", fb.color.handle, color_attachment);
      }
      glBindFramebuffer(GL_FRAMEBUFFER, current_fb);
      return false;
   }

   // Check depth attachment
   GLint depth_attachment;
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depth_attachment);

   if (depth_attachment != (GLint)fb.depth.handle) {
      if (verbose) {
         trace_error("Depth attachment mismatch: expected %d, got %d\n", fb.depth.handle, depth_attachment);
      }
      glBindFramebuffer(GL_FRAMEBUFFER, current_fb);
      return false;
   }

   // Verify depth texture is valid
   if (depth_attachment != 0 && !glIsTexture(depth_attachment)) {
      if (verbose) {
         trace_error("Depth attachment %d is not a valid texture\n", depth_attachment);
      }
      glBindFramebuffer(GL_FRAMEBUFFER, current_fb);
      return false;
   }

   // Check depth texture format
   if (depth_attachment != 0) {
      GLint depth_format;
      glBindTexture(GL_TEXTURE_2D, depth_attachment);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &depth_format);
      glBindTexture(GL_TEXTURE_2D, 0);

      if (verbose)
         printf("Depth texture format: 0x%04X\n", depth_format);

      // Common depth formats GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT32F
      if (!(depth_format == GL_DEPTH_COMPONENT || depth_format == GL_DEPTH_COMPONENT16 || depth_format == GL_DEPTH_COMPONENT24 || depth_format == GL_DEPTH_COMPONENT32F || depth_format == GL_DEPTH24_STENCIL8 || depth_format == GL_DEPTH32F_STENCIL8)) {
         if (verbose)
            printf("Depth texture has invalid format for depth attachment: 0x%04X\n", depth_format);
         glBindFramebuffer(GL_FRAMEBUFFER, current_fb);
         return false;
      }
   }

   glBindFramebuffer(GL_FRAMEBUFFER, current_fb);

   if (verbose) {
      printf("Framebuffer %d is complete and valid\n", fb.handle);
   }
   return true;
}

void debug_framebuffer_state(Framebuffer fb) {
   printf("=== Framebuffer Debug ===\n");
   printf("Framebuffer handle: %d\n", fb.handle);
   printf("Color texture: %d (valid: %s)\n", fb.color.handle, glIsTexture(fb.color.handle) ? "yes" : "no");
   printf("Depth texture: %d (valid: %s)\n", fb.depth.handle, glIsTexture(fb.depth.handle) ? "yes" : "no");

   validate_framebuffer(fb);

   // Check if depth testing is actually enabled
   GLboolean depth_test_enabled;
   glGetBooleanv(GL_DEPTH_TEST, &depth_test_enabled);
   printf("Depth test enabled: %s\n", depth_test_enabled ? "yes" : "no");

   // Check depth function
   GLint depth_func;
   glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
   printf("Depth function: 0x%04X\n", depth_func);

   printf("=========================\n");
}

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture) {
   assert(framebuffer && (is_valid_framebuffer(*framebuffer) || is_framebuffer_missing_attachment(*framebuffer)));
   assert(is_valid_texture(texture));

   GLenum attachment = GL_COLOR_ATTACHMENT0;

   switch (texture.format) {
   case TEXTURE_FORMAT_RGBA32F:
   case TEXTURE_FORMAT_RGBA8:
   case TEXTURE_FORMAT_RGB8:
   case TEXTURE_FORMAT_RG8:
   case TEXTURE_FORMAT_R8: {
      attachment = GL_COLOR_ATTACHMENT0;
      framebuffer->color = texture;
      break;
   }
   case TEXTURE_FORMAT_DEPTH24: {
      attachment = GL_DEPTH_ATTACHMENT;
      framebuffer->depth = texture;
      break;
   }
   case TEXTURE_FORMAT_SHADOW: {
      attachment = GL_DEPTH_ATTACHMENT;
      framebuffer->depth = texture;
      break;
   }
   default: {
      assert_msg(false, "Unsupported texture format for framebuffer attachment\n");
      return false;
   }
   }

   // TODO: Assert that all attachments have the same amount of samples; also check that on is_valid_framebuffer
   glNamedFramebufferTexture(framebuffer->handle, attachment, texture.handle, 0);

   if (glCheckNamedFramebufferStatus(framebuffer->handle, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Framebuffer is not complete!\n");
      return false;
   }

#if 0
   // if the framebuffer only has a depth texture might make sense
   if (attachment == GL_DEPTH_ATTACHMENT && make suure only depth and no color attachment) {
      glNamedFramebufferDrawBuffer(framebuffer->handle, GL_NONE);
      glNamedFramebufferReadBuffer(framebuffer->handle, GL_NONE);
   }
#endif

   return true;
}

Framebuffer create_framebuffer_from_textures(Texture color, Texture depth) {
   Framebuffer fb = {0};

   glCreateFramebuffers(1, &fb.handle);

   if (is_valid_texture(color) && TEXTURE_FORMAT_DEPTH24 != color.format) {
      if (!attach_texture_to_framebuffer(&fb, color)) {
         trace_error("Couldn't attach color texture %d to framebuffer %d", color.handle, fb.handle);
         glDeleteFramebuffers(1, &fb.handle);
         return (Framebuffer){0};
      }
   }

   if (is_valid_texture(depth) && TEXTURE_FORMAT_DEPTH24 == depth.format) {
      if (!attach_texture_to_framebuffer(&fb, depth)) {
         glDeleteFramebuffers(1, &fb.handle);
         trace_error("Couldn't attach depth buffer %d to framebuffer %d", depth.handle, fb.handle);
         return (Framebuffer){0};
      }
   }

   GLenum status = glCheckNamedFramebufferStatus(fb.handle, GL_FRAMEBUFFER);
   if (GL_FRAMEBUFFER_COMPLETE != status) {
      trace_error("Framebuffer not complete: 0x%X\n", status);
      glDeleteFramebuffers(1, &fb.handle);
      return (Framebuffer){0};
   }

   return fb;
}

Framebuffer create_framebuffer(int width, int height) {
   Texture color = create_texture(width, height, nullptr, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);
   Texture depth = create_texture(width, height, nullptr, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);
   return create_framebuffer_from_textures(color, depth);
}

Framebuffer create_framebuffer_multisample(int width, int height, int samples) {
   Texture_Type type = TEXTURE_TYPE_2D;
   if (samples >= 2)  type = TEXTURE_TYPE_2D_MULTISAMPLED_2X;
   if (samples >= 4)  type = TEXTURE_TYPE_2D_MULTISAMPLED_4X;
   if (samples >= 8)  type = TEXTURE_TYPE_2D_MULTISAMPLED_8X;
   if (samples >= 16) type = TEXTURE_TYPE_2D_MULTISAMPLED_16X;
   if (samples > 16) trace_warn("%s: More than 16 samples are not supported", __func__);
   Texture color = create_texture(width, height, nullptr, TEXTURE_FORMAT_RGBA32F, type, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);
   Texture depth = create_texture(width, height, nullptr, TEXTURE_FORMAT_DEPTH24, type, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);
   return create_framebuffer_from_textures(color, depth);
}

Framebuffer create_framebuffer_multisample_with_renderbuffers(int width, int height, int samples) {
   Framebuffer fb = {0};

   glCreateFramebuffers(1, &fb.handle);

   // Color renderbuffer
   GLuint color_rb;
   glCreateRenderbuffers(1, &color_rb);
   glNamedRenderbufferStorageMultisample(color_rb, (samples == 1 ? 0 : samples), GL_RGBA32F, width, height);
   glNamedFramebufferRenderbuffer(fb.handle, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rb);

   // Depth renderbuffer
   GLuint depth_rb;
   glCreateRenderbuffers(1, &depth_rb);
   glNamedRenderbufferStorageMultisample(depth_rb, (samples == 1 ? 0 : samples), GL_DEPTH_COMPONENT24, width, height);
   glNamedFramebufferRenderbuffer(fb.handle, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);

   // Use dummy textures for compatibility with framebuffer struct
   fb.color = (Texture){.handle = color_rb, .width = width, .height = height, .format = TEXTURE_FORMAT_RGBA32F, .type = TEXTURE_TYPE_2D};

   fb.depth = (Texture){.handle = depth_rb, .width = width, .height = height, .format = TEXTURE_FORMAT_DEPTH24, .type = TEXTURE_TYPE_2D};

   GLenum status = glCheckNamedFramebufferStatus(fb.handle, GL_FRAMEBUFFER);
   if (GL_FRAMEBUFFER_COMPLETE != status) {
      fprintf(stderr, "[ERROR] Multisample framebuffer incomplete: 0x%X\n", status);
      glDeleteFramebuffers(1, &fb.handle);
      fb.handle = 0;
   }

   return fb;
}


Framebuffer create_framebuffer_from_texture(const Texture texture) {
   Framebuffer result = {0};

   glCreateFramebuffers(1, &result.handle);

   if (!attach_texture_to_framebuffer(&result, texture)) {
      glDeleteFramebuffers(1, &result.handle);
      return (Framebuffer){0};
   }

   return result;
}


void destroy_framebuffer(Framebuffer *fb) {
   glDeleteFramebuffers(1, &fb->handle);
   destroy_texture(&fb->color);
   destroy_texture(&fb->depth);
   *fb = (Framebuffer){0};
}


i32 current_framebuffer_handle(void) {
   GLint fb_handle;
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb_handle);
   return fb_handle;
}


int inline default_framebuffer_samples(void) {
   // Query swapchain (default framebuffer) sample count
   static GLint swapchain_samples = -1;
   if (-1 == swapchain_samples) {
      glGetIntegerv(GL_SAMPLES, &swapchain_samples);
   }
   return swapchain_samples;
}


// Blit from one framebuffer to another (dst <- src)
// NOTE: See for common errors: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlitFramebuffer.xhtml
// Error if either read or draw buffers is multisampled the dimensions of the source and destination rectangles is not identical
// Error if data type (unsigned, fixed, float, signed) of read or draw buffers does not match
void inline blit_framebuffer(
   const Framebuffer dst_fb,
   const Framebuffer src_fb,
   int dst_x0, int dst_y0, int dst_x1, int dst_y1,
   int src_x0, int src_y0, int src_x1, int src_y1
) {
   // Validate framebuffers
   int src_samples = texture_multisamples(src_fb.color);
   int dst_samples = texture_multisamples(dst_fb.color);
   assert_msg(is_valid_framebuffer(src_fb), "Invalid source framebuffer: format=%d, samples=%d\n",      src_fb.color.format, src_samples);
   assert_msg(is_valid_framebuffer(dst_fb), "Invalid destination framebuffer: format=%d, samples=%d\n", dst_fb.color.format, dst_samples);

   int src_width  = src_x1 - src_x0;
   int src_height = src_y1 - src_y0;
   int dst_width  = dst_x1 - dst_x0;
   int dst_height = dst_y1 - dst_y0;

   // Handle MSAA rules
   if (src_samples > 0 || dst_samples > 0) {

      // Allowed if equal, or one is single-sample and the other multisample
      bool allowed =
            (src_samples == dst_samples)
         || ((src_samples == 0) && dst_samples > 0)
         || ((src_samples > 0) && dst_samples == 0)
      ;
      if (!allowed) {
         trace_error(
            "Blitting MSAA framebuffer invalid samples: dst %d vs src %d\n",
            dst_samples, src_samples
         );
         return;
      }

      //
      // NOTE: I think it should be possible to have different sizes when the samples match.
      //       But I can't make this work. I get "Source and destination dimensions must be identical with the current filtering modes."
      //       Idk what that's about. Read a bunch and could find a case where the user would like to draw a smaller fb into the bigger default fb considering
      //       they're both multisampled and with matching sample count
      //
      // If one of them is Multisample then it can't resize at the same time
      // from: https://www.khronos.org/opengl/wiki/Framebuffer#:~:text=cannot%20do%20multisampled%20blits%20and%20rescaling%20at%20the%20same%20time.
      //
      if (src_width != dst_width || src_height != dst_height) {
         trace_error(
            "Blitting MSAA framebuffer with mismatched dimensions: "
            "src %dx%d (%d samples) vs dst %dx%d (%d samples)\n",
            src_width, src_height, src_samples,
            dst_width, dst_height, dst_samples
         );
         return;
      }
   }

   // NOTE: Extension will probably be needed
   const bool want_depth = false;
   if (want_depth) {
      trace_error("Blitting depth/stencil not supported in this function");
      return;
   }

   // Yes, it is legal to blit from floating-point fb to a u32 fb and vice versa.
   // So, no need to check this
   // See: https://www.khronos.org/opengl/wiki/Framebuffer#:~:text=Thus%2C%20it%20is%20legal%20to%20blit%20from%20an%20GL_RGBA8%20buffer%20to%20a%20GL_RGBA32F%20and%20vice%20versa
   glBlitNamedFramebuffer(
       src_fb.handle,                   // source
       dst_fb.handle,                   // destination
       src_x0, src_y0, src_x1, src_y1,  // source rectangle
       dst_x0, dst_y0, dst_x1, dst_y1,  // destination rectangle
       GL_COLOR_BUFFER_BIT,             // mask
       GL_NEAREST                       // filter
   );
}

void inline overload blit_framebuffer(
   const Framebuffer dst_fb,
   const Framebuffer src_fb,
   Rectangle_I32 src_rectangle,
   Rectangle_I32 dst_rectangle
) {
   int dst_x0 = dst_rectangle.x, dst_y0 = dst_rectangle.y, dst_x1 = dst_rectangle.x + dst_rectangle.width, dst_y1 = dst_rectangle.y + dst_rectangle.height;
   int src_x0 = src_rectangle.x, src_y0 = src_rectangle.y, src_x1 = src_rectangle.x + src_rectangle.width, src_y1 = src_rectangle.y + src_rectangle.height;
   blit_framebuffer(dst_fb, src_fb,
      dst_x0, dst_y0, dst_x1, dst_y1,
      src_x0, src_y0, src_x1, src_y1
   );
}


void inline blit_framebuffer_to_swapchain(const Framebuffer framebuffer) {
   blit_framebuffer(
      default_framebuffer,
      framebuffer,
      0, 0, framebuffer.color.width, framebuffer.color.height, // source rect
      0, 0, framebuffer.color.width, framebuffer.color.height // destination rect
   );
}


Framebuffer resolve_multisample_framebuffer(Framebuffer msaa_fb) {
   static Framebuffer static_resolve_fb = {0};
   if (!is_valid_framebuffer(msaa_fb)) {
      trace_error("%s: input framebuffer is not valid.\n", __func__);
      return (Framebuffer){0};
   }

   const Texture *src = &msaa_fb.color;

   // Not multisampled? Return original
   if (texture_multisamples(*src) <= 1) {
      return msaa_fb;
   }

   // Create (or recreate) the static resolve framebuffer if needed
   bool valid = is_valid_framebuffer(static_resolve_fb);

   // Why all need to match ! See: https://www.khronos.org/opengl/wiki/Multisampling#:~:text=Note%20that%20such%20a%20resolve%20blit%20operation%20cannot%20also%20rescale%20the%20image%20or%20change%20its%20format.
   bool recreate = !valid
      || static_resolve_fb.color.width  != src->width
      || static_resolve_fb.color.height != src->height
      || static_resolve_fb.color.format != src->format
   ;

   trace_debug("recreate = %d, static{ %d, %dx%d }, src{ %d, %dx%d }",
      recreate,
      static_resolve_fb.handle, static_resolve_fb.color.width, static_resolve_fb.color.height,
      msaa_fb.handle, msaa_fb.color.width, msaa_fb.color.height
   );

   if (recreate) {
      if (valid) {
         destroy_framebuffer(&static_resolve_fb);
      }


      Texture resolved_color = create_texture(src->width, src->height, nullptr, src->format, TEXTURE_TYPE_2D, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);

      static_resolve_fb = create_framebuffer_from_texture(resolved_color);

      if (!is_valid_framebuffer(static_resolve_fb)) {
         printf("[Error] resolve_multisample_framebuffer: failed to create resolve framebuffer.\n");
         return (Framebuffer){0};
      }
   }

   assert_msg(is_valid_framebuffer(static_resolve_fb) && is_valid_framebuffer(msaa_fb), "Getting here all must be valid");

   // Blit color only, no depth.
   glBlitNamedFramebuffer(msaa_fb.handle, static_resolve_fb.handle,
      0, 0, src->width, src->height,
      0, 0, src->width, src->height,
      GL_COLOR_BUFFER_BIT,
      GL_NEAREST
   );

   GLenum err = glGetError();
   if (err != GL_NO_ERROR) {
      trace_error("[Error] glBlitNamedFramebuffer failed during resolve: GL error 0x%X\n", err);
      return (Framebuffer){0};
   }

   return static_resolve_fb;
}


// Auto sets the view port, maybe we should make that clear? maybe it doesnt matter we kinda mostly want that.
void bind_framebuffer(Framebuffer fb) {
   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
   glViewport(0, 0, fb.color.width, fb.color.height);
}


void clear_framebuffer_color_indexed(Framebuffer fb, int draw_buffer_index, const float color[4]) {
   glClearNamedFramebufferfv(fb.handle, GL_COLOR, draw_buffer_index, color);
}


void clear_framebuffer_color(Framebuffer fb, const float color[4]) {
   clear_framebuffer_color_indexed(fb, 0, color);
}


void clear_framebuffer_depth(Framebuffer fb, float depth_value) {
   glClearNamedFramebufferfv(fb.handle, GL_DEPTH, 0, &depth_value);
}


void clear_framebuffer(Framebuffer fb) {
   bool color_valid = is_valid_texture(fb.color);
   bool depth_valid = is_valid_texture(fb.depth);

   assert_msg(is_valid_framebuffer(fb), "Tried to clear a framebuffer that is not valid");
   assert_msg(color_valid || depth_valid, "Tried to clear a framebuffer that has no textures attached");

   const f32 depth_value = 0;
   if (depth_valid) {
      clear_framebuffer_depth(fb, depth_value);
   }

   const f32 alpha_value = 1.0f;
   if (color_valid) {
      clear_framebuffer_color(fb, (float[4]){.1, .1, .1, alpha_value});
   }
}


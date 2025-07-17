
typedef struct {
   GLuint handle;
   Texture color, depth;
} Framebuffer;

inline bool is_valid_framebuffer(Framebuffer fb) {
    return fb.handle != 0;
}

inline bool is_valid_framebuffer_and_its_textures(Framebuffer fb) {
    return fb.handle != 0 && is_valid_texture(fb.color) && is_valid_texture(fb.depth);
}

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture) {
   assert(framebuffer && is_valid_framebuffer(*framebuffer) && is_valid_texture(texture));

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
            glDeleteFramebuffers(1, &fb.handle);
            return (Framebuffer){0};
        }
    }

    // Add more depth compatible formats in this if needed
    if (is_valid_texture(depth) && TEXTURE_FORMAT_DEPTH24 == depth.format) {
        if (!attach_texture_to_framebuffer(&fb, depth)) {
            glDeleteFramebuffers(1, &fb.handle);
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
    Texture color = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, 1);
    Texture depth = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, 1);
    return create_framebuffer_from_textures(color, depth);
}

Framebuffer create_framebuffer_multisample(int width, int height, int samples) {
    Texture_Type type = TEXTURE_TYPE_2D;
    Texture color = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, type, samples);
    Texture depth = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, type, samples);
    return create_framebuffer_from_textures(color, depth);
}

Framebuffer create_framebuffer_multisample_with_renderbuffers(int width, int height, int samples) {
    Framebuffer fb = {0};

    glCreateFramebuffers(1, &fb.handle);

    // Color renderbuffer
    GLuint color_rb;
    glCreateRenderbuffers(1, &color_rb);
    glNamedRenderbufferStorageMultisample(color_rb, (samples == 1 ? 0 : samples), GL_RGBA8, width, height);
    glNamedFramebufferRenderbuffer(fb.handle, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_rb);

    // Depth renderbuffer
    GLuint depth_rb;
    glCreateRenderbuffers(1, &depth_rb);
    glNamedRenderbufferStorageMultisample(depth_rb, (samples == 1 ? 0 : samples), GL_DEPTH_COMPONENT24, width, height);
    glNamedFramebufferRenderbuffer(fb.handle, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);

    // Use dummy textures for compatibility with framebuffer struct
    fb.color = (Texture){
        .handle = color_rb,
        .width = width,
        .height = height,
        .format = TEXTURE_FORMAT_RGBA8,
        .type = TEXTURE_TYPE_2D,
        .samples = samples
    };

    fb.depth = (Texture){
        .handle = depth_rb,
        .width = width,
        .height = height,
        .format = TEXTURE_FORMAT_DEPTH24,
        .type = TEXTURE_TYPE_2D,
        .samples = samples
    };

    GLenum status = glCheckNamedFramebufferStatus(fb.handle, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[ERROR] Multisample framebuffer incomplete: 0x%X\n", status);
        glDeleteFramebuffers(1, &fb.handle);
        fb.handle = 0;
    }

    return fb;
}



Framebuffer create_framebuffer_from_texture(const Texture texture) {
   Framebuffer result;

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

// See for common errors: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBlitFramebuffer.xhtml
// Error if either read or draw buffers is multisampled the dimensions of the source and destination rectangles is not identical
// Error if data type (unsigned, fixed, float, signed) of read or draw buffers does not match
inline void blit_framebuffer_to_swapchain_src_and_dst(
    const Framebuffer framebuffer,
    int src_x0, int src_y0, int src_x1, int src_y1,
    int dst_x0, int dst_y0, int dst_x1, int dst_y1,
    GLbitfield mask,
    GLenum filter
) {
   assert_msg(is_valid_framebuffer(framebuffer),
              tprintf("Invalid framebuffer: format=%d, samples=%d\n",
                      framebuffer.color.format, framebuffer.color.samples));
   if (framebuffer.color.samples > 1) {
      int src_width  = src_x1 - src_x0;
      int src_height = src_y1 - src_y0;

      int dst_width  = dst_x1 - dst_x0;
      int dst_height = dst_y1 - dst_y0;

      if (src_width != dst_width || src_height != dst_height) {
         printf("[Error] Blitting MSAA framebuffer with mismatched dimensions (%dx%d vs %dx%d).\n", src_width, src_height, dst_width, dst_height);
         return;
      }
      if (filter != GL_NEAREST) {
         printf("[Warning] Attempting to blit multisampled framebuffer with GL_LINEAR — only GL_NEAREST is allowed for MSAA blits.\n");
      }

      if (mask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
         printf("[Warning] Attempting to blit depth/stencil from multisampled framebuffer — this is not allowed between different sample counts.\n");
      }

   }

   glBlitNamedFramebuffer(
       framebuffer.handle,               // src framebuffer
       0,                                // dst framebuffer (swapchain)
       src_x0, src_y0, src_x1, src_y1,   // source rectangle
       dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
       mask,                             // GL_COLOR_BUFFER_BIT, etc.
       filter                            // GL_NEAREST or GL_LINEAR
   );
}



inline void blit_framebuffer_to_swapchain_rect_src_and_dst(
    const Framebuffer framebuffer,
    const Rectanglei32 src,
    const Rectanglei32 dst,
    GLbitfield mask,
    GLenum filter
) {
   int src_x0 = (GLint)src.x, src_y0 = (GLint)src.y, src_x1 = (GLint)(src.x + src.width), src_y1 = (GLint)(src.y + src.height);
   int dst_x0 = (GLint)dst.x, dst_y0 = (GLint)dst.y, dst_x1 = (GLint)(dst.x + dst.width), dst_y1 = (GLint)(dst.y + dst.height);
   blit_framebuffer_to_swapchain_src_and_dst(
       framebuffer,
       src_x0, src_y0, src_x1, src_y1,   // source rectangle
       dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
       mask,                             // e.g. GL_COLOR_BUFFER_BIT
       filter                            // e.g. GL_NEAREST or GL_LINEAR
   );
}

inline void blit_framebuffer_to_swapchain(const Framebuffer framebuffer) {
   blit_framebuffer_to_swapchain_src_and_dst(
      framebuffer,
      0, 0, framebuffer.color.width, framebuffer.color.height, // source rect
      0, 0, framebuffer.color.width, framebuffer.color.height, // destination rect
      GL_COLOR_BUFFER_BIT, GL_NEAREST
   );
}

inline void blit_framebuffer_to_swapchain_rect(
    const Framebuffer framebuffer, const Rectanglei32 dst
) {
   int dst_x0 = (GLint)dst.x, dst_y0 = (GLint)dst.y, dst_x1 = (GLint)(dst.x + dst.width), dst_y1 = (GLint)(dst.y + dst.height);
   blit_framebuffer_to_swapchain_src_and_dst(
      framebuffer,
      0, 0, framebuffer.color.width, framebuffer.color.height, // source rect
      dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
      GL_COLOR_BUFFER_BIT, GL_NEAREST
   );
}

Framebuffer resolve_multisample_framebuffer(Framebuffer msaa_fb) {
    static Framebuffer static_resolve_fb = {0};

    if (!is_valid_framebuffer(msaa_fb)) {
        trace_error("resolve_multisample_framebuffer: input framebuffer is not valid.\n");
        return (Framebuffer){0};
    }

    const Texture* src = &msaa_fb.color;

    // Not multisampled? Return original
    if (src->samples <= 1) {
        return msaa_fb;
    }

    // Create (or recreate) the static resolve framebuffer if needed
    bool recreate =
        !is_valid_framebuffer(static_resolve_fb) ||
        static_resolve_fb.color.width != src->width ||
        static_resolve_fb.color.height != src->height ||
        static_resolve_fb.color.format != src->format;

    if (recreate) {
        if (is_valid_framebuffer(static_resolve_fb)) {
            destroy_framebuffer(&static_resolve_fb);
        }

        Texture resolved_color = create_texture_extended(
            src->width, src->height,
            nullptr, src->format,
            TEXTURE_TYPE_2D, 1 // single-sample resolve target
        );

        static_resolve_fb = create_framebuffer_from_textures(resolved_color, (Texture){0});
        if (!is_valid_framebuffer(static_resolve_fb)) {
            printf("[Error] resolve_multisample_framebuffer: failed to create resolve framebuffer.\n");
            return (Framebuffer){0};
        }
    }

    // Blit color only — no depth
    glBlitNamedFramebuffer(
        msaa_fb.handle,
        static_resolve_fb.handle,
        0, 0, src->width, src->height,
        0, 0, src->width, src->height,
        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        printf("[Error] glBlitNamedFramebuffer failed during resolve: GL error 0x%X\n", err);
        return (Framebuffer){0};
    }

    return static_resolve_fb;
}

Framebuffer resolve_multisample_framebuffer_old(const Framebuffer* msaa_fb) {
   static Framebuffer static_resolve_fb = {0};

   // Null passed? Use static framebuffer
   if (!msaa_fb) {
      if (!is_valid_framebuffer(static_resolve_fb)) {
         printf("[Error] resolve_multisample_framebuffer: static resolve framebuffer not yet initialized.\n");
      }
      return static_resolve_fb;
   }

   // Validate input framebuffer
   if (!is_valid_framebuffer(*msaa_fb)) {
      printf("[Error] resolve_multisample_framebuffer: input framebuffer is not valid.\n");
      return (Framebuffer){0};
   }

   const Texture* src = &msaa_fb->color;

   // Not multisampled? Just return the input framebuffer
   if (src->samples <= 1) {
      return *msaa_fb;
   }

   // Create the static resolve framebuffer if not yet done or size mismatch
   if (!is_valid_framebuffer(static_resolve_fb) ||
      static_resolve_fb.color.width != src->width ||
      static_resolve_fb.color.height != src->height) {

      if (is_valid_framebuffer(static_resolve_fb)) {
         destroy_framebuffer(&static_resolve_fb);
      }

      Texture resolved_color = create_texture_extended(
         src->width, src->height,
         NULL, src->format,
         TEXTURE_TYPE_2D, 1
      );

      Texture resolved_depth = create_texture_extended(
         src->width, src->height,
         NULL, TEXTURE_FORMAT_DEPTH24,
         TEXTURE_TYPE_2D, 1
      );

      static_resolve_fb = create_framebuffer_from_textures(resolved_color, resolved_depth);

      if (!is_valid_framebuffer(static_resolve_fb)) {
         printf("[Error] resolve_multisample_framebuffer: failed to create resolve framebuffer.\n");
         return (Framebuffer){0};
      }
   }

   // Blit from MSAA framebuffer to resolved framebuffer
   glBlitNamedFramebuffer(
      msaa_fb->handle,
      static_resolve_fb.handle,
      0, 0, src->width, src->height,
      0, 0, src->width, src->height,
      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
      GL_NEAREST
   );

   GLenum err = glGetError();
   if (err != GL_NO_ERROR) {
      printf("[Error] glBlitNamedFramebuffer failed during resolve: GL error 0x%X\n", err);
      return (Framebuffer){0};
   }

   return static_resolve_fb;
}

void blend_framebuffers(const Framebuffer *a, const Framebuffer *b, Framebuffer *out) {
   assert(is_valid_framebuffer(*a));
   assert(is_valid_framebuffer(*b));
   assert(a->color.width == b->color.width && a->color.height == b->color.height);

   static Framebuffer blend_fb = {0};
   static Shader blend_shader = {0};

   // Lazy init output framebuffer if `out` is NULL
   if (!out) {
      if (!is_valid_framebuffer(blend_fb)) {
         Texture out_color = create_texture_extended(a->color.width, a->color.height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, 1);
         Texture out_depth = create_texture_extended(a->color.width, a->color.height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, 1);
         blend_fb = create_framebuffer_from_textures(out_color, out_depth);
      }
      out = &blend_fb;
   }

   // Shader creation (only once)
   if (!is_valid_shader(blend_shader)) {
      const u8 *srcs[] = {
         (u8 *)
         "#version 450 core\n"
         "out vec2 uv;\n"
         "void main() {\n"
         "   const vec2 pos[3] = vec2[3](vec2(-1, -1), vec2(3, -1), vec2(-1, 3));\n"
         "   gl_Position = vec4(pos[gl_VertexID], 0, 1);\n"
         "   uv = (gl_Position.xy + 1.0) * 0.5;\n"
         "}",


         (u8 *)
         "#version 450 core\n"
         "in vec2 uv;\n"
         "layout(location = 0) out vec4 fragColor;\n"
         "layout(binding = 0) uniform sampler2D texA;\n"
         "layout(binding = 1) uniform sampler2D texB;\n"
         "void main() {\n"
         "    vec4 a = texture(texA, uv);\n"
         "    vec4 b = texture(texB, uv);\n"
         "    fragColor = mix(a, b, b.a); // Blend based on texB alpha\n"
         "}"
      };
      Shader_Type types[] = {SHADER_TYPE_VERTEX, SHADER_TYPE_FRAGMENT};
      blend_shader = create_shader_from_memory(srcs, types, count_of(types));
   }

   // Set output framebuffer
   glBindFramebuffer(GL_FRAMEBUFFER, out->handle);
   glViewport(0, 0, out->color.width, out->color.height);

   glUseProgram(blend_shader.handle);

   // Bind inputs
   glBindTextureUnit(0, a->color.handle);
   glBindTextureUnit(1, b->color.handle);

   // Render fullscreen triangle
   glDrawArrays(GL_TRIANGLES, 0, 3);
}


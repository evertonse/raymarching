

// TODO: Add 16F when hdr pipeline and test performance compared to 32F
typedef enum {
   TEXTURE_FORMAT_UNDEFINED,
   TEXTURE_FORMAT_DEPTH24,
   TEXTURE_FORMAT_SHADOW,
   TEXTURE_FORMAT_RGBA32F,
   TEXTURE_FORMAT_RGB8,
   TEXTURE_FORMAT_RGBA8,
   TEXTURE_FORMAT_RG8,
   TEXTURE_FORMAT_R8,
} Texture_Format;

typedef enum {
   TEXTURE_TYPE_UNDEFINED,
   TEXTURE_TYPE_2D,
   TEXTURE_TYPE_2D_MIPMAPPED,
   TEXTURE_TYPE_BUFFER,
} Texture_Type;

typedef enum {
   TEXTURE_ACCESS_READ =  1 << 0,
   TEXTURE_ACCESS_WRITE = 1 << 1,
} Texture_Access;

typedef struct {
   GLuint   handle;

#if defined(RENDERER_USING_BINDLESS)
   GLuint64 bindless_handle;
#endif

   ZString  path;
   // This line was purposefully left in blank (e.g. for padding the struct to correct blank aligment, necesasry for maching gpu texture /s

   i32 width;
   i32 height;
   i32 samples;
   // This line was purposefully left in blank

   Texture_Format format;
   Texture_Type   type;
} Texture;


inline bool is_valid_texture(Texture texture) {
    if (0 == texture.handle) return false;
    if (texture.width <= 0 || texture.height <= 0) return false;
    if (TEXTURE_FORMAT_UNDEFINED == texture.format) return false;
    if (TEXTURE_TYPE_UNDEFINED == texture.type) return false;
    // Actual OpenGL state check (costly, use only in debug)
    #if defined(RENDERER_DEBUG)
      return glIsTexture(texture.handle);
    #else
      return true;
    #endif
}


Texture create_texture_extended(int width, int height, void *data, Texture_Format format, Texture_Type type, int samples) {
   Texture result = {
      .width   = width,
      .height  = height,
      .format  = format,
      .type    = type,
      .samples = samples
   };

   if (1 == samples) {
      trace_warn("Are you sure you wanna create a multisample texture with %d sample? Why, I'm interested", samples);
   }

   GLenum data_type       = GL_UNSIGNED_BYTE;
   GLenum internal_format = 0,         gl_format    = 0;
   GLenum compare_mode    = 0,         compare_func = 0;
   GLint  min_filter      = GL_LINEAR, mag_filter   = GL_LINEAR;
   bool   buffer_backed   = false,     mipmapped    = false;

   // Format decoding
   switch (format) {
      case TEXTURE_FORMAT_RGBA8:   internal_format = GL_RGBA8;             gl_format = GL_RGBA;            break;
      case TEXTURE_FORMAT_RGB8:    internal_format = GL_RGB8;              gl_format = GL_RGB;             break;
      case TEXTURE_FORMAT_RG8:     internal_format = GL_RG8;               gl_format = GL_RG;              break;
      case TEXTURE_FORMAT_R8:      internal_format = GL_R8;                gl_format = GL_RED;             break;
      case TEXTURE_FORMAT_RGBA32F: internal_format = GL_RGBA32F;           gl_format = GL_RGBA;            data_type = GL_FLOAT;        break;
      case TEXTURE_FORMAT_DEPTH24: internal_format = GL_DEPTH_COMPONENT24; gl_format = GL_DEPTH_COMPONENT; data_type = GL_UNSIGNED_INT; break;
      case TEXTURE_FORMAT_SHADOW:  internal_format = GL_DEPTH_COMPONENT24; gl_format = GL_DEPTH_COMPONENT; data_type = GL_UNSIGNED_INT; compare_mode = GL_COMPARE_REF_TO_TEXTURE; compare_func = GL_LEQUAL; break;
      default: assert_msg(false, "Unsupported texture format"); return result;
   }

   // Type decoding
   switch (type) {
      case  TEXTURE_TYPE_BUFFER:       buffer_backed = true; break;
      case  TEXTURE_TYPE_2D_MIPMAPPED: mipmapped     = true; break;
      case  TEXTURE_TYPE_2D:                                 break;

      default: assert_msg(false,"Unsupported  texture type"); return result;
   }

   // Create handle
   if (buffer_backed) {
      glCreateTextures(GL_TEXTURE_BUFFER, 1, &result.handle);
   } else if (samples >= 1) {
      glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &result.handle);
   } else {
      glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);
   }

   GLenum error = glGetError();
   if (error != GL_NO_ERROR) {
      const char *errorMessage;
      switch (error) {
         case GL_INVALID_ENUM:      errorMessage = "Invalid enum value."; break;
         case GL_INVALID_VALUE:     errorMessage = "Invalid value.     "; break;
         case GL_INVALID_OPERATION: errorMessage = "Invalid operation. "; break;
         case GL_OUT_OF_MEMORY:     errorMessage = "Out of memory.     "; break;
         default:                   errorMessage = "Unknown error.";      break;
      }
      trace_error("Error creating texture: %s\n", errorMessage);
      return (Texture){0};
   }

   // Allocate storage
   if (buffer_backed) {
      // This kinda of Texture has its storage associated via glTextureBuffer at some other point in the code
   } else if (samples >= 1) {
      glTextureStorage2DMultisample(result.handle, samples, internal_format, width, height, GL_TRUE);
   } else {
      assert(0 != result.handle);
      glTextureStorage2D(result.handle, 1, internal_format, width, height);

      if (data && (format != TEXTURE_FORMAT_DEPTH24 && format != TEXTURE_FORMAT_SHADOW)) {
         glTextureSubImage2D(result.handle, 0, 0, 0, width, height, gl_format, data_type, data);
      }

      if (mipmapped) {
         glGenerateTextureMipmap(result.handle);
         min_filter = GL_LINEAR_MIPMAP_LINEAR;
      }
   }

   // Apply comparison mode
   if (compare_mode) {
      glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_MODE, compare_mode);
      glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_FUNC, compare_func);
   }

   if (!buffer_backed && samples < 1) {
      glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, min_filter);
      glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, mag_filter);

      // TODO: When loading certain models each texture has WRAP mode for u and v we need to make sure we set that shit correctly
      // HACK: Set to REAPEAT as we know most models prefer that (Symmetry seems to play a role on that)
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S, GL_REPEAT); // Before was this GL_CLAMP_TO_EDGE but didn't work with Alleya.fbx model.
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T, GL_REPEAT);

      #if defined(RENDERER_USING_BINDLESS)
         result.bindless_handle = glGetTextureHandleARB(result.handle);
         glMakeTextureHandleResidentARB(result.bindless_handle);
         assert_msg(0 != result.bindless_handle, "Texture bindless handle 0 is considered invalid. Is zero possibly valid? ", result.bindless_handle);
      #endif
   }

   return result;
}


inline Texture create_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, 0);
}

inline Texture create_texture_multisample(int width, int height, int samples) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, samples);
}

inline Texture create_depth_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, 0);
}

inline Texture create_depth_texture_multisample(int width, int height, int samples) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, samples);
}

inline Texture create_shadow_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_SHADOW, TEXTURE_TYPE_2D, 0);
}

Texture create_texture_from_filepath(const char *filepath) {
   if (!filepath) {
      trace_error("Trying to create texture from null path");
      return (Texture){0};
   }

   if(!file_exists(filepath)) {
      trace_error("Trying to create texture from `%s` inexistent path", filepath);
      return (Texture){0};
   }

   int width, height, channels;
   stbi_set_flip_vertically_on_load(true);
   unsigned char *data = stbi_load(filepath, &width, &height, &channels, 0);

   if (!data) {
      trace_error("Failed to load texture from: %s\n", filepath);
      return (Texture){0};
   }

   Texture_Format format = TEXTURE_FORMAT_RGBA8;
   switch (channels) {
   case 4: format = TEXTURE_FORMAT_RGBA8; break;
   case 3: format = TEXTURE_FORMAT_RGB8 ; break;
   case 2: format = TEXTURE_FORMAT_RG8  ; break;
   case 1: format = TEXTURE_FORMAT_R8   ; break;
   default:
      assert_msg(false, "Unsupported texture channel count from image");
      break;
   }

   Texture result = create_texture_extended(width, height, data, format, TEXTURE_TYPE_2D_MIPMAPPED, 0);
   // NOTE: I'm usure if the texture should hold this memory or not. A lota of times an externable memory is already alocatted idk.
   result.path = filepath;

#if defined(RENDERER_USING_BINDLESS)
   trace_info("'%s' %dx%d handle = %d bindless_handle = 0x%x loaded.", result.path, result.width, result.height, result.handle, result.bindless_handle);
#endif

   stbi_image_free(data);
   return result;
}

void destroy_texture(Texture *texture) {
   #if defined(RENDERER_USING_BINDLESS)
      // NOTE: We're assuming it's always resident if there is a bindless handle
      if (texture->bindless_handle) {
         glMakeTextureHandleNonResidentARB(texture->bindless_handle);
      }
   #endif
   glDeleteTextures(1, &texture->handle);
   *texture = (Texture){0};
}

void update_texture(Texture* tex, int new_width, int new_height, const void* new_data) {
    assert(tex && tex->handle);
    assert(new_width <= tex->width && new_height <= tex->height);

    GLenum format = 0, type = GL_UNSIGNED_BYTE;
    switch (tex->format) {
        case TEXTURE_FORMAT_RGBA8:     format = GL_RGBA; break;
        case TEXTURE_FORMAT_RGB8:      format = GL_RGB; break;
        case TEXTURE_FORMAT_RG8:       format = GL_RG; break;
        case TEXTURE_FORMAT_R8:        format = GL_RED; break;
        case TEXTURE_FORMAT_RGBA32F:   format = GL_RGBA; type = GL_FLOAT; break;
        case TEXTURE_FORMAT_DEPTH24:   format = GL_DEPTH_COMPONENT; type = GL_UNSIGNED_INT; break;
        default:
            assert_msg(false, "Unsupported texture format for subimage update");
            return;
    }

    // Only works for non-multisampled textures
    if (tex->samples > 1) {
        assert_msg(false, "Cannot use SubImage on multisampled textures");
        return;
    }

    glTextureSubImage2D(
        tex->handle,
        0,    // mip level
        0, 0, // xoffset, yoffset
        new_width,
        new_height,
        format,
        type,
        new_data
    );
}

void bind_texture(const Texture texture, usz binding) {
   if (!is_valid_texture(texture)) {
      trace_warn("Trying to bind invalid texture handle = %lld, path %s", texture.handle, texture.path);
      return;
   }

   // Access like this: ``layout(binding = binding) uniform sampler2D texturename;``
   glBindTextureUnit(binding, texture.handle);

   auto error_code = glGetError();
   if (error_code != GL_NO_ERROR) {
      trace_error( "OpenGL Error (%d) in %s!\n", error_code, __func__);
      debug_break();
   }
}

void bind_texture_as_image(const Texture texture, usz binding, Texture_Access access) {
    assert(texture.handle != 0);

    if (texture.type == TEXTURE_TYPE_BUFFER) {
        trace_error("%s: Cannot bind buffer textures as images.\n", __func__);
        return;
    }

    if (texture.samples > 1) {
        trace_error("%s: Multisample textures cannot be bound as image units.\n", __func__);
        return;
    }

    GLenum format = 0;

    switch (texture.format) {
        case TEXTURE_FORMAT_RGBA32F: format = GL_RGBA32F; break;
        case TEXTURE_FORMAT_RGBA8:   format = GL_RGBA8;   break;
        case TEXTURE_FORMAT_RGB8:    format = GL_RGB8;    break;
        case TEXTURE_FORMAT_RG8:     format = GL_RG8;     break;
        case TEXTURE_FORMAT_R8:      format = GL_R8;      break;

        default:
            trace_error("%s: Unsupported or invalid format for image binding (%d).\n", __func__, texture.format);
            return;
    }
    GLenum gl_access = GL_READ_ONLY; // default
    if ((access & TEXTURE_ACCESS_READ) && (access & TEXTURE_ACCESS_WRITE)) {
        gl_access = GL_READ_WRITE;
    } else if (access & TEXTURE_ACCESS_WRITE) {
        gl_access = GL_WRITE_ONLY;
    } else if (access & TEXTURE_ACCESS_READ) {
        gl_access = GL_READ_ONLY;
    } else {
        trace_warn("%s: No valid access flags set, defaulting to read only.\n", __func__);
    }

    int level = 0, layer = 0;
    bool is_layered = GL_FALSE;
    glBindImageTexture(binding, texture.handle, level, is_layered, layer, gl_access, format);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        trace_error("[OpenGL Error] glBindImageTexture failed (0x%X) for binding=%u, format=%d, access=%d\n", err, binding, texture.format, access);
    }
}



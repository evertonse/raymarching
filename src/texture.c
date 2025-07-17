

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

typedef struct {
   GLuint handle;
   i32 width;
   i32 height;
   i32 samples;
   Texture_Format format;
   Texture_Type type;
} Texture;

inline bool is_valid_texture(Texture texture) {
    if (0 == texture.handle) return false;
    if (texture.width <= 0 || texture.height <= 0) return false;
    if (TEXTURE_FORMAT_UNDEFINED == texture.format) return false;
    if (TEXTURE_TYPE_UNDEFINED == texture.type) return false;
    // Actual OpenGL state check (costly, use only in debug)
    #ifdef _DEBUG
      return glIsTexture(tex.handle);
    #else
      return true;
    #endif
}


Texture create_texture_extended(int width, int height, void *data, Texture_Format format, Texture_Type type, int samples) {
   Texture result = {0};
   result.width  = width;
   result.height = height;
   result.format = format;
   result.type   = type;
   result.samples = samples;

   GLenum internal_format, gl_format;
   GLenum data_type = GL_UNSIGNED_BYTE;
   GLenum compare_mode = 0, compare_func = 0;
   GLint min_filter = GL_LINEAR, mag_filter = GL_LINEAR;
   bool buffer_backed = false, mipmapped = false;

   // Format decoding
   switch (format) {
      case TEXTURE_FORMAT_RGBA8:      internal_format = GL_RGBA8; gl_format = GL_RGBA; break;
      case TEXTURE_FORMAT_RGB8:       internal_format = GL_RGB8;  gl_format = GL_RGB;  break;
      case TEXTURE_FORMAT_RG8:        internal_format = GL_RG8;   gl_format = GL_RG;   break;
      case TEXTURE_FORMAT_R8:         internal_format = GL_R8;    gl_format = GL_RED;  break;
      case TEXTURE_FORMAT_RGBA32F:    internal_format = GL_RGBA32F; gl_format = GL_RGBA; data_type = GL_FLOAT; break;
      case TEXTURE_FORMAT_DEPTH24:    internal_format = GL_DEPTH_COMPONENT24; gl_format = GL_DEPTH_COMPONENT; data_type = GL_UNSIGNED_INT; break;
      case TEXTURE_FORMAT_SHADOW:     internal_format = GL_DEPTH_COMPONENT24; gl_format = GL_DEPTH_COMPONENT; data_type = GL_UNSIGNED_INT; compare_mode = GL_COMPARE_REF_TO_TEXTURE; compare_func = GL_LEQUAL; break;
      default: assert_msg(false, "Unsupported texture format"); return result;
   }

   // Type decoding
   switch (type) {
      case TEXTURE_TYPE_BUFFER: buffer_backed = true; break;
      case TEXTURE_TYPE_2D_MIPMAPPED: mipmapped = true; break;
      case TEXTURE_TYPE_2D: break;
      default: assert_msg(false, "Unsupported texture type"); return result;
   }

   // Create handle
   if (buffer_backed) {
      glCreateTextures(GL_TEXTURE_BUFFER, 1, &result.handle);
   } else if (samples > 1) {
      glCreateTextures(GL_TEXTURE_2D_MULTISAMPLE, 1, &result.handle);
   } else {
      glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);
   }

   // Allocate storage
   if (buffer_backed) {
      // Texture buffer storage is done via glTextureBuffer later
   } else if (samples > 1) {
      glTextureStorage2DMultisample(result.handle, samples, internal_format, width, height, GL_TRUE);
   } else {
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

   if (!buffer_backed && samples <= 1) {
      glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, min_filter);
      glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, mag_filter);
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
   }

   return result;
}


inline Texture create_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, 1);
}

inline Texture create_texture_multisample(int width, int height, int samples) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, samples);
}

inline Texture create_depth_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, 1);
}

inline Texture create_depth_texture_multisample(int width, int height, int samples) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, TEXTURE_TYPE_2D, samples);
}

inline Texture create_shadow_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_SHADOW, TEXTURE_TYPE_2D, 1);
}

Texture create_texture_from_filepath(const char *filepath) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(filepath, &width, &height, &channels, 0);

    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", filepath);
        return (Texture){0};
    }

    Texture_Format format = TEXTURE_FORMAT_RGBA8;
    switch (channels) {
        case 4: format = TEXTURE_FORMAT_RGBA8; break;
        case 3: format = TEXTURE_FORMAT_RGB8;  break;
        case 2: format = TEXTURE_FORMAT_RG8;   break;
        case 1: format = TEXTURE_FORMAT_R8;    break;
        default:
            assert_msg(false, "Unsupported texture channel count from image");
            break;
    }

    Texture result = create_texture_extended(width, height, data, format, TEXTURE_TYPE_2D_MIPMAPPED, 1);
    stbi_image_free(data);
    return result;
}

void destroy_texture(Texture* texture) {
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
        0, // mip level
        0, 0, // xoffset, yoffset
        new_width,
        new_height,
        format,
        type,
        new_data
    );
}


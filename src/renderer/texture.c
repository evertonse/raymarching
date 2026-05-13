
// TODO: Add 16F when hdr pipeline and test performance compared to 32F
typedef enum {
   TEXTURE_FORMAT_UNDEFINED,
   TEXTURE_FORMAT_DEPTH24,
   TEXTURE_FORMAT_SHADOW,
   TEXTURE_FORMAT_RGBA32F,
   TEXTURE_FORMAT_R32F,
   TEXTURE_FORMAT_RGB8,
   TEXTURE_FORMAT_RGBA8,
   TEXTURE_FORMAT_RG8,
   TEXTURE_FORMAT_R8,
} Texture_Format;

typedef enum {
   TEXTURE_TYPE_UNDEFINED,
   TEXTURE_TYPE_2D,
   TEXTURE_TYPE_2D_MIPMAPPED,
   TEXTURE_TYPE_2D_MULTISAMPLED_2X,
   TEXTURE_TYPE_2D_MULTISAMPLED_4X,
   TEXTURE_TYPE_2D_MULTISAMPLED_8X,
   TEXTURE_TYPE_2D_MULTISAMPLED_16X,
   TEXTURE_TYPE_BUFFER,
} Texture_Type;

typedef enum {
   TEXTURE_FILTER_NONE = 0,

   TEXTURE_FILTER_BILINEAR,

   TEXTURE_FILTER_TRILINEAR,

   TEXTURE_FILTER_ANISOTROPIC_4X,
   TEXTURE_FILTER_ANISOTROPIC_8X,
   TEXTURE_FILTER_ANISOTROPIC_16X,

} Texture_Filter;

typedef enum {
   TEXTURE_WRAP_REPEAT = 0,
   TEXTURE_WRAP_CLAMP_EDGE,
   TEXTURE_WRAP_CLAMP_BORDER,
   TEXTURE_WRAP_MIRRORED_REPEAT,
} Texture_Wrap;

typedef enum {
   TEXTURE_ACCESS_READ  = 1 << 0,
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
   // This line was purposefully left in blank

   Texture_Filter filter;
   Texture_Wrap   wrap;
   Texture_Format format;
   Texture_Type   type;
} Texture;


inline bool is_valid_texture(Texture texture) {
    if (0 == texture.handle)                        return false;
    if (texture.width <= 0 || texture.height <= 0)  return false;
    if (TEXTURE_FORMAT_UNDEFINED == texture.format) return false;
    if (TEXTURE_TYPE_UNDEFINED   == texture.type)   return false;

    // Actual OpenGL state check (costly, use only in debug)
    #if defined(RENDERER_DEBUG)
      return glIsTexture(texture.handle);
    #else
      return true;
    #endif
}


void destroy_texture(Texture *texture) {

   if (!texture) {
      trace_warn("destroy_texture: texture pointer is null.");
      return;
   }

   if (!is_valid_texture(*texture)) {
      trace_debug("Trying to destroy texture that isn't valid (%s)", texture->path ? texture->path : "null path");
      return;
   }

#if defined(RENDERER_USING_BINDLESS)
   // Make non-resident before deletion.
   // Required by ARB_bindless_texture spec.
   if (texture->bindless_handle != 0) {

      GLboolean resident = glIsTextureHandleResidentARB(texture->bindless_handle);

      if (resident) {
         glMakeTextureHandleNonResidentARB(texture->bindless_handle);
      }

      texture->bindless_handle = 0;
   }
#endif

#if defined(RENDERER_DEBUG)

   // Validate before deletion to catch double-frees or corrupted handles
   if (!glIsTexture(texture->handle)) {

      trace_warn(
         "%s: handle %u is not a valid OpenGL texture.", __func__,
         texture->handle
      );

   } else {

      trace_debug(
         "%s: deleting texture %u (%dx%d)", __func__,
         texture->handle,
         texture->width,
         texture->height
      );
   }

#endif

   glDeleteTextures(1, &texture->handle);

   GLenum err = glGetError();

   if (err != GL_NO_ERROR) {

      trace_error(
         "%s: glDeleteTextures failed for handle %u (0x%X)", __func__,
         texture->handle,
         err
      );
   }

   *texture = (Texture){0};
}

Texture create_texture(int width, int height, void *data, Texture_Format format, Texture_Type type, Texture_Filter filter, Texture_Wrap wrap) {
   Texture result = {
       .width  = width,
       .height = height,
       .format = format,
       .type   = type,
       .filter = filter,
       .wrap   = wrap,
   };

   if (width <= 0 || height <= 0) {
      trace_error("%s: invalid texture size %dx%d", __func__, width, height);
      return (Texture){0};
   }

   // Decode texture type
   GLenum target  = GL_TEXTURE_2D;
   bool is_mipmapped    = false;
   bool is_multisampled = false;
   int  samples = 0;

   switch (type) {
   case TEXTURE_TYPE_2D:                  target = GL_TEXTURE_2D;                                                   break;
   case TEXTURE_TYPE_2D_MIPMAPPED:        target = GL_TEXTURE_2D; is_mipmapped = true;                              break;
   case TEXTURE_TYPE_2D_MULTISAMPLED_2X:  target = GL_TEXTURE_2D_MULTISAMPLE; is_multisampled = true; samples =  2; break;
   case TEXTURE_TYPE_2D_MULTISAMPLED_4X:  target = GL_TEXTURE_2D_MULTISAMPLE; is_multisampled = true; samples =  4; break;
   case TEXTURE_TYPE_2D_MULTISAMPLED_8X:  target = GL_TEXTURE_2D_MULTISAMPLE; is_multisampled = true; samples =  8; break;
   case TEXTURE_TYPE_2D_MULTISAMPLED_16X: target = GL_TEXTURE_2D_MULTISAMPLE; is_multisampled = true; samples = 16; break;
   default: {
      trace_error("%s: unsupported texture type", __func__);
      return (Texture){0};
   }
   }

   // MSAA textures cannot use mipmapped filters
   assert_msg(!(true == is_mipmapped && true == is_multisampled),"We should have caught that in the first switch on the type");

   // Format
   GLenum internal_format = 0, gl_format = 0, data_type = GL_UNSIGNED_BYTE;
   bool is_depth  = false;
   bool is_shadow = false;
   switch (format) {
   case TEXTURE_FORMAT_RGBA8: {
      internal_format = GL_RGBA8;
      gl_format       = GL_RGBA;
      break;
   }
   case TEXTURE_FORMAT_RGB8: {
      internal_format = GL_RGB8;
      gl_format       = GL_RGB;
      break;
   }
   case TEXTURE_FORMAT_RG8: {
      internal_format = GL_RG8;
      gl_format       = GL_RG;
      break;
   }
   case TEXTURE_FORMAT_R8: {
      internal_format = GL_R8;
      gl_format       = GL_RED;
      break;
   }
   case TEXTURE_FORMAT_RGBA32F: {
      internal_format = GL_RGBA32F;
      gl_format       = GL_RGBA;
      data_type       = GL_FLOAT;
      break;
   }
   case TEXTURE_FORMAT_R32F: {
      internal_format = GL_R32F;
      gl_format       = GL_RED;
      data_type       = GL_FLOAT;
      break;
   }
   case TEXTURE_FORMAT_DEPTH24: {
      internal_format = GL_DEPTH_COMPONENT24;
      gl_format       = GL_DEPTH_COMPONENT;
      data_type       = GL_UNSIGNED_INT;
      is_depth        = true;
      break;
   }
   case TEXTURE_FORMAT_SHADOW: {
      internal_format = GL_DEPTH_COMPONENT24;
      gl_format       = GL_DEPTH_COMPONENT;
      data_type       = GL_UNSIGNED_INT;
      is_depth        = true;
      is_shadow       = true;
      break;
   }
   default: {
      trace_error("create_texture: unsupported texture format");
      return (Texture){0};
   }
   }

   // Create texture object
   glCreateTextures(target, 1, &result.handle);

   if (!result.handle) {
      trace_error("%s glCreateTextures failed", __func__);
      return (Texture){0};
   }


   // Allocate storage
   if (is_multisampled) {
      glTextureStorage2DMultisample(result.handle, samples, internal_format, width, height, GL_TRUE);
   } else {

      int mip_levels = 1;

      if (is_mipmapped) {
         mip_levels = (int)floorf(log2f((float)max(width, height))) + 1;
      }

      glTextureStorage2D(result.handle, mip_levels, internal_format, width, height);

      if (data) {
         assert_msg(!is_depth && !is_shadow && !is_multisampled, "Tell me is it possible to want to have starting data in a multisamples texture or depth/shadow texute?");
         glTextureSubImage2D(result.handle, 0, 0, 0, width, height, gl_format, data_type, data);
      }

      if (is_mipmapped) {
         glGenerateTextureMipmap(result.handle);
      }
   }

   if (!is_multisampled) {
      GLenum  min_filter = GL_LINEAR;
      GLenum  mag_filter = GL_LINEAR;
      GLfloat anisotropy = 1.0f;
      switch (result.filter) {
      case TEXTURE_FILTER_NONE: {
         min_filter = GL_NEAREST;
         mag_filter = GL_NEAREST;
         break;
      }
      case TEXTURE_FILTER_BILINEAR: {
         min_filter = GL_LINEAR;
         mag_filter = GL_LINEAR;
         break;
      }
      case TEXTURE_FILTER_TRILINEAR:
      case TEXTURE_FILTER_ANISOTROPIC_4X:
      case TEXTURE_FILTER_ANISOTROPIC_8X:
      case TEXTURE_FILTER_ANISOTROPIC_16X: {
         min_filter = GL_LINEAR_MIPMAP_LINEAR;
         mag_filter = GL_LINEAR;
         break;
      }
      default: {
         trace_error("%s unsupported filter", __func__);
         glDeleteTextures(1, &result.handle);
         return (Texture){0};
      }
      }

      glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, min_filter);
      glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, mag_filter);

      if      (TEXTURE_FILTER_ANISOTROPIC_4X  == result.filter) anisotropy = 4.f;
      else if (TEXTURE_FILTER_ANISOTROPIC_8X  == result.filter) anisotropy = 8.f;
      else if (TEXTURE_FILTER_ANISOTROPIC_16X == result.filter) anisotropy = 16.f;

      if (anisotropy > 1.0f) {
         if (GLAD_GL_EXT_texture_filter_anisotropic || GLAD_GL_ARB_texture_filter_anisotropic) {
            GLfloat max_supported = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_supported);
            anisotropy = min(anisotropy, max_supported);
            glTextureParameterf(result.handle, GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
         } else {
            trace_warn("%s: Anisotropy filtering ain't supported", __func__);
         }
      }


      GLenum gl_wrap = GL_REPEAT;
      switch (wrap) {
      case TEXTURE_WRAP_REPEAT:          gl_wrap = GL_REPEAT;          break;
      case TEXTURE_WRAP_CLAMP_EDGE:      gl_wrap = GL_CLAMP_TO_EDGE;   break;
      case TEXTURE_WRAP_CLAMP_BORDER:    gl_wrap = GL_CLAMP_TO_BORDER; break;
      case TEXTURE_WRAP_MIRRORED_REPEAT: gl_wrap = GL_MIRRORED_REPEAT; break;
      default: {
         trace_error("%s: unsupported wrap mode", __func__);
         glDeleteTextures(1, &result.handle);
         return (Texture){0};
      }
      }
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S, gl_wrap);
      glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T, gl_wrap);

      // Shadow compare mode
      if (is_shadow) {
         glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
         glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
      }

   }

   // Bindless only is normalish 2D texture
#if defined(RENDERER_USING_BINDLESS)
   if (TEXTURE_TYPE_2D_MIPMAPPED == result.type || TEXTURE_TYPE_2D == result.type) {
      result.bindless_handle = glGetTextureHandleARB(result.handle);
      glMakeTextureHandleResidentARB(result.bindless_handle);
   }
#endif

   // Validation
   GLenum err = glGetError();
   if (err != GL_NO_ERROR) {
      trace_error("%s OpenGL error 0x%X", __func__, err);
      destroy_texture(&result);
      return (Texture){0};
   }

   return result;
}


inline Texture overload create_texture(int width, int height) {
   return create_texture(width, height, nullptr, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, TEXTURE_FILTER_TRILINEAR, TEXTURE_WRAP_REPEAT);
}

// Returns 0 if not multisampled else returns samples count.
int texture_multisamples(const Texture texture) {
   int samples =  0;
   if      (TEXTURE_TYPE_2D_MULTISAMPLED_2X  == texture.type) samples =  2;
   else if (TEXTURE_TYPE_2D_MULTISAMPLED_4X  == texture.type) samples =  4;
   else if (TEXTURE_TYPE_2D_MULTISAMPLED_8X  == texture.type) samples =  8;
   else if (TEXTURE_TYPE_2D_MULTISAMPLED_16X == texture.type) samples = 16;
   return samples;
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
   u8 *data = nullptr;
   bool stbi_need_free = false;
   Cye_DString ds = {0};
   if (true) {
      data = stbi_load(filepath, &width, &height, &channels, 0);
      stbi_need_free = true;
   } else {
      ds_read_file(filepath, &ds);
      data = ds.data;
      stbi_load_from_memory(ds.data, ds.size, &width, &height, &channels, 0);
   }

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

   Texture result = create_texture(width, height, data, format, TEXTURE_TYPE_2D_MIPMAPPED, TEXTURE_FILTER_ANISOTROPIC_16X, TEXTURE_WRAP_REPEAT);
   // NOTE: I'm usure if the texture should hold this memory or not. A lota of times an externable memory is already alocatted idk.
   result.path = filepath;

#if defined(RENDERER_USING_BINDLESS)
   trace_info("'%s' %dx%d handle = %d bindless_handle = 0x%x loaded.", result.path, result.width, result.height, result.handle, result.bindless_handle);
#endif

   if (stbi_need_free) {
      stbi_image_free(data);
   } else {
      ds_free(ds);
   }

   return result;
}


inline Texture overload create_texture(const char *filepath) {
   return create_texture_from_filepath(filepath);
}


void update_texture(Texture* texture, int new_width, int new_height, const void* new_data) {
   assert(texture && texture->handle);
   assert(new_width <= texture->width && new_height <= texture->height);
   
   GLenum format = 0, type = GL_UNSIGNED_BYTE;
   switch (texture->format) {
      case TEXTURE_FORMAT_RGBA8:     format = GL_RGBA;                                    break;
      case TEXTURE_FORMAT_RGB8:      format = GL_RGB;                                     break;
      case TEXTURE_FORMAT_RG8:       format = GL_RG;                                      break;
      case TEXTURE_FORMAT_R8:        format = GL_RED;                                     break;
      case TEXTURE_FORMAT_RGBA32F:   format = GL_RGBA;            type = GL_FLOAT;        break;
      case TEXTURE_FORMAT_R32F:      format = GL_RED;             type = GL_FLOAT;        break;
      case TEXTURE_FORMAT_DEPTH24:   format = GL_DEPTH_COMPONENT; type = GL_UNSIGNED_INT; break;
      default: assert_msg(false, "Unsupported texture format for subimage update");       return;
   }
   
   // Only works for non-multisampled textures
   if (texture_multisamples(*texture) > 1) {
      assert_msg(false, "Cannot use SubImage on multisampled textures");
      return;
   }
   
   glTextureSubImage2D(
      texture->handle,
      0,    // mip level
      0, 0, // xoffset, yoffset
      new_width,
      new_height,
      format,
      type,
      new_data
   );
}


void copy_texture(Texture dst, Texture src) {
   if (!is_valid_texture(src) || !is_valid_texture(dst)) {
      trace_error("%s: invalid texture(s)", __func__);
      return;
   }

   if (TEXTURE_TYPE_2D != src.type || TEXTURE_TYPE_2D != dst.type) {
      trace_error("%s: only 2D textures supported", __func__);
      return;
   }

   if (src.width != dst.width || src.height != dst.height) {
      trace_warn("%s: size mismatch, skipping copy", __func__);
      return;
   }

   // Direct GPU side copy
   glCopyImageSubData(src.handle, GL_TEXTURE_2D, 0, 0, 0, 0, dst.handle, GL_TEXTURE_2D, 0, 0, 0, 0, src.width, src.height, 1);
}


// Access like this: ``layout(binding = binding) uniform sampler2D texturename;``
void bind_texture(const Texture texture, usz binding) {
   if (!is_valid_texture(texture)) {
      trace_warn("Trying to bind invalid texture handle = %lld, path %s", texture.handle, texture.path);
      return;
   }

   glBindTextureUnit(binding, texture.handle);

   auto error_code = glGetError();
   if (error_code != GL_NO_ERROR) {
      trace_error("OpenGL Error (%d) in %s!\n", error_code, __func__);
      debug_break();
   }
}


void bind_texture_as_image(const Texture texture, usz binding, Texture_Access access) {
   assert(texture.handle != 0);

   if (texture.type == TEXTURE_TYPE_BUFFER) {
      trace_error("%s: Cannot bind buffer textures as images.\n", __func__);
      return;
   }

   if (texture_multisamples(texture) > 1) {
      trace_error("%s: Multisample textures cannot be bound as image units.\n", __func__);
      return;
   }

   GLenum format = 0;

   switch (texture.format) {
   case TEXTURE_FORMAT_RGBA32F: format = GL_RGBA32F; break;
   case TEXTURE_FORMAT_R32F: format = GL_R32F; break;
   case TEXTURE_FORMAT_RGBA8: format = GL_RGBA8; break;
   case TEXTURE_FORMAT_RGB8: format = GL_RGB8; break;
   case TEXTURE_FORMAT_RG8: format = GL_RG8; break;
   case TEXTURE_FORMAT_R8: format = GL_R8; break;
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

#define bind_texture_as_sampler bind_texture

bool generate_max_mipmaps(Texture *texture) {

   if (texture == NULL) {
      trace_error("%s: texture null", __func__);
      return false;
   }

   if (!is_valid_texture(*texture)) {
      trace_error("%s: invalid texture", __func__);
      return false;
   }

   if (texture_multisamples(*texture) > 0) {
      trace_error("%s: multisampled not supported", __func__);
      return false;
   }

   if (texture->format != TEXTURE_FORMAT_R32F) {
      trace_warn("%s: converting texture to R32F automatically (using R channel)", __func__);
   }

   GLint existing_levels = 0;
   glGetTextureParameteriv(texture->handle, GL_TEXTURE_IMMUTABLE_LEVELS, &existing_levels);

   if (existing_levels > 1) {
      trace_error("%s: texture already has mipmaps allocated", __func__);
      return false;
   }

   int base_width  = texture->width;
   int base_height = texture->height;

   if (base_width <= 0 || base_height <= 0) {
      trace_error("%s: invalid size", __func__);
      return false;
   }

   int max_dimension = base_width > base_height ? base_width : base_height;
   int mip_count = 1 + (int)floor(log2((float)max_dimension));

   usize base_pixel_count = (usize)base_width * (usize)base_height;

   float *base_level_data = malloc(base_pixel_count * size_of(float));
   if (base_level_data == NULL) {
      trace_error("%s: malloc failed", __func__);
      return false;
   }

   glGetTextureImage(
      texture->handle,
      0,
      GL_RED,
      GL_FLOAT,
      base_pixel_count * size_of(float),
      base_level_data
   );
   for (usize i = 0; i < base_pixel_count; i++) {
      float data = base_level_data[i];
      if (data > 1.0) {
         trace_fatal("givver than 1. %f", data);
      }
   }

   glDeleteTextures(1, &texture->handle);
   glCreateTextures(GL_TEXTURE_2D, 1, &texture->handle);

   glTextureStorage2D(
      texture->handle,
      mip_count,
      GL_R32F,
      base_width,
      base_height
   );

   glTextureSubImage2D(
      texture->handle,
      0,
      0,
      0,
      base_width,
      base_height,
      GL_RED,
      GL_FLOAT,
      base_level_data
   );

   int previous_width  = base_width;
   int previous_height = base_height;

   float *previous_level_data = base_level_data;

   for (int mip_level = 1; mip_level < mip_count; mip_level++) {

      int next_width  = previous_width  > 1 ? previous_width  / 2 : 1;
      int next_height = previous_height > 1 ? previous_height / 2 : 1;

      usize next_pixel_count = (usize)next_width * (usize)next_height;
      float *next_level_data = malloc(next_pixel_count * size_of(float));

      if (next_level_data == NULL) {
         free(previous_level_data);
         trace_error("%s: malloc failed", __func__);
         return false;
      }

      for (int y = 0; y < next_height; y++) {
         for (int x = 0; x < next_width; x++) {

            int base_x = x * 2;
            int base_y = y * 2;

            float maximum_value = -1e30f;

            const int sample_offsets[4][2] = {
               {0, 0},
               {1, 0},
               {0, 1},
               {1, 1}
            };

            for (int i = 0; i < count_of(sample_offsets); i++) {

               int sample_x = base_x + sample_offsets[i][0];
               int sample_y = base_y + sample_offsets[i][1];

               if (sample_x < previous_width && sample_y < previous_height) {

                  float value = previous_level_data[sample_y * previous_width + sample_x];

                  if (value > maximum_value) {
                     maximum_value = value;
                  }
               }
            }

            if (maximum_value > 1.0) {
               trace_fatal("maximum_value > 1. %f (We're expecting range to be from 0.0 to 1.0)", maximum_value);
            }

            next_level_data[y * next_width + x] = maximum_value;
         }
      }

      glTextureSubImage2D(
         texture->handle,
         mip_level,
         0,
         0,
         next_width,
         next_height,
         GL_RED,
         GL_FLOAT,
         next_level_data
      );

      if (mip_level > 1) {
         free(previous_level_data);
      }

      previous_level_data = next_level_data;
      previous_width  = next_width;
      previous_height = next_height;
   }

   free(previous_level_data);

   glTextureParameteri(texture->handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
   glTextureParameteri(texture->handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   texture->format = TEXTURE_FORMAT_R32F;
   return true;
}


bool dump_texture_mips_png(Texture *texture, const char *filename) {
   if (!texture || !is_valid_texture(*texture) || !filename) {
      trace_error("%s: invalid args", __func__);
      return false;
   }

   GLint base_width = 0, base_height = 0;
   glGetTextureLevelParameteriv(texture->handle, 0, GL_TEXTURE_WIDTH, &base_width);
   glGetTextureLevelParameteriv(texture->handle, 0, GL_TEXTURE_HEIGHT, &base_height);

   if (base_width <= 0 || base_height <= 0) {
      trace_error("invalid texture size");
      return false;
   }

   // detect mip count
   int mip_levels = 1;
   int w = base_width, h = base_height;
   while (w > 1 || h > 1) {
      w = (w > 1) ? w / 2 : 1;
      h = (h > 1) ? h / 2 : 1;
      mip_levels++;
   }

   // detect channels
   GLint internal_format;
   glGetTextureLevelParameteriv(texture->handle, 0, GL_TEXTURE_INTERNAL_FORMAT, &internal_format);

   int channels = 1;
   if (internal_format == GL_RG32F || internal_format == GL_RG16F) {
      channels = 2;
   }
   if (internal_format == GL_RGBA32F) {
      channels = 4;
   }

   int total_height = 0;
   w = base_width;
   h = base_height;
   for (int i = 0; i < mip_levels; i++) {
      total_height += h;
      w = (w > 1) ? w / 2 : 1;
      h = (h > 1) ? h / 2 : 1;
   }

   int out_width = base_width;
   int out_height = total_height;

   u8 *png_pixels = malloc((usize)out_width * out_height * 3);
   memset(png_pixels, 0, (usize)out_width * out_height * 3);

   int y_offset = 0;
   w = base_width;
   h = base_height;

   for (int level = 0; level < mip_levels; level++) {

      usize pixel_count = (usize)w * h * channels;
      float *buffer = malloc(pixel_count * size_of(float));

      glGetTextureImage(texture->handle, level, (channels == 1 ? GL_RED : (channels == 2 ? GL_RG : GL_RGBA)), GL_FLOAT, pixel_count * size_of(float), buffer);

      float minv = buffer[0];
      float maxv = buffer[0];

      for (usize i = 0; i < pixel_count; i += channels) {
         float v = buffer[i];

         if (isnan(v) || isinf(v))
            continue;

         if (v < minv)
            minv = v;
         if (v > maxv)
            maxv = v;
      }

      float range = maxv - minv;
      if (range < 1e-8f)
         range = 1.0f;

      for (int y = 0; y < h; y++) {
         for (int x = 0; x < w; x++) {

            float v = buffer[(y * w + x) * channels];

            // remove this if you want raw values visualization
            float n = (v - minv) / range;

            if (n < 0)
               n = 0;
            if (n > 1)
               n = 1;

            u8 c = (u8)(n * 255);

            int out_y = y_offset + y;
            int idx = (out_y * out_width + x) * 3;

            png_pixels[idx + 0] = c;
            png_pixels[idx + 1] = c;
            png_pixels[idx + 2] = c;
         }
      }

      free(buffer);
      y_offset += h;

      w = (w > 1) ? w / 2 : 1;
      h = (h > 1) ? h / 2 : 1;
   }

   int ok = stbi_write_png(filename, out_width, out_height, 3, png_pixels, out_width * 3);

   free(png_pixels);

   if (!ok) {
      trace_error("png write failed");
      return false;
   }

   trace_info("%s wrote %s", __func__, filename);
   return true;
}

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct {
   GLuint handle;
   int32_t width;
   int32_t height;
   Texture_Format format;
} Texture;

typedef struct {
   GLuint handle;
   Texture color, depth;
} Framebuffer;

typedef struct {
   union {
      f32 position[3];
      Vector3 position_v3;
   };

   union {
      f32 normal[3];
      Vector3 normal_v3;
   };
   union {
      f32 uv[2];
      Vector2 uv_v2;
   };
} Vertex;

typedef struct {
    GLuint handle;
    GLuint vbo;
    GLuint ibo;
    GLuint vertex_count;
    GLuint index_count;
} Vertex_Array;

typedef struct {
    GLuint ibo;
    GLuint count;
} Index_Buffer;

typedef struct {
   Vector3 *vertices;
   Vector3 *normals;
   Vector2 *uvs;
   u32     *indices;

   u32 vertices_count;
   u32 uvs_count;
   u32 normals_count;
   u32 indices_count;
} Mesh;

typedef struct {
    i32 x;                // Rectangle top-left corner position x
    i32 y;                // Rectangle top-left corner position y
    i32 width;            // Rectangle width
    i32 height;           // Rectangle height
} Rectanglei32;


inline bool is_valid_shader(Shader shader) {
    return shader.handle != INVALID_SHADER_HANDLE;
}

inline bool is_valid_texture(Texture texture) {
    if (0 == texture.handle) return false;
    if (texture.width <= 0 || texture.height <= 0) return false;
    if (TEXTURE_FORMAT_UNDEFINED == texture.format) return false;
    
    // Actual OpenGL state check (costly, use only in debug)
    #ifdef _DEBUG
      return glIsTexture(tex.handle);
    #else
      return true;
    #endif
}

inline bool is_valid_framebuffer(Framebuffer fb) {
    return fb.handle != 0;
}

inline bool is_valid_framebuffer_and_its_textures(Framebuffer fb) {
    return fb.handle != 0 && is_valid_texture(fb.color) && is_valid_texture(fb.depth);
}

inline bool is_valid_vertex_array(Vertex_Array va) {
    if (va.handle == 0 || va.vbo == 0) return false;
    if (va.vertex_count == 0) return false;
    
    #ifdef _DEBUG
    GLint vao_valid;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_valid);
    return vao_valid == va.handle;
    #else
    return true;
    #endif
}

// Index Buffer is used separately so maybe we shouldnt expose this?
inline bool is_valid_index_buffer(Index_Buffer ib) {
    return ib.ibo != 0 && ib.count > 0;
}

// Mesh
inline bool is_valid_mesh(Mesh mesh) {
    return mesh.vertices != NULL && mesh.indices != NULL &&
           mesh.vertices_count > 0 && mesh.indices_count > 0;
}

// Rectanglei32
inline bool is_valid_rectangle(Rectanglei32 r) {
    return r.width > 0 && r.height > 0;
}

// The last element buffer object that gets bound while a VAO is bound, is stored as the VAO's element buffer object. Binding to a VAO then also automatically binds that EBO.
Vertex_Array create_vertex_array_non_dsa(const Vertex* vertices, usz vertex_count, const u32* indices, usz index_count) {
    Vertex_Array va;
    glGenVertexArrays(1, &va.handle);
    glBindVertexArray(va.handle);

    // Vertex Buffer
    glGenBuffers(1, &va.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, va.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * size_of(Vertex), vertices, GL_STATIC_DRAW);

    // Vertex Buffer attributes
    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, position));

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, normal));

    glEnableVertexAttribArray(2); // uv (text coordinate)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, uv));

    // Index Buffer
    glGenBuffers(1, &va.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(GLuint), indices, GL_STATIC_DRAW);


    glBindVertexArray(0);

    va.vertex_count = (GLuint)vertex_count;
    va.index_count = (GLuint)index_count;
    return va;
}

Vertex_Array create_vertex_array(const Vertex* vertices, usz vertex_count, const u32* indices, usz index_count) {
    Vertex_Array va = {0};

    glCreateVertexArrays(1, &va.handle);
    glCreateBuffers(1, &va.vbo);
    glNamedBufferStorage(va.vbo, vertex_count * size_of(Vertex), vertices, 0);

    glVertexArrayVertexBuffer(va.handle, 0, va.vbo, 0, size_of(Vertex));

    // Vertex attributes
    glEnableVertexArrayAttrib(va.handle, 0);
    glVertexArrayAttribFormat(va.handle, 0, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, position));
    glVertexArrayAttribBinding(va.handle, 0, 0);

    glEnableVertexArrayAttrib(va.handle, 1);
    glVertexArrayAttribFormat(va.handle, 1, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, normal));
    glVertexArrayAttribBinding(va.handle, 1, 0);

    glEnableVertexArrayAttrib(va.handle, 2);
    glVertexArrayAttribFormat(va.handle, 2, 2, GL_FLOAT, GL_FALSE, offset_of(Vertex, uv));
    glVertexArrayAttribBinding(va.handle, 2, 0);

    // Index buffer
    glCreateBuffers(1, &va.ibo);
    glNamedBufferStorage(va.ibo, index_count * sizeof(GLuint), indices, 0);
    glVertexArrayElementBuffer(va.handle, va.ibo);

    va.vertex_count = (GLuint)vertex_count;
    va.index_count = (GLuint)index_count;
    return va;
}


Vertex_Array create_vertex_array_from_mesh(const Mesh *mesh) {
   Vertex_Array va = {0};

   assert(mesh->vertices_count == mesh->normals_count && mesh->vertices_count == mesh->uvs_count);

   // Calculate sizes
   usz vertex_size = mesh->vertices_count * sizeof(Vector3);
   usz normal_size = mesh->normals_count * sizeof(Vector3);
   usz uv_size     = mesh->uvs_count * sizeof(Vector2);
   usz total_size  = vertex_size + normal_size + uv_size;

   // Create VAO
   glCreateVertexArrays(1, &va.handle);

   // Create and upload VBO
   glCreateBuffers(1, &va.vbo);
   glNamedBufferStorage(va.vbo, total_size, NULL, GL_DYNAMIC_STORAGE_BIT);
   glNamedBufferSubData(va.vbo, 0, vertex_size, mesh->vertices);
   glNamedBufferSubData(va.vbo, vertex_size, normal_size, mesh->normals);
   glNamedBufferSubData(va.vbo, vertex_size + normal_size, uv_size, mesh->uvs);

   // Link VBO to VAO (positions)
   glEnableVertexArrayAttrib(va.handle, 0);
   glVertexArrayVertexBuffer(va.handle, 0, va.vbo, 0, sizeof(Vector3));
   glVertexArrayAttribFormat(va.handle, 0, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 0, 0);

   // Normals (offset binding)
   glEnableVertexArrayAttrib(va.handle, 1);
   glVertexArrayVertexBuffer(va.handle, 1, va.vbo, vertex_size, sizeof(Vector3));
   glVertexArrayAttribFormat(va.handle, 1, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 1, 1);

   // UVs
   glEnableVertexArrayAttrib(va.handle, 2);
   glVertexArrayVertexBuffer(va.handle, 2, va.vbo, vertex_size + normal_size, sizeof(Vector2));
   glVertexArrayAttribFormat(va.handle, 2, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 2, 2);

   // Create and upload index buffer
   glCreateBuffers(1, &va.ibo);
   glNamedBufferStorage(va.ibo, mesh->indices_count * sizeof(u32), mesh->indices, 0);
   glVertexArrayElementBuffer(va.handle, va.ibo);

   // Store counts
   va.vertex_count = mesh->vertices_count;
   va.index_count = mesh->indices_count;

   return va;
}


Vertex_Array create_vertex_array_from_mesh_non_dsa(const Mesh *mesh) {
   Vertex_Array va = {0};

   // Create and bind VAO
   glGenVertexArrays(1, &va.handle);
   glBindVertexArray(va.handle);

   // Generate VBO and IBO
   glGenBuffers(1, &va.vbo);
   glGenBuffers(1, &va.ibo);

   // Calculate sizes
   usz vertex_size = mesh->vertices_count * sizeof(Vector3);
   usz normal_size = mesh->normals_count * sizeof(Vector3);
   usz uv_size     = mesh->uvs_count * sizeof(Vector2);
   usz total_size  = vertex_size + normal_size + uv_size;

   assert(mesh->vertices_count == mesh->normals_count && mesh->vertices_count == mesh->uvs_count);

   // Upload VBO data
   glBindBuffer(GL_ARRAY_BUFFER, va.vbo);
   // glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_STATIC_DRAW);
   glBufferData(GL_ARRAY_BUFFER, total_size, NULL, GL_DYNAMIC_DRAW);
   
   glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_size, mesh->vertices);
   glBufferSubData(GL_ARRAY_BUFFER, vertex_size, normal_size, mesh->normals);
   glBufferSubData(GL_ARRAY_BUFFER, vertex_size + normal_size, uv_size, mesh->uvs);


   // Setup vertex attributes. Always stride = 0 because is tightly packed. Attributes are in separate blocks, this is correct.
   glEnableVertexAttribArray(0); // Position
   glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void *)(uintptr_t)0);

   glEnableVertexAttribArray(1); // Normal
   glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void *)(uintptr_t)vertex_size);

   glEnableVertexAttribArray(2); // UV
   glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, (void *)(uintptr_t)(vertex_size + normal_size));

   // Upload index buffer
   glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va.ibo);
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh->indices_count * sizeof(u32), mesh->indices, GL_STATIC_DRAW);

   // Store counts
   va.vertex_count = mesh->vertices_count;
   va.index_count = mesh->indices_count;

   // Unbind VAO, buffer and attributes to avoid accidental changes
   glEnableVertexAttribArray(0);
   glBindBuffer(GL_ARRAY_BUFFER, 0);
   glBindVertexArray(0);

   return va;
}

Index_Buffer create_index_buffer(const GLuint* indices, usz index_count) {
    Index_Buffer vi;
    glGenBuffers(1, &vi.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vi.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(GLuint), indices, GL_STATIC_DRAW);
    vi.count = (GLuint)index_count;
    return vi;
}



Texture create_texture_extended(int width, int height, void *data, Texture_Format format, bool generate_mipmap) {
   Texture result = {0};
   result.width = width;
   result.height = height;
   result.format = format;

   glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);

   GLenum internal_format, gl_format;
   GLenum type = GL_UNSIGNED_BYTE;

   GLenum compare_mode = 0;
   GLenum compare_func = 0;
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
         type            = GL_FLOAT;
         break;
      }
      case TEXTURE_FORMAT_DEPTH24: {
         internal_format = GL_DEPTH_COMPONENT24;
         gl_format       = GL_DEPTH_COMPONENT;
         type            = GL_UNSIGNED_INT;
         break;
      }
      case TEXTURE_FORMAT_SHADOW: {
         internal_format = GL_DEPTH_COMPONENT24;
         gl_format       = GL_DEPTH_COMPONENT;
         type            = GL_UNSIGNED_INT;
         compare_mode    = GL_COMPARE_REF_TO_TEXTURE;
         compare_func    = GL_LEQUAL;
         break;
      }
      default: {
         assert_msg(false, "Unsupported texture format\n");
         return result;
      }
   }

   glTextureStorage2D(result.handle, 1, internal_format, width, height);

   if (data && (format != TEXTURE_FORMAT_DEPTH24 && format != TEXTURE_FORMAT_SHADOW)) {
      glTextureSubImage2D(result.handle, 0, 0, 0, width, height, gl_format, type, data);
   }

   if (compare_mode) {
      glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_MODE, compare_mode);
      glTextureParameteri(result.handle, GL_TEXTURE_COMPARE_FUNC, compare_func);
   }

   glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE);
   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE);

   if (generate_mipmap) {
      glGenerateTextureMipmap(result.handle);
   }

   return result;
}


inline Texture create_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, false);
}

inline Texture create_depth_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, false);
}

inline Texture create_shadow_texture(int width, int height) {
    return create_texture_extended(width, height, NULL, TEXTURE_FORMAT_SHADOW, false);
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

    Texture result = create_texture_extended(width, height, data, format, true);
    stbi_image_free(data);
    return result;
}

//-------- Framebuffer ---------

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture) {
   assert(framebuffer && is_valid_framebuffer(*framebuffer));

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

Framebuffer create_framebuffer_extended(Texture color, Texture depth) {
    Framebuffer fb = {0};

    glCreateFramebuffers(1, &fb.handle);

    if (is_valid_texture(color) && color.format != TEXTURE_FORMAT_DEPTH24) {
        if (!attach_texture_to_framebuffer(&fb, color)) {
            glDeleteFramebuffers(1, &fb.handle);
            return (Framebuffer){0};
        }
    }

    // Add more depth compatible formats in this if needed
    if (is_valid_texture(depth) && depth.format == TEXTURE_FORMAT_DEPTH24) {
        if (!attach_texture_to_framebuffer(&fb, depth)) {
            glDeleteFramebuffers(1, &fb.handle);
            return (Framebuffer){0};
        }
    }

    GLenum status = glCheckNamedFramebufferStatus(fb.handle, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer not complete: 0x%X\n", status);
        glDeleteFramebuffers(1, &fb.handle);
        return (Framebuffer){0};
    }

    return fb;
}

Framebuffer create_framebuffer(int width, int height) {
    Texture color = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_RGBA32F, false);
    Texture depth = create_texture_extended(width, height, NULL, TEXTURE_FORMAT_DEPTH24, false);
    return create_framebuffer_extended(color, depth);
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



inline void destroy_texture(Texture texture) {
   glDeleteTextures(1, &texture.handle);
}

inline void blit_framebuffer_to_swapchain_src_and_dst_non_dsa(
   const Framebuffer framebuffer,
   int src_x0, int src_y0, int src_x1, int src_y1,
   int dst_x0, int dst_y0, int dst_x1, int dst_y1,
   GLbitfield mask,
   GLenum filter
) {
   glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer.handle);
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // default framebuffer (screen)

   glBlitFramebuffer(
       src_x0, src_y0, src_x1, src_y1,   // source rectangle
       dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
       mask,                             // e.g. GL_COLOR_BUFFER_BIT
       filter                            // e.g. GL_NEAREST or GL_LINEAR
   );
}

inline void blit_framebuffer_to_swapchain_src_and_dst(
    const Framebuffer framebuffer,
    int src_x0, int src_y0, int src_x1, int src_y1,
    int dst_x0, int dst_y0, int dst_x1, int dst_y1,
    GLbitfield mask,
    GLenum filter
) {
    glBlitNamedFramebuffer(
       framebuffer.handle,               // src framebuffer
       0,                                // dst framebuffer (swapchain in this case)
       src_x0, src_y0, src_x1, src_y1,   // source rectangle
       dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
       mask,                             // e.g. GL_COLOR_BUFFER_BIT
       filter                            // e.g. GL_NEAREST or GL_LINEAR
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
    const Framebuffer  framebuffer, const Rectanglei32 dst
) {
   int dst_x0 = (GLint)dst.x, dst_y0 = (GLint)dst.y, dst_x1 = (GLint)(dst.x + dst.width), dst_y1 = (GLint)(dst.y + dst.height);
   blit_framebuffer_to_swapchain_src_and_dst(
      framebuffer,
      0, 0, framebuffer.color.width, framebuffer.color.height, // source rect
      dst_x0, dst_y0, dst_x1, dst_y1,   // destination rectangle
      GL_COLOR_BUFFER_BIT, GL_NEAREST
   );
}




void draw(const Vertex_Array va, const Shader shader, const Texture texture) {
    glUseProgram(shader.handle);
    glBindVertexArray(va.handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, va.ibo);

    if (false) {
       // Bind Texture(s)
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, texture.handle);
       glUniform1i(glGetUniformLocation(shader.handle, "uTexture"), 0);
    }

    // MVP would be set here too
    // glUniformMatrix4fv(..., glm::value_ptr(mvp));

    glDrawElements(GL_TRIANGLES, va.index_count, GL_UNSIGNED_INT, NULL);

    glBindVertexArray(0);
    glUseProgram(0);
}

void draw_with_index_buffer(const Vertex_Array va, const Index_Buffer vi, const Shader shader, const Texture texture) {
    glUseProgram(shader.handle);
    glBindVertexArray(va.handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vi.ibo);

    if (false) {
       // Bind Texture(s)
       glActiveTexture(GL_TEXTURE0);
       glBindTexture(GL_TEXTURE_2D, texture.handle);
       glUniform1i(glGetUniformLocation(shader.handle, "uTexture"), 0);
    }

    // MVP would be set here too
    // glUniformMatrix4fv(..., glm::value_ptr(mvp));

    glDrawElements(GL_TRIANGLES, vi.count, GL_UNSIGNED_INT, NULL);

    glBindVertexArray(0);
    glUseProgram(0);
}

static const char* human_readable_size(i64 bytes) {
    static char output[32];
    static const char *units[] = {"B", "KB", "MB", "GB"};
    f64 size = (f64)bytes;
    int unit_index = 0;

    while (size >= 1024.0 && unit_index < 3) {
        size /= 1024.0;
        unit_index++;
    }

    snprintf(output, size_of(output), "%.2f %s", size, units[unit_index]);
    return output;
}


void print_opengl_resource_limits(void) {
    GLint value;

    printf("\n=== OpenGL Resource Limits ===\n\n");

    // TEXTURES
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
    printf("Max texture image units per fragment shader: %d\n", value);

    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
    printf("Max combined texture image units (all shader stages): %d\n", value);

    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &value);
    printf("Max texture units in vertex shader: %d\n", value);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    printf("Max 2D texture size: %dx%d\n", value, value);

    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value);
    printf("Max 3D texture size: %dx%dx%d\n", value, value, value);

    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &value);
    printf("Max cube map size: %dx%d\n", value, value);

    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &value);
    printf("Max array texture layers: %d\n", value);

    // UNIFORMS
    glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
    printf("Max vertex shader uniforms (floats): %d\n", value);

    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &value);
    printf("Max fragment shader uniforms (floats): %d\n", value);

    glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &value);
    printf("Max combined uniform blocks across all stages: %d\n", value);

    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
    printf("Max uniform buffer binding points: %d\n", value);

    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
    printf("Max size of a single UBO: %s\n", human_readable_size(value));

    // SHADER STORAGE BUFFERS (SSBOs)
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &value);
    printf("Max SSBO binding points: %d\n", value);

    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
    printf("Max SSBO block size: %s\n", human_readable_size(value));

    // ATTRIBUTES & VARYINGS
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
    printf("Max vertex attributes (vec3 pos, vec3 normal, etc): %d\n", value);

    glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &value);
    printf("Max varying components between vertex & fragment shaders: %d\n", value);

    // FRAMEBUFFERS
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
    printf("Max framebuffer color attachments: %d\n", value);

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &value);
    printf("Max draw buffers (MRT): %d\n", value);

    // IMAGE UNITS
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &value);
    printf("Max image units for shaders: %d\n", value);

    // COMPUTE SHADER
    glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value);
    printf("Max compute work group invocations: %d\n", value);

    GLint wg_size[3];
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &wg_size[0]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &wg_size[1]);
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &wg_size[2]);
    printf("Max compute work group sizes: [%d, %d, %d]\n", wg_size[0], wg_size[1], wg_size[2]);

    // TRANSFORM FEEDBACK
    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &value);
    printf("Max transform feedback separate attribs: %d\n", value);

    glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, &value);
    printf("Max transform feedback components: %d\n", value);

    // RECOMMENDED DRAW COUNTS
    glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
    printf("Max recommended glDrawElements vertices: %d\n", value);

    glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &value);
    printf("Max recommended glDrawElements indices: %d\n", value);

    // VIEWPORT
    GLint dims[2];
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
    printf("Max viewport dimensions: %d x %d\n", dims[0], dims[1]);

    printf("\n=================================\n\n");
}




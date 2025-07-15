#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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
   glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * size_of(GLuint), indices, GL_STATIC_DRAW);


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
    glEnableVertexArrayAttrib (va.handle, 0);
    glVertexArrayAttribFormat (va.handle, 0, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, position));
    glVertexArrayAttribBinding(va.handle, 0, 0);

    glEnableVertexArrayAttrib (va.handle, 1);
    glVertexArrayAttribFormat (va.handle, 1, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, normal));
    glVertexArrayAttribBinding(va.handle, 1, 0);

    glEnableVertexArrayAttrib (va.handle, 2);
    glVertexArrayAttribFormat (va.handle, 2, 2, GL_FLOAT, GL_FALSE, offset_of(Vertex, uv));
    glVertexArrayAttribBinding(va.handle, 2, 0);

    // Index buffer
    glCreateBuffers(1, &va.ibo);
    glNamedBufferStorage(va.ibo, index_count * size_of(GLuint), indices, 0);
    glVertexArrayElementBuffer(va.handle, va.ibo);

    va.vertex_count = (GLuint)vertex_count;
    va.index_count = (GLuint)index_count;
    return va;
}


Vertex_Array create_vertex_array_from_mesh(const Mesh *mesh) {
   Vertex_Array va = {0};

   assert(mesh->vertices_count == mesh->normals_count && mesh->vertices_count == mesh->uvs_count);

   // Calculate sizes
   usz vertex_size = mesh->vertices_count * size_of(Vector3);
   usz normal_size = mesh->normals_count * size_of(Vector3);
   usz uv_size     = mesh->uvs_count * size_of(Vector2);
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
   glVertexArrayVertexBuffer(va.handle, 0, va.vbo, 0, size_of(Vector3));
   glVertexArrayAttribFormat(va.handle, 0, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 0, 0);

   // Normals (offset binding)
   glEnableVertexArrayAttrib(va.handle, 1);
   glVertexArrayVertexBuffer(va.handle, 1, va.vbo, vertex_size, size_of(Vector3));
   glVertexArrayAttribFormat(va.handle, 1, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 1, 1);

   // UVs
   glEnableVertexArrayAttrib(va.handle, 2);
   glVertexArrayVertexBuffer(va.handle, 2, va.vbo, vertex_size + normal_size, size_of(Vector2));
   glVertexArrayAttribFormat(va.handle, 2, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 2, 2);

   // Create and upload index buffer
   glCreateBuffers(1, &va.ibo);
   glNamedBufferStorage(va.ibo, mesh->indices_count * size_of(u32), mesh->indices, 0);
   glVertexArrayElementBuffer(va.handle, va.ibo);

   // Store counts
   va.vertex_count = mesh->vertices_count;
   va.index_count = mesh->indices_count;

   return va;
}

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

Framebuffer create_framebuffer_from_texture(const Texture texture) {
   Framebuffer result;

   glCreateFramebuffers(1, &result.handle);

   if (!attach_texture_to_framebuffer(&result, texture)) {
      glDeleteFramebuffers(1, &result.handle);
      return (Framebuffer){0};
   }

   return result;
}

i32 current_framebuffer_handle(void) {
   GLint fb_handle;
   glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb_handle);
   return fb_handle;
}

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

Framebuffer resolve_multisample_framebuffer(const Framebuffer* msaa_fb) {
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
            glDeleteFramebuffers(1, &static_resolve_fb.handle);
            glDeleteTextures(1, &static_resolve_fb.color.handle);
            glDeleteTextures(1, &static_resolve_fb.depth.handle);
            static_resolve_fb = (Framebuffer){0};
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




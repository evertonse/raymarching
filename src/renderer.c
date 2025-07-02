#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
   GLuint handle;
   int32_t width;
   int32_t height;
} Texture;

typedef struct {
   GLuint handle;
   Texture color_attachment;
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
    GLuint vao;
    GLuint vbo;
    GLuint count;
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


#include "assets/all_obj.h"

static Mesh bamboo_mesh = {
   .vertices       = (Vector3*)bamboo_objVerts,
   .normals        = (Vector3*)bamboo_objNormals,
   .uvs            = (Vector2*)bamboo_objTexCoords,
   .indices        = (u32*)bamboo_objIndexes,

   .vertices_count = bamboo_objVertsCount,
   .uvs_count      = bamboo_objTexCoordsCount,
   .normals_count  = bamboo_objNormalsCount,
   .indices_count  = bamboo_objIndexesCount
};

Texture create_texture(int width, int height);
Texture load_texture(const char *filepath);

Framebuffer create_framebuffer_with_texture(const Texture texture);

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture);
void blit_framebuffer_to_swapchain(const Framebuffer framebuffer);

Vertex_Array create_vertex_array(const Vertex* vertices, usz vertex_count) {
    Vertex_Array va;
    glGenVertexArrays(1, &va.vao);
    glBindVertexArray(va.vao);

    glGenBuffers(1, &va.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, va.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * size_of(Vertex), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0); // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, position));

    glEnableVertexAttribArray(1); // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, normal));

    glEnableVertexAttribArray(2); // uv
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, size_of(Vertex), (void*)offset_of(Vertex, uv));

    glBindVertexArray(0);

    va.count = (GLuint)vertex_count;
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

Texture create_texture(int width, int height) {
   Texture result;
   result.width = width;
   result.height = height;

   glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);

   glTextureStorage2D(result.handle, 1, GL_RGBA32F, width, height);

   glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   return result;
}

Texture load_texture(const char *filepath) {
   int width, height, channels;
   unsigned char *data = stbi_load(filepath, &width, &height, &channels, 0);

   if (!data) {
      fprintf(stderr, "Faile to load texture: %s\n", filepath);
      return (Texture){0};
   }

   GLenum format = channels == 4 ? GL_RGBA : channels == 3 ? GL_RGB : channels == 1 ? GL_RED : 0;

   Texture result;
   result.width = width;
   result.height = height;

   glCreateTextures(GL_TEXTURE_2D, 1, &result.handle);

   glTextureStorage2D(result.handle, 1, (format == GL_RGBA ? GL_RGBA8 : GL_RGB8), width, height);

   glTextureSubImage2D(result.handle, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, data);

   glTextureParameteri(result.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTextureParameteri(result.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_S, GL_REPEAT);
   glTextureParameteri(result.handle, GL_TEXTURE_WRAP_T, GL_REPEAT);

   glGenerateTextureMipmap(result.handle);
   stbi_image_free(data);

   return result;
}

Framebuffer create_framebuffer_with_texture(const Texture texture) {
   Framebuffer result;

   glCreateFramebuffers(1, &result.handle);

   if (!attach_texture_to_framebuffer(&result, texture)) {
      glDeleteFramebuffers(1, &result.handle);
      return (Framebuffer){0};
   }

   return result;
}

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture) {
   glNamedFramebufferTexture(framebuffer->handle, GL_COLOR_ATTACHMENT0, texture.handle, 0);

   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "Framebuffer is not complete!");
      return false;
   }

   framebuffer->color_attachment = texture;
   return true;
}

void blit_framebuffer_to_swapchain(const Framebuffer framebuffer) {
   glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer.handle);
   glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // swapchain

   glBlitFramebuffer(
      0, 0, framebuffer.color_attachment.width, framebuffer.color_attachment.height, // source rect
      0, 0, framebuffer.color_attachment.width, framebuffer.color_attachment.height, // destination rect
      GL_COLOR_BUFFER_BIT, GL_NEAREST
   );
}

void blit_framebuffer_to_swapchain_rect(
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



void draw(const Vertex_Array va, const Index_Buffer vi, const Shader shader, const Texture texture) {
    glUseProgram(shader.handle);
    glBindVertexArray(va.vao);
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


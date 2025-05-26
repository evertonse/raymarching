#include <stdbool.h>
#include <stdio.h>

typedef struct {
   GLuint handle;
   int32_t width;
   int32_t height;
} Texture;

typedef struct {
   GLuint handle;
   Texture color_attachment;
} Framebuffer;

Texture create_texture(int width, int height);
Texture load_texture(const char *filepath);

Framebuffer create_framebuffer_with_texture(const Texture texture);

bool attach_texture_to_framebuffer(Framebuffer *framebuffer, const Texture texture);
void blit_framebuffer_to_swapchain(const Framebuffer framebuffer);

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

   glBlitFramebuffer(0, 0, framebuffer.color_attachment.width,
                     framebuffer.color_attachment.height, // source rect
                     0, 0, framebuffer.color_attachment.width,
                     framebuffer.color_attachment.height, // destination rect
                     GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

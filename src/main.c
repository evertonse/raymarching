#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

// #define RAYMATH_IMPLEMENTATION
// #include "raymath.h"

#define CYE_IMPLEMENTATION
#undef assert
#undef unreachable
#include "cye.h"

#include "stb_c_lexer.c"
#include "renderer.c"
#include "shader.c"

static uint32_t compute_shader = -1;
// static const char *compute_shader_path = "src/compute.glsl";
// static const char *compute_shader_path = "src/shaders/Tunnel-Cylinders.glsl";
static const char *compute_shader_path = "./src/shaders/shadertoy/base.glsl";

static void error_callback(int error, const char *description) { fprintf(stderr, "Error (%d): %s", error, description); }

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

   if (key == GLFW_KEY_R)
      compute_shader = reload_compute_shader(compute_shader, compute_shader_path);
}

int main() {
   glfwSetErrorCallback(error_callback);

   if (!glfwInit())
      exit(EXIT_FAILURE);

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

   int width = 1280;
   int height = 720;

   GLFWwindow *window = glfwCreateWindow(width, height, "ShaderToy", NULL, NULL);
   if (!window) {
      glfwTerminate();
      exit(EXIT_FAILURE);
   }

   glfwSetKeyCallback(window, key_callback);

   glfwMakeContextCurrent(window);
   gladLoadGL(glfwGetProcAddress);
   glfwSwapInterval(1);

   compute_shader = create_compute_shader(compute_shader_path);
   // compute_shader = create_compute_shader_toy(compute_shadertoy_path);
   if (compute_shader == INVALID_SHADER_HANDLE) {
      fprintf(stderr, "Compute shader failed. Fix it and press 'R' to reload.\n");
      // return -1;
   }

   Texture compute_shader_texture = create_texture(width, height);
   Framebuffer fb = create_framebuffer_with_texture(compute_shader_texture);
   double start_time = glfwGetTime();
   while (!glfwWindowShouldClose(window)) {


      glfwGetFramebufferSize(window, &width, &height);
      double current_time = glfwGetTime();
      float elapsed_time = (float)(current_time - start_time);

      // Pass elapsed time as uniform
      // Resize texture
      if (width != compute_shader_texture.width || height != compute_shader_texture.height) {
         glDeleteTextures(1, &compute_shader_texture.handle);
         compute_shader_texture = create_texture(width, height);
         attach_texture_to_framebuffer(&fb, compute_shader_texture);
      }

      // Compute GO!

      if (compute_shader != INVALID_SHADER_HANDLE)
      {
         // uniform float     iFrameRate;            // shader frame rate
         glUseProgram(compute_shader);
         GLint time_loc = glGetUniformLocation(compute_shader, "iTime");
         glUniform1f(time_loc, elapsed_time);

         GLint resolution_loc = glGetUniformLocation(compute_shader, "iResolution");
         glUniform3f(resolution_loc, (float)width, (float)height, width / (float)height);

         glBindImageTexture(0, fb.color_attachment.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

         const GLuint workGroupSizeX = 16;
         const GLuint workGroupSizeY = 16;

         GLuint numGroupsX = (width + workGroupSizeX - 1) / workGroupSizeX;
         GLuint numGroupsY = (height + workGroupSizeY - 1) / workGroupSizeY;

         glDispatchCompute(numGroupsX, numGroupsY, 1);

         // Ensure all writes to the image are complete
         glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      }

      // Blit
      {
         blit_framebuffer_to_swapchain(fb);
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
   }

   glfwDestroyWindow(window);

   glfwTerminate();
}

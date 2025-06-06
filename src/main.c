#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "raymath.h"

#define CYE_IMPLEMENTATION
#undef assert
#undef unreachable
#include "cye.h"

#include "stb_c_lexer.c"
#include "renderer.c"
#include "shader.c"

static u32 compute_shader = -1;
// static const char *compute_shader_path = "src/compute.glsl";
// static const char *compute_shader_path = "src/shaders/Tunnel-Cylinders.glsl";
static const char *compute_shader_path = "./src/shaders/shadertoy/base.glsl";

// TODO: Make the error be tracable throught aligning current line number with the shader file
// If is from glad
static void error_callback(int error, const char *description) {
   fprintf(stderr, "Error (%d): %s", error, description);
}

typedef struct {
   struct {
      const char *base;
      const char *sticky;
      const char *reload;
      const char *fps;
      const char *zero;
   };

   u8 mem[512];
} Window_Title;

static Window_Title title = {
   .base =  "ShaderToy",
   // These Should be empty string, not null
   .sticky = "",
   .reload = "",
   .fps = "",
   .zero = 0 // Mark the end
};

typedef struct {
   Vector3 position;
   Vector2 rotation;
} Camera;

Camera move_camera(GLFWwindow *window, Camera cam) {
   Vector3 v;
   v.x = 0;
   v.y = 0;
   v.z = 0;

   const f32 c = 0.00000005;
   if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      v.z += c;
   }

   if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      v.z -= c;
   }

   if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      v.x -= c;
   }


   if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      v.x += c;
   }

   v = Vector3Normalize(v);

   cam.position = Vector3Add(cam.position, v);
   // cam.rotation;
   return cam;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      // glfwSetWindowShouldClose(window, GLFW_TRUE);
   }

   if (key == GLFW_KEY_R && action == GLFW_PRESS) {
      u32 old = compute_shader;
      compute_shader = reload_compute_shader(compute_shader, compute_shader_path);
      if (old == compute_shader) {
         title.reload = "(reload failed)";
      } else {
         title.reload = "";
      }
   }

   if (key == GLFW_KEY_C && action == GLFW_RELEASE) {
      bool sticky = glfwGetWindowAttrib(window, GLFW_FLOATING);
      glfwSetWindowAttrib(window, GLFW_FLOATING, !sticky);

   }
}

static bool mouse_right_pressed = false;
static bool mouse_left_pressed = false;
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
   if (button == GLFW_MOUSE_BUTTON_LEFT) {
      if (action == GLFW_PRESS) {
         mouse_left_pressed = true;
      } else if (action == GLFW_RELEASE) {
         mouse_left_pressed = false;
      }
   }

   if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      if (action == GLFW_PRESS) {
         mouse_right_pressed = true;
      } else if (action == GLFW_RELEASE) {
         mouse_right_pressed = false;
      }
   }
}

static void conditionally_change_windows_title(GLFWwindow *window, f64 dt) {
   static char fps[512];
   if (dt > 0) {
      int written = snprintf(fps, (sizeof fps / sizeof fps[0]),"%.2f", 1./dt);
      title.fps = fps;
   }
   bool sticky = glfwGetWindowAttrib(window, GLFW_FLOATING);
   if (sticky) {
      title.sticky = "*sticky";
   } else {
      title.sticky = "";
   }

   const char **curr = &title.base;
   usz count = 0;
   isz max = (sizeof title.mem / sizeof title.mem[0]);
   const char* fmt = "%s ";
   while (*curr) {
      // If last don't add the space
      if (NULL == *(curr + 1)) {
         fmt = "%s";
      }
      count += snprintf((char*)title.mem + count, max - count, fmt, *curr);
      curr++;
   }
   const char *new_title = (const char*)title.mem;

   const char* current_title =  glfwGetWindowTitle(window);
   bool please_update =  0 == strcmp(new_title,  current_title);
   if (!please_update) {
      glfwSetWindowTitle(window, new_title);
   }
}


int main() {
   glfwSetErrorCallback(error_callback);

   if (!glfwInit())
      exit(EXIT_FAILURE);

   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

   GLFWmonitor *monitor = glfwGetPrimaryMonitor();
   const GLFWvidmode *mode = glfwGetVideoMode(monitor);

   // Get the maximum resolution
   const int max_width = mode->width;
   const int max_height = mode->height;

   int window_width  = max_width / 3.5;                // Half the width of the screen
   int window_height = max_height / 1.6;               // Half the height of the screen

   int right_padding_from_windows_bar = 67;
   int window_x = max_width - window_width - right_padding_from_windows_bar;  // 3/4 from the left
   int window_y = (max_height - window_height) / 2;                      // Centered vertically

   // int width = 1280;
   // int height = 720;

   GLFWwindow *window = glfwCreateWindow(window_width, window_height, title.base, NULL, NULL);
   if (!window) {
      glfwTerminate();
      exit(EXIT_FAILURE);
   }

   glfwSetWindowAttrib(window, GLFW_FLOATING, true);
   glfwSetWindowPos(window, window_x, window_y);

   glfwSetKeyCallback(window, key_callback);
   glfwSetMouseButtonCallback(window, mouse_button_callback);


   glfwMakeContextCurrent(window);
   gladLoadGL(glfwGetProcAddress);
   glfwSwapInterval(1);

   compute_shader = create_compute_shader(compute_shader_path);
   if (INVALID_SHADER_HANDLE == compute_shader) {
      fprintf(stderr, "Compute shader failed. Fix it and press 'R' to reload.\n");
   }

   Texture compute_shader_texture = create_texture(window_width, window_height);
   Framebuffer fb = create_framebuffer_with_texture(compute_shader_texture);
   f64 start_time = glfwGetTime();

   f64 conditionally_change_windows_title_timer_default = 0.75;
   f64 conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;

   f64 shader_needs_reload_timer_default = 1.1; // Seconds
   f64 shader_needs_reload_timer = shader_needs_reload_timer_default;

   f64 previous_time = glfwGetTime();
   Camera camera_default  = (Camera){
      cliteral(Vector3){.x=0, .y=3.0, .z=-5.},
      cliteral(Vector2){.x=0, .y=3.0 }
   };

   Camera camera = camera_default;

   while (!glfwWindowShouldClose(window)) {
      bool window_minized = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
      f64 current_time = glfwGetTime();
      f64 delta_time = current_time - previous_time; // Time since last frame

      f64 elapsed_time = (f32)(current_time - start_time); // Total time since start

      // TODO: Timed operations struct instead
      conditionally_change_windows_title_timer -= delta_time;
      shader_needs_reload_timer -= delta_time;

      if (conditionally_change_windows_title_timer <= 0) {
         conditionally_change_windows_title(window, delta_time);
         conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;
      }

      if (shader_needs_reload_timer <= 0) {
         if (shader_needs_reload(compute_shader)) {
            u32 old = compute_shader;
            compute_shader = reload_compute_shader(compute_shader, compute_shader_path);
            if (old == compute_shader) {
               title.reload = "(reload failed)";
            } else {
               title.reload = "";
            }
         }
         shader_needs_reload_timer = shader_needs_reload_timer_default;
      }

      glfwGetFramebufferSize(window, &window_width, &window_height);

      // Resize texture only if need and is not minimized
      if ((window_width != compute_shader_texture.width
         || window_height != compute_shader_texture.height)
         && !window_minized)
      {
         glDeleteTextures(1, &compute_shader_texture.handle);
         compute_shader_texture = create_texture(window_width, window_height);
         attach_texture_to_framebuffer(&fb, compute_shader_texture);
      }

      // Compute GO!
      camera = move_camera(window, camera);

      if (INVALID_SHADER_HANDLE != compute_shader)
      {
         glUseProgram(compute_shader);

         {  // Time uniform
            GLint loc = glGetUniformLocation(compute_shader, "iTime");
            glUniform1f(loc, (f32)elapsed_time);
         }


         {  // Resolution uniform
            GLint loc = glGetUniformLocation(compute_shader, "iResolution");
            glUniform3f(
               loc,
               (f32)compute_shader_texture.width, (f32)compute_shader_texture.height,
               compute_shader_texture.width/(f32)compute_shader_texture.height
            );
         }

         {  // Position uniform
            GLint loc = glGetUniformLocation(compute_shader, "iPosition");
            glUniform3f(
               loc, camera.position.x, camera.position.y, camera.position.z
            );
         }

         {  // Position uniform
            GLint loc = glGetUniformLocation(compute_shader, "iRotation");
            glUniform2f(
               loc, camera.rotation.x, camera.rotation.y
            );
         }


         {  // Mouse uniform
            GLint mouse_loc = glGetUniformLocation(compute_shader, "iMouse");
            f64 mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            glUniform4f(
               mouse_loc,
               (f32)mouse_x, (f32)mouse_y,
               mouse_left_pressed ? 1.f : 0.0f,
               mouse_right_pressed ? 1.f : 0.0f
            );
         }

         glBindImageTexture(0, fb.color_attachment.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

         const GLuint work_group_size_x = 16;
         const GLuint work_group_size_y = 16;

         GLuint num_groups_x = (compute_shader_texture.width + work_group_size_x - 1) / work_group_size_x;
         GLuint num_groups_y = (compute_shader_texture.height + work_group_size_y - 1) / work_group_size_y;

         glDispatchCompute(num_groups_x, num_groups_y, 1);

         // Ensure all writes to the image are complete
         glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      }

      // Only blit if windows is not minimized
      if (!window_minized) {
          blit_framebuffer_to_swapchain(fb);
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
      previous_time = current_time;
   }

   glfwDestroyWindow(window);

   glfwTerminate();
}

#include "raymath.h"
#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"



#define CYE_IMPLEMENTATION
#undef assert
#undef unreachable
#include "cye.h"

#include "renderer/renderer.c"
#include "assets/all_obj.h"


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
static bool mouse_right_pressed = false;
static bool mouse_left_pressed = false;

// Macro to define a mesh from OBJ data
#define DEFINE_MESH(prefix, ext)                          \
   static Mesh prefix##_mesh = {                          \
       .vertices       = (Vector3*)prefix##_objVerts,     \
       .normals        = (Vector3*)prefix##_objNormals,   \
       .uvs            = (Vector2*)prefix##_objTexCoords, \
       .indices        = (u32*)prefix##_objIndexes,       \
                                                          \
       .vertices_count = prefix##_objVertsCount,          \
       .uvs_count      = prefix##_objTexCoordsCount,      \
       .normals_count  = prefix##_objNormalsCount,        \
       .indices_count  = prefix##_objIndexesCount         \
   };                                                     \
   static char *prefix##_texture_path = "res/textures/" #prefix ext


DEFINE_MESH(bamboo, ".jpg");
DEFINE_MESH(enemy, ".png");
DEFINE_MESH(tiger, "_yellow.png");
DEFINE_MESH(horse, ".png");

static Mesh cube_mesh = {
   .vertices       = (Vector3*)cube_objVerts,
   .normals        = (Vector3*)cube_objNormals,
   .uvs            = (Vector2*)cube_objTexCoords,
   .indices        = (u32*)cube_objIndexes,

   .vertices_count = cube_objVertsCount,
   .uvs_count      = cube_objTexCoordsCount,
   .normals_count  = cube_objNormalsCount,
   .indices_count  = cube_objIndexesCount
};

// Pull out camera to camera file (gameplay folder?)
typedef struct {
   Vector3 position;
   Vector3 rotation; // .x value is radians rotation around x-axis
   f32 zoom;
} Camera;

static Camera camera = {0};


// Values here are read only and are always up to date
typedef struct {
   void (*init)  (void* self);
   void (*update)(void* self, f64 dt);

   struct {
      GLFWwindow *handle; // Why abstract ? I'm not gonna add anything to it besides "making it ours" bleh
      int  width, height;
      bool is_minimized;
   } window;

   struct {
      f64 current, delta, previous, elapsed;
   } time;

   Camera camera;
} Application;

#define create_application(init_fn, update_fn)     \
   {                                  \
      .init   = (void (*)(void*))      (init_fn),  \
      .update = (void (*)(void*, f64)) (update_fn) \
   }



// #define chosen_mesh bamboo_mesh
// #define chosen_texture_path bamboo_texture_path

// #define chosen_mesh horse_mesh
// #define chosen_texture_path horse_texture_path

#define chosen_mesh tiger_mesh
#define chosen_texture_path tiger_texture_path

// #define chosen_mesh enemy_mesh
// #define chosen_texture_path enemy_texture_path



#include "./projection-application.c"
#include "./raymarch-application.c"

// TODO: Make the error be tracable throught aligning current line number with the shader file
// If is from glad
static void error_callback(int error, const char *description) {
   fprintf(stderr, "Error (%d): %s", error, description);
}


static f64 glfwGetScroll(GLFWwindow *window);


Camera move_camera(GLFWwindow *window, Camera cam) {

   ////////////////////////
   //// Rotation //////////
   ////////////////////////
   static bool mouse_button_left_down = false;
   static Vector2 mouse_last_position = { .x = -1.0f, .y = -1.0f };

   int mouse_left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
   f64 mouse_x, mouse_y;
   glfwGetCursorPos(window, &mouse_x, &mouse_y);
   const f32 sensitivity = 60.0;
   f32 sensitivity_factor = Remap(sensitivity, 0.0f, 100.0f, 0.0059f, 0.0009f);

   if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_4) == GLFW_PRESS) {
      printf("Mouse Buttom 4 pressed\n");
   }

   if (mouse_left == GLFW_PRESS) {
      if (!mouse_button_left_down) {
         // First time pressing the button, store the last position
         mouse_button_left_down = true;
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      } else {
         // Calculate the mouse movement
         f32 delta_x = (f32)(mouse_x - mouse_last_position.x);
         f32 delta_y = (f32)(mouse_y - mouse_last_position.y);

         // Update camera rotation based on mouse movement
         cam.rotation.y -= delta_x * sensitivity_factor;
         cam.rotation.x -= delta_y * sensitivity_factor;

         // Clamp the vertical rotation to prevent flipping
         if (cam.rotation.x > DEG2RAD * (89.0f))
            cam.rotation.x = DEG2RAD * (89.0f);
         if (cam.rotation.x < DEG2RAD * (-89.0f))
            cam.rotation.x = DEG2RAD * (-89.0f);

         // Update the last mouse position
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      }
   } else if (mouse_left == GLFW_RELEASE) {
      mouse_button_left_down = false;
   }

   ////////////////////////
   //// Position //////////
   ////////////////////////

   Vector3 v;
   v.x = 0;
   v.y = 0;
   v.z = 0;

   Vector3 forward = Vector3RotateByAxisAngle((Vector3){0., 0., 1.}, (Vector3){0., 1., 0.}, -cam.rotation.y);
   Vector3 right   = Vector3CrossProduct(forward, (Vector3){0., 1., 0.});
   forward = Vector3Normalize(forward);
   right   = Vector3Normalize(right);


   const f32 c = 0.14;
   if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      v = Vector3Add(v, forward);
   }

   if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      v = Vector3Subtract(v, forward);
   }

   if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      v = Vector3Add(v, right);
   }

   if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      v = Vector3Subtract(v, right);
   }

   v = Vector3Normalize(v);
   v = Vector3Scale(v, c);

   cam.position = Vector3Add(cam.position, v);

   f64 yoffset = glfwGetScroll(window);
   if (yoffset != 0.0) {
      cam.zoom = 1+yoffset;
   }

   if (cam.zoom < 1) {
      cam.zoom = 1;
      yoffset = 0;
   }


   return cam;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      // glfwSetWindowShouldClose(window, GLFW_TRUE);
   }

#if 0
   if (key == GLFW_KEY_R && action == GLFW_PRESS) {
      Shader old = compute_shader;
      compute_shader = reload_shader(compute_shader);
      printf("reloaded and its broken ? %s\n", INVALID_SHADER_HANDLE == compute_shader.handle ? "yes" : "no");
      if (old.handle == compute_shader.handle) {
         title.reload = "(reload failed)";
      } else {
         title.reload = "";
      }
   }
#endif

   if (key == GLFW_KEY_C && action == GLFW_RELEASE) {
      bool sticky = glfwGetWindowAttrib(window, GLFW_FLOATING);
      glfwSetWindowAttrib(window, GLFW_FLOATING, !sticky);

   }
}

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

static f64 scroll_offset = 0.0;
static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
   scroll_offset += yoffset;
   // printf("xoffset=%f yoffset=%f\n", xoffset, yoffset);
}

static f64 glfwGetScroll(GLFWwindow *window) {
   return scroll_offset;
}

int main() {
   glfwSetErrorCallback(error_callback);

   if (!glfwInit())
      exit(EXIT_FAILURE);

   {  // open gl hints
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   }

   glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

   GLFWmonitor *monitor = glfwGetPrimaryMonitor();
   const GLFWvidmode *mode = glfwGetVideoMode(monitor);

   // Get the maximum resolution
   const int max_width  = mode->width;
   const int max_height = mode->height;
   trace_info("Monitor Width x Height = %d x %d", max_width, max_height);

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

   glfwSetWindowAttrib(window, GLFW_FLOATING, false); // sticky
   glfwSetWindowPos(window, window_x, window_y);
   // GLFW_CURSOR_HIDDEN GLFW_CURSOR_NORMAL GLFW_CURSOR_DISABLED(fps style) GLFW_CURSOR_CAPTURED(Won't be able to leave window) GLFW_CURSOR_DISABLED
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);



   glfwSetKeyCallback(window, key_callback);
   glfwSetScrollCallback(window, scroll_callback);
   glfwSetMouseButtonCallback(window, mouse_button_callback);


   glfwMakeContextCurrent(window);
   gladLoadGL(glfwGetProcAddress);
   glfwSwapInterval(1);

   int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
   if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
      enable_error_report();
   }

   print_opengl_resource_limits();

   { // Some expected settings
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_MULTISAMPLE);
      glDisable(GL_CULL_FACE);
      // glCullFace(GL_BACK);          // Cull back faces
      glFrontFace(GL_CCW);             // GL_CCW to define front faces as counter-clockwise
   }
   ////////////////////////////////////////////////////////////////////////////////////////////



   f64 start_time = glfwGetTime();

   f64 conditionally_change_windows_title_timer_default = 0.15;
   f64 conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;

   f64 previous_time = glfwGetTime();

   Camera camera_default = (Camera) {
       .position = cliteral(Vector3){.x = 0, .y = 3.0, .z = -10.0},
       .rotation = cliteral(Vector3){.x = 0, .y = 0.0, .z =  0.0 },
       .zoom = 1.0f
   };

   camera = camera_default;


   Application* apps[] = {(Application*)&raymarching_application, (Application*)&projection_application};
   // Application* apps[] = {(Application*)&projection_application};
   // Application* apps[] = {(Application*)&raymarching_application};

   bool window_minimized =  false;

   for (isz idx = 0; idx < count_of(apps); idx++) {
      auto app = apps[idx];
      trace_info("Monitor Width x Height = %d x %d", max_width, max_height);
      app->window.handle       = window;
      app->window.width        = window_width;
      app->window.height       = window_height;
      app->window.is_minimized = window_minimized;
      app->init(app);
   }

   while (!glfwWindowShouldClose(window)) {
      window_minimized = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
      f64 current_time = glfwGetTime();
      f64 delta_time = current_time - previous_time; // Time since last frame

      f64 elapsed_time = (f32)(current_time - start_time); // Total time since start


      // app->time.elapsed  = elapsed_time;
      // app->time.current  = current_time;
      // app->time.delta    = delta_time;
      // app->time.previous = previous_time;


      // TODO: Timed operations struct instead
      conditionally_change_windows_title_timer -= delta_time;

      if (conditionally_change_windows_title_timer <= 0) {
         conditionally_change_windows_title(window, delta_time);
         conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;
      }



      glfwGetFramebufferSize(window, &window_width, &window_height);

      // Camera GO!
      camera = move_camera(window, camera);


      for (isz idx = 0; idx < count_of(apps); idx++) {
         auto app = apps[idx];

         app->camera = camera;
         app->time = (typeof(app->time)){
            .elapsed  = elapsed_time,
            .current  = current_time,
            .delta    = delta_time,
            .previous = previous_time
         };
         app->window.width        = window_width;
         app->window.height       = window_height;
         app->window.is_minimized = window_minimized;

         app->update(app, delta_time);
      }

      // Only blit if windows is not minimized
      if (!window_minimized) {
         // app->update(app, delta_time);
         // projection_update((Projection_Application*)app, delta_time);
         // render_mesh_to_framebuffer(&chosen_mesh);
         // blend_framebuffers();
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
      previous_time = current_time;
   }

   glfwDestroyWindow(window);

   glfwTerminate();
}

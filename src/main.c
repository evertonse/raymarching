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

#include "./window.c"

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

   f64 yoffset = get_mouse_scroll();
   if (yoffset != 0.0) {
      cam.zoom = 1+yoffset;
   }

   if (cam.zoom < 1) {
      cam.zoom = 1;
      yoffset = 0;
   }


   return cam;
}

static void conditionally_change_windows_title(f64 dt) {
   static char fps[512];
   if (dt > 0) {
      int written = snprintf(fps, (sizeof fps / sizeof fps[0]),"%.2f", 1./dt);
      title.fps = fps;
   }
   bool sticky = is_window_sticky();
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

   const char* current_title =  current_window_title();
   bool please_update =  0 == strcmp(new_title,  current_title);
   if (!please_update) {
      change_window_title(new_title);
   }
}


int main() {

   create_window();
   init_renderer();

   f64 start_time = time_now();

   f64 conditionally_change_windows_title_timer_default = 0.15;
   f64 conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;

   f64 previous_time = time_now();

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

   int window_width  = get_window_width();
   int window_height = get_window_height();

   for (isz idx = 0; idx < count_of(apps); idx++) {
      auto app = apps[idx];
      app->window.handle       = __window.handle;
      app->window.width        = window_width;
      app->window.height       = window_height;
      app->window.is_minimized = window_minimized;
      app->init(app);
   }

   while (!should_close_window()) {
      update_window();
      window_minimized = is_window_minimized();
      f64 current_time = time_now();
      f64 delta_time = current_time - previous_time; // Time since last frame

      f64 elapsed_time = (f32)(current_time - start_time); // Total time since start


      // app->time.elapsed  = elapsed_time;
      // app->time.current  = current_time;
      // app->time.delta    = delta_time;
      // app->time.previous = previous_time;


      // TODO: Timed operations struct instead
      conditionally_change_windows_title_timer -= delta_time;

      if (conditionally_change_windows_title_timer <= 0) {
         conditionally_change_windows_title(delta_time);
         conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;
      }



      assert(nullptr != __window.handle);
      glfwGetFramebufferSize(__window.handle, &window_width, &window_height);

      // Camera GO!
      camera = move_camera(__window.handle, camera);


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

      glfwSwapBuffers(__window.handle);
      glfwPollEvents();
      previous_time = current_time;
   }

   destroy_window();
}

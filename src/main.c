#include <stdio.h>

#include "./raymath.c"

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"



#define CYE_IMPLEMENTATION
#undef assert
#undef unreachable
#include "cye.h"

#include "./state.c"
#include "./timing.c"

#include "renderer/renderer.c"

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

#include "./window.c"
#include "./camera.c"
#include "./gui.c"

static Camera camera = {0};

// Values here are read only and are always up to date
typedef struct {
   void (*init)  (void* self);
   void (*update)(void* self, f64 dt);

   struct {
      GLFWwindow *handle; // Why abstract ? I'm not gonna add anything to it besides "making it ours" bleh
      int  width, height;
   } window;

   struct {
      f64 current, delta, elapsed;
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
   init_window();
   init_renderer();
   init_time();

   f64 start_time = time_now();


   Countdown window_title_countdown = create_countdown(0.15, true);

   Camera camera_default = {
       .position = cliteral(Vector3){.x = 0, .y = 3.0, .z = -10.0},
       .rotation = cliteral(Vector3){.x = 0, .y = 0.0, .z =  0.0 },
       .zoom = 1.0f
   };

   camera = camera_default;


   Application* apps[] = {(Application*)&raymarching_application, (Application*)&projection_application};
   // Application* apps[] = {(Application*)&projection_application};
   // Application* apps[] = {(Application*)&raymarching_application};

   int window_width  = get_window_width();
   int window_height = get_window_height();

   for (isz idx = 0; idx < count_of(apps); idx++) {
      auto app = apps[idx];
      app->window.handle       = __state.window.handle;
      app->window.width        = window_width;
      app->window.height       = window_height;
      app->init(app);
   }

   while (!should_close_window()) {
      update_window();
      update_time();
      camera = move_camera(camera); // Update Camera
      // TODO: Timed operations struct instead

      update_countdown(&window_title_countdown, conditionally_change_windows_title(time_delta()));

      assert(nullptr != __state.window.handle);
      window_width = get_window_width(), window_height = get_window_height();
      for (isz idx = 0; idx < count_of(apps); idx++) {
         auto app = apps[idx];

         app->camera = camera;
         app->time = (typeof(app->time)){
            .elapsed  = time_elapsed(),
            .current  = time_now(),
            .delta    = time_delta(),
         };
         app->window.width  = window_width;
         app->window.height = window_height;
         app->update(app, time_delta());
      }

      // swap_window_buffers();
      // pool_window_events();
   }

   shutdown_window();
   shutdown_renderer();
}

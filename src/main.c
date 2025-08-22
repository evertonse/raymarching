// NOTE: 3 spaces indentation is the best and I'm tired of pretending it's not. I'm sorry if it hurts your powers of 2 brain.

#include <stdio.h>
#include <stdlib.h>


#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"

#define overload __attribute__((overloadable))
#define require  __must_check
#define stack_alloc __builtin_alloca

#define trace_struct(d)     __builtin_dump_struct(&d, &printf)
#define type_as_string(d)   __builtin_type_as_string(&d, &tprintf)

#define private __attribute__((visibility("hidden")))



// NOTE: If not defined, nuklear will try to define itself BUT is crashes when freeing a null which is wrong since stb relys on that behaviour it seems.
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))


#undef unreachable
#undef normalize
#include "./math.c"

#undef assert
#define CYE_IMPLEMENTATION
#include "cye.h"
#undef trace_debug
#define trace_debug(fmt, ...) cye_trace_log(CYE_LOG_DEBUG, "`%s`: " fmt, __func__, ##__VA_ARGS__)


const char* cye_human_readable_size(i64 bytes) {
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

#define human_readable_size cye_human_readable_size


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

constexpr Camera camera_default = {
    .position = {.x = 100., .y = 4.0, .z = -10.0},
    .rotation = {.x = 0, .y = 0.0, .z =  0.0 },
    .zoom = 1.0f
};

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
   {                                               \
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
   init_gui();
   init_manager();

   f64 start_time = time_now();


   Countdown window_title_countdown = create_countdown(0.15, true);


   camera = camera_default;


   Application *apps[] = {(Application*)&raymarching_application, (Application*)&projection_application};
   // Application* apps[] = {(Application*)&projection_application};
   // Application* apps[] = {(Application*)&raymarching_application};

   int window_width  = get_window_width();
   int window_height = get_window_height();

   for (isz idx = 0; idx < count_of(apps); idx++) {
      Application *app = apps[idx];
      app->window.handle       = __state.window.handle;
      app->window.width        = window_width;
      app->window.height       = window_height;
      app->init(app);
   }

   while (!should_close_window()) {
      update_window();
      // DONE: Timed operations struct instead
      update_time();
      camera = move_camera(camera); // Update Camera
      update_gui();

      Framebuffer default_framebuffer = {
         .handle = 0,
         .color = {
            .width  = get_window_width(),
            .height = get_window_height()
         },
         .depth = {
            .width  = get_window_width(),
            .height = get_window_height()
         },
      };
      screen_width  = 1600;
      screen_height = 800;

      update_countdown(&window_title_countdown, conditionally_change_windows_title(time_delta()));

      assert(nullptr != __state.window.handle);
      window_width = get_window_width(), window_height = get_window_height();
      for (isz idx = 0; idx < count_of(apps); idx++) {
         Application* app = apps[idx];

         app->camera = camera;
         app->time = (typeof(app->time)){
            .elapsed  = time_elapsed(),
            .current  = time_now(),
            .delta    = time_delta(),
         };
         app->window.width  = window_width;
         app->window.height = window_height;
         /* setup global state */
         glEnable(GL_BLEND);
         glBlendEquation(GL_FUNC_ADD);
         glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
         glDisable(GL_CULL_FACE);
         glEnable(GL_DEPTH_TEST);
         glEnable(GL_SCISSOR_TEST);
         // glActiveTexture(GL_TEXTURE0);
         app->update(app, time_delta());
      }


      render_gui(default_framebuffer);

      // swap_window_buffers();
      // pool_window_events();
   }

   shutdown_gui();
   shutdown_window();
   shutdown_renderer();
}

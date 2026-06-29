// NOTE: 3 spaces indentation is the best and I'm tired of pretending it's not. I'm sorry if it hurts your powers of 2 brain.

#include <stdio.h>
#include <stdlib.h>


#define GLFW_INCLUDE_NONE
#include "glfw/glfw/include/GLFW/glfw3.h"


#define GLAD_GL_IMPLEMENTATION

#if defined(RENDERER_USING_BINDLESS)
#  include "glad/gl_extended.h"
#else
// #  include "glad/gl.h"
#  include "glad/gl_extended.h"
#endif

#define overload    __attribute__((overloadable))
#define require     __must_check
#define stack_alloc __builtin_alloca

// Defer options for C: https://antonz.org/defer-in-c/#gccclang
// more on defer: https://thephd.dev/c2y-the-defer-technical-specification-its-time-go-go-go
// https://thephd.dev/_vendor/future_cxx/technical%20specification/C%20-%20defer/C%20-%20defer%20Technical%20Specification.pdf
#define defer _Defer

#define trace_struct(d)     __builtin_dump_struct(&d, &printf)
#define type_as_string(d)   __builtin_type_as_string(&d, &tprintf)

#define private __attribute__((visibility("hidden")))
#define type_of typeof
#define zero_of(x) ((typeof(x)) {0})
#define interpret_as(Type, v) ((union{ typeof(v) s; Type d; }){.s = v}.d)

void wait_for_enter_on_terminal(void) {
   int c;
   puts("Press Enter to continue...");
   // Read until newline is consumed
   while ((c = getchar()) != '\n' && c != EOF) { }
}

// NOTE: If not defined, nuklear will try to define itself BUT is crashes when freeing a null which is wrong since stb relies on that behaviour it seems.
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))



#define STBDS_NO_SHORT_NAMES
#define STB_DS_IMPLEMENTATION
#include "stb/stb_ds.h"
// Hashmap Operations (for typed keys)
#define table_free(map)                     stbds_hmfree((map))
#define table_length(map)                   stbds_hmlen(map)
#define table_length_unsigned(map)          stbds_hmlenu(map)
#define table_index_of(map, key)            stbds_hmgeti(map, key)
#define table_index_of_ts(map, key, temp)   stbds_hmgeti_ts(map, key, temp)
#define table_get(map, key)                 stbds_hmget(map, key)
#define table_get_temporary(map, key, temp) stbds_hmget_ts(map, key, temp)
#define table_get_struct(map, key)          stbds_hmgets(map, key)
#define table_get_ptr(map, key)             stbds_hmgetp(map, key)
#define table_get_ptr_temporary(map, key, temp)    stbds_hmgetp_ts(map, key, temp)
#define table_get_ptr_or_null(map, key)     stbds_hmgetp_null(map, key)
#define table_set_default_value(map, val)   stbds_hmdefault(map, val)
#define table_set_default_struct(map, item) stbds_hmdefaults(map, item)
#define table_put(map, key, value)          stbds_hmput(map, key, value)
#define table_puts(map, item)               stbds_hmputs(map, item)
#define table_delete(map, key)              stbds_hmdel(map, key)

// String Hashmap Operations (for string keys)
#define string_table_free(map)                     stbds_shfree(map)
#define string_table_length(map)                   stbds_shlen(map)
#define string_table_length_unsigned(map)          stbds_shlenu(map)
#define string_table_index_of(map, key)            stbds_shgeti(map, key)
#define string_table_get(map, key)                 stbds_shget(map, key)
#define string_table_get_struct(map, key)          stbds_shgets(map, key)
#define string_table_get_ptr(map, key)             stbds_shgetp(map, key)
#define string_table_get_ptr_or_null(map, key)     stbds_shgetp_null(map, key)
#define string_table_set_default_value(map, val)   stbds_shdefault(map, val)
#define string_table_set_default_struct(map, item) stbds_shdefaults(map, item)
#define string_table_put(map, key, value)          stbds_shput(map, key, value)
#define string_table_puti(map, key, value)         stbds_shputi(map, key, value)
#define string_table_puts(map, item)               stbds_shputs(map, item)
#define string_table_new_arena(arena)              stbds_sh_new_arena(arena)
#define string_table_new_strdup(arena)             stbds_sh_new_strdup(arena)
#define string_table_delete(map, key)              stbds_shdel(map, key)




// Defines shared between shader and cpu
// Must be before math.c so gpu also does the calculations in same space as cpu.
#include "./renderer/shared/defines.glsl"


#undef assert
#define CYE_IMPLEMENTATION
#include "cye.h"
#undef trace_debug
#define trace_debug(fmt, ...) cye_trace_log(CYE_LOG_DEBUG, "`%s`: " fmt, __func__, ##__VA_ARGS__)
#undef da_append

#undef unreachable
#undef normalize
#include "./math.c"

// Append an item to a dynamic array using thread_local Cye_Context cye_context
#define da_append(da, ...)                                                                                       \
   do {                                                                                                          \
      if ((da)->count >= (da)->capacity) {                                                                       \
         (da)->capacity = (da)->capacity == 0 ? CYE_DARRAY_INIT_CAP : (da)->capacity * CYE_DARRAY_CAP_MULTIPLIER;\
                                                                                                                 \
         (da)->items = cye_context.realloc((da)->items, (da)->capacity * sizeof(((da)->items)[0]));              \
         cye_assert((da)->items != NULL && "Dynamic Array: OOM");                                                \
      }                                                                                                          \
                                                                                                                 \
      (da)->items[(da)->count++] = (typeof((da)->items[0]))__VA_ARGS__;                                          \
   } while (0)

#define trace_warnf(fmt, ...)  trace_warn( "%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define trace_infof(fmt, ...)  trace_info( "%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define trace_errorf(fmt, ...) trace_error("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define trace_fatalf(fmt, ...) trace_fatal("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#if defined(PLATFORM_WINDOWS)

bool is_debugging() {
   return IsDebuggerPresent();
}

#elif defined(PLATFORM_LINUX)

bool is_debugging() {
   FILE *file = fopen("/proc/self/status", "r");
   if (file) {
      char line[256];
      while (fgets(line, sizeof(line), file)) {
         if (strncmp(line, "TracerPid:", 10) == 0) {
            int tracer_pid = atoi(line + 10);
            if (tracer_pid != 0) {
               return true;
            }
            break;
         }
      }
      fclose(file);
   } else {
      trace_warn("could not open /proc/self/status.\n");
   }
   return false;
}
#endif

const char* cye_human_readable_size(i64 bytes) {
    static char output[32];
    static const char *units[] = {"B", "KB", "MB", "GB"};
    f64 size = (f64)bytes;
    int unit_index = 0;

    while (size >= 1024.0 && unit_index < 3) {
        size /= 1024.0;
        unit_index += 1;
    }

    snprintf(output, size_of(output), "%.2f %s", size, units[unit_index]);
    return output;
}

#define human_readable_size cye_human_readable_size


#include "./state.c"
#include "./timing.c"
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
#include "./renderer/renderer.c"
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



#include "./run/projection-application.c"
#include "./run/raymarch-application.c"



static void conditionally_change_windows_title(f64 dt) {
   static char fps[512];
   static bool which_fps = false;
   if (is_button_pressed(BUTTON_Y)) {
      which_fps = !which_fps;
   }
   if (dt > 0) {
      int written = snprintf(fps, count_of(fps),"%.2f", 1./dt);
      if (which_fps) {
         title.fps = fps;
      } else {
         auto checkpoint = tsave(); // just in case is uses temporary memory
         title.fps = get_fps_string();
         trestore(checkpoint);
      }
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
      curr += 1;
   }
   const char *new_title = (const char*)title.mem;

   const char* current_title =  current_window_title();
   bool please_update = 0 == strcmp(new_title,  current_title);
   if (!please_update) {
      change_window_title(new_title);
   }
}



int main() {
   init_window();
   init_time();
   init_fps();
   init_renderer();
   init_manager();
   init_managed_shaders();

   static const bool fixed_gui = false;
   if (fixed_gui) {
      init_gui();
   }


   Countdown window_title_countdown = create_countdown(0.15, true);


   camera = camera_default;


   // Application *apps[] = {(Application*)&raymarching_application, (Application*)&projection_application};
   Application *apps[] = {(Application*)&projection_application};
   // Application *apps[] = {(Application*)&raymarching_application};

   for (isz idx = 0; idx < count_of(apps); idx += 1) {
      Application *app = apps[idx];
      app->init(app);
   }
   if (is_debugging()) {
      minimize_window();
   }

   while (!should_close_window()) {
      update_window();
      update_renderer();
      update_time();
      update_fps();
      update_managed_shaders();

      if (fixed_gui) {
         update_gui();
      }

      camera = move_camera(camera); // Update Camera
      screen_width  = 1600;
      screen_height = 800;

      update_countdown(&window_title_countdown, conditionally_change_windows_title(time_delta()));


      assert(nullptr != __state.window.handle);
      for (isz idx = 0; idx < count_of(apps); idx += 1) {
         Application* app = apps[idx];


         app->camera = camera;
         app->update(app, time_delta());
      }


      if (fixed_gui) {
         render_gui(default_framebuffer);
      }
   }

   if (fixed_gui) {
      shutdown_gui();
   }
   shutdown_window();
   shutdown_renderer();
}

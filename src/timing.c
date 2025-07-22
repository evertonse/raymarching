#include <stdio.h>
#include <time.h>

// Optional: For Windows you might want to use QueryPerformanceCounter
// This example uses cross-platform clock_gettime (POSIX)
#ifdef _WIN32
#include <windows.h>
static LARGE_INTEGER freq;
static BOOL initialized = FALSE;

static f64 time_sec() {
   if (!initialized) {
      QueryPerformanceFrequency(&freq);
      initialized = TRUE;
   }
   LARGE_INTEGER t;
   QueryPerformanceCounter(&t);
   return (f64)t.QuadPart / (f64)freq.QuadPart;
}

#else

#include <time.h>
static f64 time_sec() {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}
#endif

inline f64 time_now() {
   return glfwGetTime();
}

 // Total time since start
inline f64 time_elapsed() {
   return (time_now() - __state.time.start);
}

inline f64 time_delta() {
   return __state.time.delta;
}


inline void init_time() {
   __state.time.start    = time_now();
   __state.time.previous = __state.time.start;
}

inline void update_time(void) {
   f64 current_time = time_now();
   __state.time.delta = current_time - __state.time.previous; // Time since last frame
   __state.time.previous = current_time;
}

// CPU-side FPS counter
typedef struct {
   f64 last_time;
   f64 frame_accum;
   int frame_count;
   f64 fps;
} FPS_Timer;

void fps_timer_init(FPS_Timer *timer) {
   timer->last_time = time_sec();
   timer->frame_accum = 0.0;
   timer->frame_count = 0;
   timer->fps = 0.0;
}

void fps_timer_update(FPS_Timer *timer) {
   f64 current_time = time_sec();
   f64 delta = current_time - timer->last_time;
   timer->last_time = current_time;
   timer->frame_accum += delta;
   timer->frame_count++;

   if (timer->frame_accum >= 1.0) {
      timer->fps = timer->frame_count / timer->frame_accum;
      timer->frame_accum = 0.0;
      timer->frame_count = 0;
      printf("FPS: %.2f\n", timer->fps);
   }
}

typedef enum {
   Type_Invalid,
   Type_Countdown
} Type;

typedef struct {
   u32 magic;                // Unique identifier for type safety
   const f64 seconds;        // Time to wait
   f64  seconds_left;        // Remaining time
   f64  previous_time;       // Time of last update
   bool repeat;              // Should it reset after triggering?
   u32 repeat_count;         // How many times it triggered
} Countdown;


Countdown create_countdown(f64 seconds, bool repeat) {
    return (Countdown){
        .magic         = Type_Countdown,
        .seconds       = seconds,
        .seconds_left  = seconds,
        .previous_time = time_now(),
        .repeat        = repeat,
        .repeat_count  = 0
    };
}

#define assert_countdown_ptr(ptr) \
    static_assert(_Generic((ptr), Countdown*: 1, default: 0), "Expected Countdown*")


#define update_countdown(c, code_block) do {                                  \
   assert_countdown_ptr(c);                                                   \
   if ((c)->magic != Type_Countdown) {                                       \
      trace_error("Invalid Countdown object at %s:%d\n", __FILE__, __LINE__); \
   }                                                                          \
   if ((c)->seconds_left <= 0.0) {                                            \
      code_block;                                                             \
      (c)->repeat_count++;                                                    \
      (c)->seconds_left = (c)->repeat ? (c)->seconds : 0.0;                   \
   } else {                                                                   \
      f64 _now = time_now();                                                  \
      f64 _delta = _now - (c)->previous_time;                                 \
      (c)->seconds_left -= _delta;                                            \
      (c)->previous_time = _now;                                              \
   }                                                                          \
} while(0)


typedef struct {
    GLuint gpu_query;
    GLuint64 gpu_time_ns;
    struct timespec cpu_start;
    bool gpu_valid, gpu_active;
} Profile_Timer;

constexpr static isz profile_timer_array_count = 200; // 200 functions deep at max
static isz profile_timer_idx = 0;
static Profile_Timer profile_timer_array[profile_timer_array_count] = {0};

void profile_begin_old(void) {
   assert(profile_timer_idx < profile_timer_array_count);
   assert(__state.window.initialized && __state.renderer.initialized);
   Profile_Timer *profiler = &profile_timer_array[profile_timer_idx++];

   // Make sure GPU timer isn't already in use
   assert_msg(profiler->gpu_query == 0, "GPU query already in use at stack index %lld", profile_timer_idx - 1);

   timespec_get(&profiler->cpu_start, TIME_UTC);

   glGenQueries(1, &profiler->gpu_query);
   glBeginQuery(GL_TIME_ELAPSED, profiler->gpu_query);

   GLenum err = glGetError();
   assert_msg(err == GL_NO_ERROR, "glBeginQuery failed with error 0x%X", err);
}

void profile_end_old(const char *label) {
   assert(profile_timer_idx > 0);
   assert(__state.window.initialized && __state.renderer.initialized);
   Profile_Timer *profiler = &profile_timer_array[--profile_timer_idx];

   glEndQuery(GL_TIME_ELAPSED);

   GLenum err = glGetError();
   assert_msg(err == GL_NO_ERROR, "glEndQuery failed with error 0x%X", err);

   // Optional: block until result is available
   GLint available = 0;
   for (isz max_tries = 100; !available && max_tries > 0; --max_tries) {
      glGetQueryObjectiv(profiler->gpu_query, GL_QUERY_RESULT_AVAILABLE, &available);
   }

   glGetQueryObjectui64v(profiler->gpu_query, GL_QUERY_RESULT, &profiler->gpu_time_ns);
   glDeleteQueries(1, &profiler->gpu_query);

   struct timespec end;
   timespec_get(&end, TIME_UTC);

   time_t sec = end.tv_sec - profiler->cpu_start.tv_sec;
   long nsec = end.tv_nsec - profiler->cpu_start.tv_nsec;

   double cpu_ms = (double)(sec * 1000.0) + (nsec / 1e6);
   double gpu_ms = profiler->gpu_time_ns / 1e6;

   trace_info("[PROFILE] %s: CPU: %.3f ms | GPU: %.3f ms\n", label, cpu_ms, gpu_ms);

   *profiler = (Profile_Timer){0}; // Clear after use
}

void profile_begin(void) {
   assert_msg(profile_timer_idx < profile_timer_array_count, "profile_timer_idx = %d profile_timer_array_count=%d", profile_timer_idx, profile_timer_array_count);
   Profile_Timer *profiler = &profile_timer_array[profile_timer_idx];
   profile_timer_idx += 1;
   *profiler = (Profile_Timer){0}; // Clear everything initially

   timespec_get(&profiler->cpu_start, TIME_UTC);

   if (profiler->gpu_active) {
      // Try creating and starting a GPU query
      glGenQueries(1, &profiler->gpu_query);
      glBeginQuery(GL_TIME_ELAPSED, profiler->gpu_query);

      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_warn("GL ERROR: glBeginQuery failed with 0x%X, GPU profiling disabled for this block", err);
         profiler->gpu_valid = false;
         glDeleteQueries(1, &profiler->gpu_query);
         profiler->gpu_query = 0;
      } else {
         profiler->gpu_valid = true;
      }
   }
}

void profile_end(const char *label) {
   assert(profile_timer_idx > 0);
   profile_timer_idx -= 1;
   Profile_Timer *profiler = &profile_timer_array[profile_timer_idx];

   struct timespec end;
   timespec_get(&end, TIME_UTC);

   time_t sec = end.tv_sec - profiler->cpu_start.tv_sec;
   auto nsec = end.tv_nsec - profiler->cpu_start.tv_nsec;
   double cpu_ms = (double)(sec * 1000.0) + (nsec / 1e6);

   double gpu_ms = -1.0;

   if (profiler->gpu_valid) {
      glEndQuery(GL_TIME_ELAPSED);
      GLenum err = glGetError();

      if (err != GL_NO_ERROR) {
         trace_warn("GL ERROR: glEndQuery failed with 0x%X", err);
      } else {
         GLint available = 0;
         for (isz max_tries = 100; !available && max_tries > 0; --max_tries) {
            glGetQueryObjectiv(profiler->gpu_query, GL_QUERY_RESULT_AVAILABLE, &available);
         }

         if (!available) {
            trace_warn("GPU profiling: query result not available in time");
         } else {
            glGetQueryObjectui64v(profiler->gpu_query, GL_QUERY_RESULT, &profiler->gpu_time_ns);
            GLenum err2 = glGetError();
            if (err2 == GL_NO_ERROR) {
               gpu_ms = profiler->gpu_time_ns / 1e6;
            } else {
               trace_warn("GL ERROR: glGetQueryObjectui64v failed with 0x%X", err2);
            }
         }
      }

      glDeleteQueries(1, &profiler->gpu_query);
   }

   if (gpu_ms < 0.0) {
      trace_info("[PROFILE] %s: CPU: %.3f ms | GPU: N/A (error)", label, cpu_ms);
   } else {
      trace_info("[PROFILE] %s: CPU: %.3f ms | GPU: %.3f ms", label, cpu_ms, gpu_ms);
   }

   *profiler = (Profile_Timer){0};
}

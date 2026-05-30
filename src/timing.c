#include <stdio.h>
#include <time.h>

// For Windows we might wanna to use QueryPerformanceCounter
// For now we're just using hte glfw implementation rsrs

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
f64 inline time_elapsed() {
   return (time_now() - __state.time.start);
}

// The same delta for the whole frame
f64 inline time_delta() {
   return __state.time.delta;
}

// Instructions within the same frame might see different deltas.
f64 inline time_delta_recomputed() {
   f64 current_time = time_now();
   f64 delta = current_time - __state.time.previous;
   return delta;
}


void inline init_time() {
   __state.time.start    = time_now();
   __state.time.previous = __state.time.start;
}


void inline update_time(void) {
   f64 current_time = time_now();
   __state.time.delta = current_time - __state.time.previous; // Time since last frame
   __state.time.previous = current_time;
}


void init_fps() {
   memset(&__state.fps, 0, size_of(Fps));
   __state.fps.last_frame_time = time_now();
   __state.fps.min_fps = 999999.0;
   __state.fps.max_fps = 0.0;
}


void update_fps() {
   f64 current_time = time_now();
   f64 frame_time = current_time - __state.fps.last_frame_time;

   if (frame_time > 0.0) {
      f64 current_fps = 1.0 / frame_time;

      // Update min/max
      if (current_fps < __state.fps.min_fps)
         __state.fps.min_fps = current_fps;
      if (current_fps > __state.fps.max_fps)
         __state.fps.max_fps = current_fps;
      __state.fps.total_frames += 1;


      // Add to circular buffer
      __state.fps.frame_times[__state.fps.current_index] = frame_time;
      __state.fps.current_index = (__state.fps.current_index + 1) % FPS_MAX_SAMPLES;

      if (__state.fps.sample_count < FPS_MAX_SAMPLES) {
         __state.fps.sample_count += 1;
      }

      // Calculate average
      f64 total_time = 0.0;
      for (int i = 0; i < __state.fps.sample_count; i += 1) {
         total_time += __state.fps.frame_times[i];
      }

      __state.fps.avg_fps = __state.fps.sample_count / total_time;
   }

   __state.fps.last_frame_time = current_time;
}


char* get_fps_string() {
   static char str[512];

   f64 variance = 0.0;
   if (__state.fps.sample_count > 1) {
      f64 avg_frame_time = 1.0 / __state.fps.avg_fps;
      for (int i = 0; i < __state.fps.sample_count; i += 1) {
         f64 diff = __state.fps.frame_times[i] - avg_frame_time;
         variance += diff * diff;
      }
      variance /= __state.fps.sample_count;
   }

   f64 std_dev = sqrt(variance);
   f64 consistency = __state.fps.avg_fps > 0 ? (1.0 - std_dev / (1.0 / __state.fps.avg_fps)) * 100.0 : 0.0;

   if (consistency < 0.0) {
      consistency = 0.0;
   }
   if (consistency > 100.0) {
      consistency = 100.0;
   }

   snprintf(str, 256, "fps: %.1f | frametime: %.7fms | min: %.1f | max: %.1f | frames: %d | consistency: %.1f%%",
      __state.fps.avg_fps, __state.fps.frame_times[__state.fps.sample_count-1], __state.fps.min_fps == 999999.0 ? 0.0 : __state.fps.min_fps, __state.fps.max_fps, __state.fps.total_frames, consistency
   );

   return str;
}

typedef enum {
   Type_Invalid,
   Type_Countdown
} Type;

typedef struct {
   u32 magic;                // Unique identifier for type safety (experimenting with that. Overengineering type shit)
   f64 seconds;              // Time to wait
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
      (c)->repeat_count += 1;                                                 \
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
    bool gpu_valid;
} Profile_Timer;


// This count determine the max functions deep where each function uses the being_profile and no end counterpart in sight.
constexpr static isz profile_timer_array_count = 200;
static isz profile_timer_idx = 0;
static Profile_Timer profile_timer_array[profile_timer_array_count] = {0};


void begin_profile(void) {
   assert_msg(profile_timer_idx < profile_timer_array_count, "profile_timer_idx = %d profile_timer_array_count=%d", profile_timer_idx, profile_timer_array_count);

   Profile_Timer *profiler = &profile_timer_array[profile_timer_idx++];
   *profiler = (Profile_Timer){0}; // Clear everything

   timespec_get(&profiler->cpu_start, TIME_UTC);

   // Always try to create GPU query
   glGenQueries(1, &profiler->gpu_query);
   glBeginQuery(GL_TIME_ELAPSED, profiler->gpu_query);

   GLenum err = glGetError();
   profiler->gpu_valid = (err == GL_NO_ERROR);
   if (!profiler->gpu_valid) {
      trace_warn("GL ERROR: glBeginQuery failed with 0x%X, GPU profiling disabled", err);
      glDeleteQueries(1, &profiler->gpu_query);
      profiler->gpu_query = 0;
   }
}


void end_profile(const char *label) {
   assert(profile_timer_idx > 0);
   Profile_Timer *profiler = &profile_timer_array[--profile_timer_idx];

   struct timespec end;
   timespec_get(&end, TIME_UTC);

   // Calculate CPU time
   time_t sec = end.tv_sec - profiler->cpu_start.tv_sec;
   long nsec = end.tv_nsec - profiler->cpu_start.tv_nsec;
   double cpu_ms = (sec * 1000.0) + (nsec / 1e6);
   double gpu_ms = -1.0;

   if (profiler->gpu_valid) {
      glEndQuery(GL_TIME_ELAPSED);

      // Wait for query result (with timeout)
      GLint available = 0;
      isz max_tries = 10000000000000;
      const bool finish = false;

      if (finish) {
         glFinish(); // Is this necessary?
      }

      while (!available && max_tries-- > 0) {
         glGetQueryObjectiv(profiler->gpu_query, GL_QUERY_RESULT_AVAILABLE, &available);
      }

      if (available) {
         glGetQueryObjectui64v(profiler->gpu_query, GL_QUERY_RESULT, &profiler->gpu_time_ns);
         gpu_ms = profiler->gpu_time_ns / 1e6;
      } else {
         trace_warn("GPU query result not available after waiting");
      }

      glDeleteQueries(1, &profiler->gpu_query);
   }

   // Log results
   if (gpu_ms < 0) {
      trace_info("[Profile] %s: CPU: %.3f ms | GPU: N/A", label, cpu_ms);
   } else {
      trace_info("[Profile] %s: CPU: %.3f ms | GPU: %.3f ms", label, cpu_ms, gpu_ms);
   }

   *profiler = (Profile_Timer){0};
}

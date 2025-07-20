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

// Optional: GPU-side timer queries (measures GPU frame time)
typedef struct {
   GLuint query_start;
   GLuint query_end;
   GLuint64 gpu_time_ns;
   f64 gpu_time_ms;
   bool active;
} GPU_Timer;

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
        .magic = Type_Countdown,
        .seconds = seconds,
        .seconds_left = seconds,
        .previous_time = time_now(),
        .repeat = repeat,
        .repeat_count = 0
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

void gpu_timer_init(GPU_Timer *timer) {
   glGenQueries(1, &timer->query_start);
   glGenQueries(1, &timer->query_end);
   timer->gpu_time_ns = 0;
   timer->gpu_time_ms = 0.0;
   timer->active = false;
}

void gpu_timer_begin(GPU_Timer *timer) {
   if (!timer->active) {
      glQueryCounter(timer->query_start, GL_TIMESTAMP);
      timer->active = true;
   }
}

void gpu_timer_end(GPU_Timer *timer) {
   if (timer->active) {
      glQueryCounter(timer->query_end, GL_TIMESTAMP);
   }
}

void gpu_timer_update(GPU_Timer *timer) {
   if (!timer->active)
      return;

   GLint available = 0;
   glGetQueryObjectiv(timer->query_end, GL_QUERY_RESULT_AVAILABLE, &available);
   if (available) {
      GLuint64 start, end;
      glGetQueryObjectui64v(timer->query_start, GL_QUERY_RESULT, &start);
      glGetQueryObjectui64v(timer->query_end, GL_QUERY_RESULT, &end);

      timer->gpu_time_ns = end - start;
      timer->gpu_time_ms = (f64)timer->gpu_time_ns / 1e6;
      timer->active = false;

      printf("GPU Frame Time: %.3f ms\n", timer->gpu_time_ms);
   }
}

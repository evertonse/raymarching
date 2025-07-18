#include <GL/gl3w.h> // or glad or appropriate loader
#include <stdio.h>
#include <time.h>

// Optional: For Windows you might want to use QueryPerformanceCounter
// This example uses cross-platform clock_gettime (POSIX)
#ifdef _WIN32
#include <windows.h>
static LARGE_INTEGER freq;
static BOOL initialized = FALSE;

static double time_sec() {
   if (!initialized) {
      QueryPerformanceFrequency(&freq);
      initialized = TRUE;
   }
   LARGE_INTEGER t;
   QueryPerformanceCounter(&t);
   return (double)t.QuadPart / (double)freq.QuadPart;
}

#else

#include <time.h>
static double time_sec() {
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec + ts.tv_nsec / 1e9;
}
#endif

// CPU-side FPS counter
typedef struct {
   double last_time;
   double frame_accum;
   int frame_count;
   double fps;
} FPS_Timer;

void fps_timer_init(FPS_Timer *timer) {
   timer->last_time = time_sec();
   timer->frame_accum = 0.0;
   timer->frame_count = 0;
   timer->fps = 0.0;
}

void fps_timer_update(FPS_Timer *timer) {
   double current_time = time_sec();
   double delta = current_time - timer->last_time;
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
   double gpu_time_ms;
   bool active;
} GPU_Timer;

typedef struct {
   double value;
} Milliseconds;

typedef struct {
   Milliseconds default
   Milliseconds current;
} Countdown;

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
      timer->gpu_time_ms = (double)timer->gpu_time_ns / 1e6;
      timer->active = false;

      printf("GPU Frame Time: %.3f ms\n", timer->gpu_time_ms);
   }
}

// We're taking this from raylib and assuming all matches glfw. But we didnt check all

static void window_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
   auto current = &__state.button.current[BUTTON_MOUSE_BEGIN-button];
   if     (action == GLFW_RELEASE) *current = BUTTON_IS_UP;
   else if(action == GLFW_PRESS)   *current = BUTTON_IS_DOWN;
   else if(action == GLFW_REPEAT)  *current = BUTTON_IS_DOWN;
}

static void window_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
   int button = key;

   auto current = &__state.button.current[BUTTON_MOUSE_BEGIN-button];
   if     (action == GLFW_RELEASE) *current = BUTTON_IS_UP;
   else if(action == GLFW_PRESS)   *current = BUTTON_IS_DOWN;
   else if(action == GLFW_REPEAT)  *current = BUTTON_IS_DOWN;

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

}

static void window_scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
   __state.scroll_offset += yoffset;
}

f64 get_mouse_scroll() {
   return __state.scroll_offset;
}

// TODO: Make the error be tracable throught aligning current line number with the shader file
// If is from glad
static void window_error_callback(int error, const char *description) {
   fprintf(stderr, "Error (%d): %s", error, description);
}

void init_window(void) {
   if (!glfwInit()) {
      trace_error("Failure on window initialization.");
      exit(EXIT_FAILURE);
   }

   {  // open gl hints
      glfwSetErrorCallback(window_error_callback);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

      const bool back_buffer_multisample = false;
      if (back_buffer_multisample) {
         glfwWindowHint(GLFW_SAMPLES, 32);
         glfwWindowHint(GLFW_RED_BITS, 32);
         glfwWindowHint(GLFW_GREEN_BITS, 32);
         glfwWindowHint(GLFW_BLUE_BITS, 32);
         glfwWindowHint(GLFW_ALPHA_BITS, 32);
         glfwWindowHint(GLFW_DEPTH_BITS, 24);
         glfwWindowHint(GLFW_STENCIL_BITS, 8);
      }
   }

   glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

   __state.window.monitor = glfwGetPrimaryMonitor();
   __state.window.mode    = glfwGetVideoMode(__state.window.monitor);
   const int max_width  = __state.window.mode->width;
   const int max_height = __state.window.mode->height;

   int window_width  = max_width / 3.2;                // Half the width of the screen
   int window_height = max_height / 1.6;               // Half the height of the screen

   int right_padding_from_windows_bar = 67;
   int window_x = max_width - window_width - right_padding_from_windows_bar;  // 3/4 from the left
   int window_y = (max_height - window_height) / 2;                      // Centered vertically

   __state.window.handle = glfwCreateWindow(window_width, window_height, title.base, NULL, NULL);
   if (!__state.window.handle) {
      glfwTerminate();
      trace_error("Failed to even create a window, that's the saddest thing. Hope you the best.");
      exit(EXIT_FAILURE);
   }
   auto window = __state.window.handle;

   glfwSetWindowAttrib(window, GLFW_FLOATING, true); // sticky
   glfwSetWindowPos(window, window_x, window_y);
   // GLFW_CURSOR_HIDDEN GLFW_CURSOR_NORMAL GLFW_CURSOR_DISABLED(fps style) GLFW_CURSOR_CAPTURED(Won't be able to leave window) GLFW_CURSOR_DISABLED
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

   glfwSetKeyCallback(window, window_key_callback);
   glfwSetScrollCallback(window, window_scroll_callback);
   glfwSetMouseButtonCallback(window, window_mouse_button_callback);


   glfwMakeContextCurrent(window);
   gladLoadGL(glfwGetProcAddress);
   const bool cap_frame_rate = false;
   glfwSwapInterval(cap_frame_rate);
   __state.window.initialized = true;
}


inline void shutdown_window(void) {
   glfwDestroyWindow(__state.window.handle);
   glfwTerminate();
}

inline void swap_window_buffers(void) {
   glfwSwapBuffers(__state.window.handle);
}

inline void pool_window_events(void) {
   glfwPollEvents();
}

Vector2 window_size() {
   int window_width, window_height;
   glfwGetFramebufferSize(__state.window.handle, &window_width, &window_height);
   return (Vector2){(f32)window_width, (f32)window_height};
}

void* platform_window_handle() {
   return __state.window.handle;
}


int get_window_height() {
   {
      int width, height;
      glfwGetFramebufferSize(__state.window.handle, &width, &height);
      (void)width;
      return height;
   }

   {
      int width, height;
      glfwGetWindowSize(__state.window.handle, &width, &height);
      (void)width;
      return height;
   }

}

int get_window_width() {
   int width, height;
   {
      glfwGetFramebufferSize(__state.window.handle, &width, &height);
      (void)height;
      return width;
   }

   {
      int width, height;
      glfwGetWindowSize(__state.window.handle, &width, &height);
      (void)height;
      return width;
   }
}

Vector2 get_screen_resolution() {
   __state.window.mode = glfwGetVideoMode(__state.window.monitor);
   const int max_width  = __state.window.mode->width;
   const int max_height = __state.window.mode->height;
   return (Vector2){(f32)max_width, (f32)max_height};
}

bool should_close_window(void) {
   return glfwWindowShouldClose(__state.window.handle);
}

void close_window(void) {
   glfwSetWindowShouldClose(__state.window.handle, GLFW_TRUE);
}


inline bool is_button_pressed(Button input) {
   if (input >= BUTTON_MOUSE_LEFT) {
      return glfwGetMouseButton(__state.window.handle, input-BUTTON_MOUSE_LEFT) == GLFW_PRESS;
   }
   return glfwGetKey(__state.window.handle, input) == GLFW_PRESS;
}

inline bool is_button_released(Button input) {
   if (input >= BUTTON_MOUSE_LEFT) {
      return glfwGetMouseButton(__state.window.handle,  input-BUTTON_MOUSE_LEFT) == GLFW_RELEASE;
   }
   return glfwGetKey(__state.window.handle, input) == GLFW_RELEASE;
}

inline bool is_button_down(Button input) {
   return __state.button.current[input] == BUTTON_IS_DOWN;
}

inline bool is_button_up(Button input) {
   return __state.button.current[input] == BUTTON_IS_UP;
}

inline Vector2 cursor_position() {
   f64 pos_x, pos_y;
   glfwGetCursorPos(__state.window.handle, &pos_x, &pos_y);
   return (Vector2){(f32)pos_x, (f32)pos_y};
}


void maximize_window(void) {
    if (glfwGetWindowAttrib(__state.window.handle, GLFW_RESIZABLE) == GLFW_TRUE) {
        glfwMaximizeWindow(__state.window.handle);
    }
}

void minimize_window(void) {
    glfwIconifyWindow(__state.window.handle);
}

inline bool is_window_minimized() {
   return glfwGetWindowAttrib(__state.window.handle, GLFW_ICONIFIED) == GLFW_TRUE;
}

inline bool is_window_sticky() {
   return glfwGetWindowAttrib(__state.window.handle, GLFW_FLOATING);
}

inline void change_window_title(ZString new_title) {
   glfwSetWindowTitle(__state.window.handle, new_title);
}

const char* current_window_title() {
   return glfwGetWindowTitle(__state.window.handle);
}

void update_on_button(void) {
   if (is_button_pressed(BUTTON_T)) {
      bool sticky = is_window_sticky();
      glfwSetWindowAttrib(__state.window.handle, GLFW_FLOATING, !sticky);
   }
}

inline void update_window(void) {
   update_on_button();
   pool_window_events();
   swap_window_buffers();
}

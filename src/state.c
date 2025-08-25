typedef enum {
   INPUT_NONE             = 0,
   BUTTON_APOSTROPHE      = 39,       // Key: '
   BUTTON_COMMA           = 44,       // Key: ,
   BUTTON_MINUS           = 45,       // Key: -
   BUTTON_PERIOD          = 46,       // Key: .
   BUTTON_SLASH           = 47,       // Key: /
   BUTTON_0               = 48,       // Key: 0
   BUTTON_1               = 49,       // Key: 1
   BUTTON_2               = 50,       // Key: 2
   BUTTON_3               = 51,       // Key: 3
   BUTTON_4               = 52,       // Key: 4
   BUTTON_5               = 53,       // Key: 5
   BUTTON_6               = 54,       // Key: 6
   BUTTON_7               = 55,       // Key: 7
   BUTTON_8               = 56,       // Key: 8
   BUTTON_9               = 57,       // Key: 9
   BUTTON_SEMICOLON       = 59,       // Key: ;
   BUTTON_EQUAL           = 61,       // Key: =
   BUTTON_A               = 65,       // Key: A | a
   BUTTON_B               = 66,       // Key: B | b
   BUTTON_C               = 67,       // Key: C | c
   BUTTON_D               = 68,       // Key: D | d
   BUTTON_E               = 69,       // Key: E | e
   BUTTON_F               = 70,       // Key: F | f
   BUTTON_G               = 71,       // Key: G | g
   BUTTON_H               = 72,       // Key: H | h
   BUTTON_I               = 73,       // Key: I | i
   BUTTON_J               = 74,       // Key: J | j
   BUTTON_K               = 75,       // Key: K | k
   BUTTON_L               = 76,       // Key: L | l
   BUTTON_M               = 77,       // Key: M | m
   BUTTON_N               = 78,       // Key: N | n
   BUTTON_O               = 79,       // Key: O | o
   BUTTON_P               = 80,       // Key: P | p
   BUTTON_Q               = 81,       // Key: Q | q
   BUTTON_R               = 82,       // Key: R | r
   BUTTON_S               = 83,       // Key: S | s
   BUTTON_T               = 84,       // Key: T | t
   BUTTON_U               = 85,       // Key: U | u
   BUTTON_V               = 86,       // Key: V | v
   BUTTON_W               = 87,       // Key: W | w
   BUTTON_X               = 88,       // Key: X | x
   BUTTON_Y               = 89,       // Key: Y | y
   BUTTON_Z               = 90,       // Key: Z | z
   BUTTON_LEFT_BRACKET    = 91,       // Key: [
   BUTTON_BACKSLASH       = 92,       // Key: '\'
   BUTTON_RIGHT_BRACKET   = 93,       // Key: ]
   BUTTON_GRAVE           = 96,       // Key: `
   // Function keys
   BUTTON_SPACE           = 32,       // Key: Space
   BUTTON_ESCAPE          = 256,      // Key: Esc
   BUTTON_ENTER           = 257,      // Key: Enter
   BUTTON_TAB             = 258,      // Key: Tab
   BUTTON_BACKSPACE       = 259,      // Key: Backspace
   BUTTON_INSERT          = 260,      // Key: Ins
   BUTTON_DELETE          = 261,      // Key: Del
   BUTTON_RIGHT           = 262,      // Key: Cursor right
   BUTTON_LEFT            = 263,      // Key: Cursor left
   BUTTON_DOWN            = 264,      // Key: Cursor down
   BUTTON_UP              = 265,      // Key: Cursor up
   BUTTON_PAGE_UP         = 266,      // Key: Page up
   BUTTON_PAGE_DOWN       = 267,      // Key: Page down
   BUTTON_HOME            = 268,      // Key: Home
   BUTTON_END             = 269,      // Key: End
   BUTTON_CAPS_LOCK       = 280,      // Key: Caps lock
   BUTTON_SCROLL_LOCK     = 281,      // Key: Scroll down
   BUTTON_NUM_LOCK        = 282,      // Key: Num lock
   BUTTON_PRINT_SCREEN    = 283,      // Key: Print screen
   BUTTON_PAUSE           = 284,      // Key: Pause
   BUTTON_F1              = 290,      // Key: F1
   BUTTON_F2              = 291,      // Key: F2
   BUTTON_F3              = 292,      // Key: F3
   BUTTON_F4              = 293,      // Key: F4
   BUTTON_F5              = 294,      // Key: F5
   BUTTON_F6              = 295,      // Key: F6
   BUTTON_F7              = 296,      // Key: F7
   BUTTON_F8              = 297,      // Key: F8
   BUTTON_F9              = 298,      // Key: F9
   BUTTON_F10             = 299,      // Key: F10
   BUTTON_F11             = 300,      // Key: F11
   BUTTON_F12             = 301,      // Key: F12
   BUTTON_SHIFT           = 340,      // Key: Shift left
   BUTTON_LEFT_SHIFT      = 340,      // Key: Shift left

   BUTTON_CONTROL         = 341,      // Key: Control left
   BUTTON_LEFT_CONTROL    = 341,      // Key: Control left

   BUTTON_LEFT_ALT        = 342,      // Key: Alt left
   BUTTON_LEFT_SUPER      = 343,      // Key: Super left

   BUTTON_RIGHT_SHIFT     = 344,      // Key: Shift right
   BUTTON_RIGHT_CONTROL   = 345,      // Key: Control right
   BUTTON_RIGHT_ALT       = 346,      // Key: Alt right
   BUTTON_RIGHT_SUPER     = 347,      // Key: Super right
   BUTTON_KB_MENU         = 348,      // Key: KB menu
   // Keypad keys
   BUTTON_KP_0            = 320,      // Key: Keypad 0
   BUTTON_KP_1            = 321,      // Key: Keypad 1
   BUTTON_KP_2            = 322,      // Key: Keypad 2
   BUTTON_KP_3            = 323,      // Key: Keypad 3
   BUTTON_KP_4            = 324,      // Key: Keypad 4
   BUTTON_KP_5            = 325,      // Key: Keypad 5
   BUTTON_KP_6            = 326,      // Key: Keypad 6
   BUTTON_KP_7            = 327,      // Key: Keypad 7
   BUTTON_KP_8            = 328,      // Key: Keypad 8
   BUTTON_KP_9            = 329,      // Key: Keypad 9
   BUTTON_KP_DECIMAL      = 330,      // Key: Keypad .
   BUTTON_KP_DIVIDE       = 331,      // Key: Keypad /
   BUTTON_KP_MULTIPLY     = 332,      // Key: Keypad *
   BUTTON_KP_SUBTRACT     = 333,      // Key: Keypad -
   BUTTON_KP_ADD          = 334,      // Key: Keypad +
   BUTTON_KP_ENTER        = 335,      // Key: Keypad Enter
   BUTTON_KP_EQUAL        = 336,      // Key: Keypad =
   // Android key buttons
   BUTTON_BACK            = 4,        // Key: Android back button
   BUTTON_MENU            = 5,        // Key: Android menu button
   BUTTON_VOLUME_UP       = 24,       // Key: Android volume up button
   BUTTON_VOLUME_DOWN     = 25,        // Key: Android volume down button

   BUTTON_MOUSE_BEGIN   = 400,
   BUTTON_MOUSE_LEFT    = 400,       // Mouse button left
   BUTTON_MOUSE_RIGHT   = 401,       // Mouse button right
   BUTTON_MOUSE_MIDDLE  = 402,       // Mouse button middle (pressed wheel)
   BUTTON_MOUSE_SIDE    = 403,       // Mouse button side (advanced mouse device)
   BUTTON_MOUSE_EXTRA   = 404,       // Mouse button extra (advanced mouse device)
   BUTTON_MOUSE_FORWARD = 405,       // Mouse button forward (advanced mouse device)
   BUTTON_MOUSE_BACK    = 406,       // Mouse button back (advanced mouse device)

   BUTTON_MAX_VALUE
} Button;


enum {
   BUTTON_IS_UP = 0,
   BUTTON_IS_DOWN
};


#define FPS_MAX_SAMPLES 60
typedef struct {
   f64 frame_times[FPS_MAX_SAMPLES];
   int sample_count;
   int current_index;
   f64 last_frame_time;
   f64 min_fps, max_fps, avg_fps;
   int total_frames;
} Fps;


static struct {
   struct {
      GLFWwindow  *handle;
      GLFWmonitor *monitor;
      const GLFWvidmode *mode;
      bool initialized;
   } window;

   struct {
      f64 start;
      f64 previous;
      f64 delta;
   } time;

   Fps fps;

   struct {
      int previous[BUTTON_MAX_VALUE];
      int current [BUTTON_MAX_VALUE];
   } button;

   struct {
      bool initialized;
   } renderer;

   f64 scroll_offset;
} __state = {0};

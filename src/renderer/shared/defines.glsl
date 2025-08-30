#ifndef SHARED_DEFINES_HEADER
#define SHARED_DEFINES_HEADER

#define RENDERER_USING_BINDLESS 1
// #define RENDERER_DEBUG 1
#define BINDING_DRAW_COMMAND 18
#define BINDING_MATERIAL     10
#define BINDING_CAMERA       2


// I refuse to call this mix smh.
#ifndef lerp
#   define lerp mix
#endif

// Some helpful constants
#ifndef PI
#   define PI 3.14159265358979323846
#endif

#ifndef TAU
#   define TAU PI * 2.
#endif

#ifndef EPSILON
#   define EPSILON 0.000001
#endif

#ifndef DEG2RAD
#   define DEG2RAD (PI/180.0)
#endif

#ifndef RAD2DEG
#   define RAD2DEG (180.0/PI)
#endif

#endif // SHARED_DEFINES_HEADER
// defines.glsl end


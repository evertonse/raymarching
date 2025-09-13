#ifndef SHARED_DEFINES_HEADER
#define SHARED_DEFINES_HEADER

#define RENDERER_USING_BINDLESS 1

#ifdef RELEASE
// Define nothing
#else
#   define RENDERER_DEBUG  1
#endif

// TODO: fix convention for these
#define BINDING_INSTANCE_BUFFER    6
#define BINDING_INDICES_BUFFER     5
#define BINDING_VERTEX_BUFFER      3
#define BINDING_VERTEX_TANGENT     7
#define BINDING_TANGENTS_BUFFER    8
#define BINDING_PER_FRAME          4
#define BINDING_DRAW_COMMAND       18
#define BINDING_MATERIAL           10
#define BINDING_CAMERA             2
#define BINDING_ANIMATION_MATRICES 12
#define BINDING_JOINT_BUFFER       9


// I refuse to call this mix smh.
#ifndef lerp
#   define lerp mix
#endif

// Some helpful constants
#ifndef PI
#   define PI 3.14159265358979323846
#endif

// Smallest such that 1.0 + FLT_EPSILON != 1.0.
#ifndef FLT_EPSILON
#   define FLT_EPSILON 1.192092896e-07F
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

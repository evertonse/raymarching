#ifndef SHARED_DEFINES_HEADER
#define SHARED_DEFINES_HEADER

#define RENDERER_USING_BINDLESS 1 // Nothing works without it

#ifdef RELEASE
// Define nothing
#else
#   define RENDERER_DEBUG  1
#endif

// TODO: fix convention for these
#define BINDING_INSTANCE_BUFFER           6
#define BINDING_INDICES_BUFFER            5
#define BINDING_VERTEX_BUFFER             3
#define BINDING_VERTEX_TANGENT            7
#define BINDING_TANGENTS_BUFFER           8
#define BINDING_PER_FRAME                 4
#define BINDING_DRAW_COMMAND              18
#define BINDING_MATERIAL                  10
#define BINDING_CAMERA                    2
#define BINDING_BLOOM                     3
#define BINDING_ANIMATION_MATRICES        12
#define BINDING_JOINT_BUFFER              9

#define BINDING_FRAMEBUFFER_DEPTH_TEXTURE        0
#define BINDING_FRAMEBUFFER_NORMAL_TEXTURE       1
#define BINDING_FRAMEBUFFER_POSITION_TEXTURE     2
#define BINDING_FRAMEBUFFER_DIRECT_LIGHT_TEXTURE 3
#define BINDING_AMBIENT_OCCLUSION_TEXTURE        5
#define BINDING_FRAMEBUFFER_HDR_SCENE_TEXTURE    4

#define BINDING_LDR_SCENE_IMAGE               0
#define BINDING_AMBIENT_OCCLUSION_IMAGE       1

#define BINDING_BLUR_INPUT_TEXTURE 0
#define BINDING_BLUR_IMAGE 0

#define FRAMEBUFFER_ATTACHMENTH_COLOR     0
#define FRAMEBUFFER_ATTACHMENTH_NORMAL    1
#define FRAMEBUFFER_ATTACHMENTH_POSITION  2


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

const float fov        = PI/3.;
const float near_plane = 0.005;
const float far_plane  = 5*256.000000;


#endif // SHARED_DEFINES_HEADER

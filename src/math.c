#define RAYMATH_IMPLEMENTATION
#include "raymath.h"
// #include <tgmath.h>

typedef float Matrix4 __attribute__((matrix_type(4, 4)));
typedef float float4 __attribute__((ext_vector_type(4)));

Vector3 spherical_to_cartesian(float theta, float phi) {
  float x = sin(phi)  * cos(theta);
  float y = -sin(phi) * sin(theta);
  float z = cos(phi);
  return (Vector3){x, y, z};
}

static Vector3 MatrixMultiplyVector3(Matrix matrix, Vector3 vector) {
  return Vector3Transform(vector, matrix);
}

Matrix MatrixViewFromSpherical(Vector3 position, float theta, float phi) {
   auto cross     = Vector3CrossProduct;
   auto dot       = Vector3DotProduct;
   auto normalize = Vector3Normalize;

   Vector3 forward = {0.0, 0.0, 1.0};
   Vector3 right   = {1.0, 0.0, 0.0};
   Vector3 up      = {0.0, 1.0, 0.0};

   forward = spherical_to_cartesian(theta, phi);
   right   = cross(up, forward);
   up      = cross(forward, right);

   forward = normalize(forward);
   right   = normalize(right);
   up      = normalize(up);

   Matrix view_old = {
      // Column 1 (right vector)
      right.x, right.y, right.z, 0,
      // Column 2 (right vector)
      up.x, up.y, up.z, 0,
      // Column 3 (right vector)
      forward.x, forward.y, forward.z, 0,
      // Column 4 (translation)
      -dot(right, position),
      -dot(up, position),
      -dot(forward, position),
      1.0
   };

   Matrix rotate = {
      right.x, up.x, forward.x, 0,
      right.y, up.y, forward.y, 0,
      right.z, up.z, forward.z, 0,
      0,       0,    0,         1.0
   };

   Matrix translate = {
      1.0,         0.0,         0.0,         0.0,
      0.0,         1.0,         0.0,         0.0,
      0.0,         0.0,         1.0,         0.0,
      -position.x, -position.y, -position.z, 1.0
   };

   Matrix view = MatrixMultiply(rotate, translate);
   return view;
}


#define add(a, b) _Generic(((a)),                                   \
   Vector4: _Generic(((b)),                                         \
      int:     Vector4AddValue,                                     \
      float:   Vector4AddValue,                                     \
      double:  Vector4AddValue,                                     \
      Vector4: Vector4Add,                                          \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   Vector3: _Generic(((b)),                                         \
      int:     Vector3AddValue,                                     \
      float:   Vector3AddValue,                                     \
      double:  Vector3AddValue,                                     \
      Vector3: Vector3Add,                                          \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   Vector2: _Generic(((b)),                                         \
      int:     Vector2AddValue,                                     \
      float:   Vector2AddValue,                                     \
      double:  Vector2AddValue,                                     \
      Vector2: Vector2Add,                                          \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   int: _Generic(((b)),                                             \
      Vector2: Vector2AddValueSwapped,                              \
      Vector3: Vector3AddValueSwapped,                              \
      Vector4: Vector4AddValueSwapped,                              \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   float: _Generic(((b)),                                           \
      Vector2: Vector2AddValueSwapped,                              \
      Vector3: Vector3AddValueSwapped,                              \
      Vector4: Vector4AddValueSwapped,                              \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   double: _Generic(((b)),                                          \
      Vector2: Vector2AddValueSwapped,                              \
      Vector3: Vector3AddValueSwapped,                              \
      Vector4: Vector4AddValueSwapped,                              \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   )                                                                \
)(((a)), ((b)))

#define sub(a, b) _Generic(((a)),                                   \
   Vector4: _Generic(((b)),                                         \
      int:     Vector4SubtractValue,                                \
      float:   Vector4SubtractValue,                                \
      double:  Vector4SubtractValue,                                \
      Vector4: Vector4Subtract,                                     \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   Vector3: _Generic(((b)),                                         \
      int:     Vector3SubtractValue,                                \
      float:   Vector3SubtractValue,                                \
      double:  Vector3SubtractValue,                                \
      Vector3: Vector3Subtract,                                     \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   Vector2: _Generic(((b)),                                         \
      int:     Vector2SubtractValue,                                \
      float:   Vector2SubtractValue,                                \
      double:  Vector2SubtractValue,                                \
      Vector2: Vector2Subtract,                                     \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   int: _Generic(((b)),                                             \
      Vector2: Vector2SubtractValueSwapped,                         \
      Vector3: Vector3SubtractValueSwapped,                         \
      Vector4: Vector4SubtractValueSwapped,                         \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   float: _Generic(((b)),                                           \
      Vector2: Vector2SubtractValueSwapped,                         \
      Vector3: Vector3SubtractValueSwapped,                         \
      Vector4: Vector4SubtractValueSwapped,                         \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   ),                                                               \
   double: _Generic(((b)),                                          \
      Vector2: Vector2SubtractValueSwapped,                         \
      Vector3: Vector3SubtractValueSwapped,                         \
      Vector4: Vector4SubtractValueSwapped,                         \
      default: COMPILE_ERROR_TYPE_UNSUPPORTED                       \
   )                                                                \
)(((a)), ((b)))


// NOTE: We're forced to use default in all cases from second deep generic because mingwgcc got confused
// Now using 'default:' to handle all cases is fine but we can't use a compile time error. This can't be used because somethin something expression or whatever.
// now we're forced to do them things in runtime asserts
void __invalid_generic();
#define COMPILE_ERROR_TYPE_UNSUPPORTED __invalid_generic

// #define COMPILE_ERROR_TYPE_UNSUPPORTED ((void)_Static_assert(0, "Unsupported multiplication types"), *(int*)0)
#define invert MatrixInvert


#define mul(a, b) _Generic(((a)),                              \
    Vector4: _Generic(((b)),                                   \
        int:     Vector4Scale,                                 \
        float:   Vector4Scale,                                 \
        double:  Vector4Scale,                                 \
        Vector4: Vector4Multiply,                              \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    Vector3: _Generic(((b)),                                   \
        int:     Vector3Scale,                                 \
        float:   Vector3Scale,                                 \
        double:  Vector3Scale,                                 \
        Vector3: Vector3Multiply,                              \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    Vector2: _Generic(((b)),                                   \
        int:     Vector2Scale,                                 \
        float:   Vector2Scale,                                 \
        double:  Vector2Scale,                                 \
        Vector2: Vector2Multiply,                              \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    Matrix: _Generic(((b)),                                    \
        Matrix:  MatrixMultiplySwapped,                        \
        Vector3: MatrixMultiplyVector3,                        \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    int: _Generic(((b)),                                       \
        Vector2: Vector2ScaleSwapped,                          \
        Vector3: Vector3ScaleSwapped,                          \
        Vector4: Vector4ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    float: _Generic(((b)),                                     \
        Vector2: Vector2ScaleSwapped,                          \
        Vector3: Vector3ScaleSwapped,                          \
        Vector4: Vector4ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    double: _Generic(((b)),                                    \
        Vector2: Vector2ScaleSwapped,                          \
        Vector3: Vector3ScaleSwapped,                          \
        Vector4: Vector4ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    )                                                          \
)(((a)), ((b)))


#define length(a) _Generic((a), \
    Vector2: Vector2Length, \
    Vector3: Vector3Length \
)((a))


#define dot(a, b) _Generic((a), \
    Vector2: Vector2DotProduct, \
    Vector3: Vector3DotProduct \
)((a), (b))


#define cross(a, ...) \
    _Generic(((a)), Vector3: Vector3CrossProduct)(((a)), ((__VA_ARGS__)))


// #define normalize(a) Vector3Normalize(a)

#define normalize(a) _Generic((a), \
    Vector3: Vector3Normalize, \
    Vector2: Vector2Normalize \
)((a))


// Note: MatrixMultiply(a, b) returns b * a in raymath; we pass (b, a) to invert.
static inline Matrix MatrixMultiplySwapped(Matrix a, Matrix b) {
   return MatrixMultiply(b, a);
}

static inline Vector4 Vector4ScaleSwapped(float scalar, Vector4 vector) {
   return Vector4Scale(vector, scalar);
}

static inline Vector3 Vector3ScaleSwapped(float scalar, Vector3 vector) {
   return Vector3Scale(vector, scalar);
}

static inline Vector2 Vector2ScaleSwapped(float scalar, Vector2 vector) {
   return Vector2Scale(vector, scalar);
}

static inline Vector2 Vector2AddValueSwapped(float scalar, Vector2 vector) {
   return Vector2AddValue(vector, scalar);
}

static inline Vector3 Vector3AddValueSwapped(float scalar, Vector3 vector) {
   return Vector3AddValue(vector, scalar);
}

static inline Vector4 Vector4AddValueSwapped(float scalar, Vector4 vector) {
   return Vector4AddValue(vector, scalar);
}

static inline Vector2 Vector2SubtractValueSwapped(float scalar, Vector2 vector) {
   return Vector2SubtractValue(vector, scalar);
}

static inline Vector3 Vector3SubtractValueSwapped(float scalar, Vector3 vector) {
   return Vector3SubtractValue(vector, scalar);
}

static inline Vector4 Vector4SubtractValueSwapped(float scalar, Vector4 vector) {
   return Vector4SubtractValue(vector, scalar);
}


typedef struct Transform {
   union {
      Vector3 translation, position;
   };
   Quaternion rotation;
   Vector3    scale;
} Transform;

constexpr Transform transform_identity = {
    .translation = {0},

    .rotation = {
       .x = 0.0f,
       .y = 0.0f,
       .z = 0.0f,
       .w = 1.0f
    },

    // Set scale to one
    .scale = {
       .x = 1.0f,
       .y = 1.0f,
       .z = 1.0f,
    }
};

// Calculate linear interpolation between two floats
double Lerpf64(double start, double end, double amount) {
   double result = start + amount*(end - start);
   return result;
}

Transform TransformInterpolate(Transform t1, Transform t2, float amount) {
   Transform result = {0};
   result.translation = Vector3Lerp(t1.translation, t2.translation, amount);
   result.scale       = Vector3Lerp(t1.scale, t2.scale, amount);
   result.rotation    = QuaternionSlerp(t1.rotation, t2.rotation, amount);
   return result;
}
#define TransformLerp TransformInterpolate


Transform TransformCombine(Transform parent, Transform child) {
    Transform out;

    // Scale: multiply component-wise
    out.scale = Vector3Multiply(parent.scale, child.scale);

    // Rotation: quaternion multiplication (parent * child)
    out.rotation = QuaternionMultiply(parent.rotation, child.rotation);

    // Translation: parent translation + (parent rotation * (parent scale * child translation))
    Vector3 scaled_child_position = Vector3Multiply(child.translation, parent.scale);
    Vector3 rotated_child_position = Vector3RotateByQuaternion(scaled_child_position, parent.rotation);
    out.translation = Vector3Add(parent.translation, rotated_child_position);

    return out;
}


Matrix MatrixCompose(Transform transform) {
   Quaternion q = transform.rotation;
   Vector3 s    = transform.scale;
   Vector3 t    = transform.translation;

   float sx = 2.0f * s.x,
         sy = 2.0f * s.y,
         sz = 2.0f * s.z;

   float xx = q.x*q.x,
         xy = q.x*q.y,
         xz = q.x*q.z,
         xw = q.x*q.w;

   float yy = q.y*q.y,
         yz = q.y*q.z,
         yw = q.y*q.w;

   float zz = q.z*q.z,
         zw = q.z*q.w;

   Matrix m = {0};
   // First column (X axis)
   m.m0 = sx * (- yy - zz + 0.5f);
   m.m1 = sx * (+ xy + zw);
   m.m2 = sx * (- yw + xz);

   // Second column (Y axis)
   m.m4 = sy * (- zw + xy);
   m.m5 = sy * (- xx - zz + 0.5f);
   m.m6 = sy * (+ xw + yz);

   // Third column (Z axis)
   m.m8  = sz * (+ xz + yw);
   m.m9  = sz * (- xw + yz);
   m.m10 = sz * (- xx - yy + 0.5f);

   // Fourth column (Translation)
   m.m12 = t.x;
   m.m13 = t.y;
   m.m14 = t.z;
   m.m15 = 1.0;
   return m;
}

typedef union {
  struct {
    int items[4];
  };
  struct {
    int x;
    int y;
    int z;
    int w;
  };
} Vector4Int;

// Get float array of matrix data
Matrix FloatsToMatrix(float floats[16]) {
   Matrix mat = { 0 };
   mat.m0  = floats[ 0];
   mat.m1  = floats[ 1];
   mat.m2  = floats[ 2];
   mat.m3  = floats[ 3];
   mat.m4  = floats[ 4];
   mat.m5  = floats[ 5];
   mat.m6  = floats[ 6];
   mat.m7  = floats[ 7];
   mat.m8  = floats[ 8];
   mat.m9  = floats[ 9];
   mat.m10 = floats[10];
   mat.m11 = floats[11];
   mat.m12 = floats[12];
   mat.m13 = floats[13];
   mat.m14 = floats[14];
   mat.m15 = floats[15];
   return mat;
}

Vector2 overload vector2(float v) { return (Vector2){v, v}; }
Vector2 overload vector2(float x, float y) { return (Vector2){x, y}; }

Vector3 overload vector3(float v) { return (Vector3){v, v, v}; }
Vector3 overload vector3(float x, float y, float z) { return (Vector3){x, y, z}; }

Vector4 overload vector4(float v) { return (Vector4){v, v, v, v}; }
Vector4 overload vector4(float x, float y, float z, float w) { return (Vector4){x, y, z, w}; }

Vector3 camera_forward(Vector2 spherical) {
   float theta = -spherical.x;
   float phi   = -spherical.y + (PI / 2.0f);

   float x = sinf(phi) * sinf(theta);
   float y = cosf(phi);
   float z = sinf(phi) * cosf(theta);

   return normalize(((Vector3){x, y, z}));
}

typedef struct {
   union {
      Vector3 origin, position;
   };
   Vector3 direction;
} Ray;


Vector3 ndc_to_world(
    Vector3 ndc,  // all three components, each in [-1, 1]
    float fov_y, float aspect,
    float z_near, float z_far,
    Vector3 camera_position, Vector3 camera_forward,  Vector3 camera_right, Vector3 camera_up
) {
   float view_z = z_near + (((ndc.z + 1.)*(z_far-z_near)) / 2.);
   float j = tanf(fov_y * 0.5f)*view_z;
   float view_x = ndc.x*aspect*j;
   float view_y = ndc.y*j;

   Vector3 view_position = vector3(view_x, view_y, view_z);

   Vector3 forward       = camera_forward, right = camera_right, up = camera_up;

   // This is how we do in shader but raymath uses a different coordinate the ours sad.
   // Can't use their shit unless we change it alot.
   // Hence the explcit early return
   Matrix look_at        = MatrixLookAt(camera_position, add(camera_position, camera_forward), camera_up);
   auto look_at_inverted = invert(look_at);
   Vector3 world_pos     = mul(look_at_inverted, view_position);
   return (Vector3){
     right.x * view_x + up.x * view_y + forward.x * view_z + camera_position.x,
     right.y * view_x + up.y * view_y + forward.y * view_z + camera_position.y,
     right.z * view_x + up.z * view_y + forward.z * view_z + camera_position.z,
   };
   return world_pos;
}


void camera_basis(Vector2 spherical, Vector3 *forward, Vector3 *right,Vector3 *up) {
   *forward = camera_forward(spherical);

   // replica of look_at cross product order
   Vector3 world_up = {0.0f, 1.0f, 0.0f};
   *right = normalize(cross(world_up, *forward));
   *up    = normalize(cross(*forward, *right));
}


// TODO: THESE should go in shared or be changable
const float near_plane = 0.005;
const float far_plane = 256.000000;
const float fov    = PI/3.;


Ray compute_mouse_ray(
    float mouse_x, float mouse_y,
    float screen_width, float screen_height,
    float fov_y, float aspect,
    Vector3 camera_position, Vector2 spherical
) {
   float ndc_x =  (mouse_x / screen_width)  * 2.0f - 1.0f;
   float ndc_y = -(mouse_y / screen_height) * 2.0f + 1.0f;

   Vector3 forward, right, up;
   camera_basis(spherical, &forward, &right, &up);
   auto ndc = (Vector3){ ndc_x, ndc_y,  1.0f };
   // printf("mouse={%f, %f}\n", mouse_x, mouse_y);
   // printf("ndc={%f, %f}\n", ndc_x, ndc_y);

   // Two points on the ray at different depths
   Vector3 near_world = ndc_to_world(
      (Vector3){ ndc_x, ndc_y, -1.0f },
      fov_y, aspect,
      near_plane, far_plane,
      camera_position, forward, right, up
   );

   Vector3 near_center_world = ndc_to_world(
      (Vector3){ 0.0, 0.0, -1.0f },
      fov_y, aspect,
      near_plane, far_plane,
      camera_position, forward, right, up
   );

   Vector3 far_world  = ndc_to_world(ndc,
      fov_y, aspect,
      near_plane, far_plane,
      camera_position, forward, right, up
   );

   Ray r = {0};
   // r.origin    = camera_position,
   r.origin    = near_center_world,
   r.direction = normalize(sub(far_world, r.origin));

   return r;
}


bool raycast_ground(Ray ray, Vector3 *hit) {
   // Ray is parallel to ground, no intersection
   if (fabsf(ray.direction.y) < 1e-6f) {
      return false;
   }
   float t = -ray.origin.y / ray.direction.y;
   // Intersection behind the camera
   if (t < 0.0f) {
      return false;
   }
   *hit = (Vector3){
      ray.origin.x + t * ray.direction.x,
      0.0f,
      ray.origin.z + t * ray.direction.z,
   };
   return true;
}


Quaternion billboard_rotation(bool point_aligned, Vector3 position,  Vector3 camera_position, Vector3 camera_forward, Vector3 camera_right, Vector3 camera_up) {
   // View aligned is what Mobas (League) uses for UI elements i.e. health bars.
   // Point aligned is used for things like particles or sprites that need to face the camera from any angle.
   // TODO: Make it into different functions or paremeter, also all these paremeters aren't necessary we're just experiementing with it like
   //       trying to align the billboard basis with the camera basis to see if a better billboard rotation could come out.
   
   const float threshold = 5.f;
   if (point_aligned && length(sub(position, camera_position)) < threshold) {
      point_aligned = false;
   }

   // Using camera_forward makes it parallel to camera plane, uniform across viewport
   Vector3 direction = point_aligned ? normalize(sub(position, camera_position)) : camera_forward;

   // Yaw spin around world Y to face camera in XZ plane
   float yaw = atan2f(direction.x, direction.z);
   Quaternion qy = QuaternionFromAxisAngle((Vector3){0, 1, 0}, yaw);

   // Pitch tilt around right axis to track camera elevation
   float pitch = -asinf(direction.y);
   Quaternion qx = QuaternionFromAxisAngle((Vector3){1, 0, 0}, pitch);

   // Yaw first, then pitch
   return QuaternionMultiply(qy, qx);
}


float randf_range(float lo, float hi) { return lo + ((float)rand() / RAND_MAX) * (hi - lo); }

typedef Vector4 Color;


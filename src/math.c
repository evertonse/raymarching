#define RAYMATH_IMPLEMENTATION
#include "raymath.h"
// #include <tgmath.h>

typedef float Matrix4 __attribute__((matrix_type(4, 4)));
typedef float float4 __attribute__((ext_vector_type(4)));

Vector3 spherical_to_cartesian(float theta, float phi) {
    float x =  sin(phi) * cos(theta);
    float y = -sin(phi) * sin(theta);
    float z =  cos(phi);
    return (Vector3){x, y, z};
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


#define add(a, b) _Generic((a), \
    Vector3: Vector3Add, \
    Matrix: MatrixAdd \
)((a), (b))

#define sub(a, b) _Generic((a), \
    Vector3: Vector3Subtract, \
    Matrix:  MatrixSubtract \
)((a), (b))


// NOTE: We're forced to use default in all cases from second deep generic because mingwgcc got confused
// Now using 'default:' to handle all cases is fine but we can't use a compile time error. This can't be used because somethin something expression or whatever.
// now we're forced to do them things in runtime asserts
void __invalid_generic();
#define COMPILE_ERROR_TYPE_UNSUPPORTED __invalid_generic

// #define COMPILE_ERROR_TYPE_UNSUPPORTED ((void)_Static_assert(0, "Unsupported multiplication types"), *(int*)0)

#define mul(a, b) _Generic(((a)),                              \
    Vector3: _Generic(((b)),                                   \
        int:     Vector3Scale,                                 \
        float:   Vector3Scale,                                 \
        double:  Vector3Scale,                                 \
        Vector3: Vector3Multiply,                              \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    Matrix: _Generic(((b)),                                    \
        Matrix:  MatrixMultiplySwapped,                        \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    int: _Generic(((b)),                                       \
        Vector3: Vector3ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    float: _Generic(((b)),                                     \
        Vector3: Vector3ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    ),                                                         \
    double: _Generic(((b)),                                    \
        Vector3: Vector3ScaleSwapped,                          \
        default: COMPILE_ERROR_TYPE_UNSUPPORTED                \
    )                                                          \
)(((a)), ((b)))


#define dot(a, b) _Generic((a), \
    Vector3: Vector3DotProduct \
)((a), (b))

#define cross(a, ...) \
    _Generic(((a)), Vector3: Vector3CrossProduct)(((a)), ((__VA_ARGS__)))


// #define normalize(a) Vector3Normalize(a)

#define normalize(a) _Generic((a), \
    Vector3: Vector3Normalize \
)((a))


// Note: MatrixMultiply(a, b) returns b * a in raymath; we pass (b, a) to invert.
static inline Matrix MatrixMultiplySwapped(Matrix a, Matrix b) {
   return MatrixMultiply(b, a);
}

static inline Vector3 Vector3ScaleSwapped(double scalar, Vector3 vec) {
   return Vector3Scale(vec, scalar);
}


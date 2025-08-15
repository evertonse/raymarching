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
        Vector3: MatrixMultiplyVector3,                        \
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

typedef struct Transform {
	Vector3    translation;
	Quaternion rotation;
	Vector3    scale;
} Transform;

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
    Vector3 scaledChildPos = Vector3Multiply(child.translation, parent.scale);
    Vector3 rotatedChildPos = Vector3RotateByQuaternion(scaledChildPos, parent.rotation);
    out.translation = Vector3Add(parent.translation, rotatedChildPos);

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

#define RAYMATH_IMPLEMENTATION
#include "raymath.h"

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

#define RAYMATH_IMPLEMENTATION
#include "raymath.h"

// left handed
// RMAPI Matrix MatrixPerspectiveLH(double fovy, double aspect, double nearPlane, double farPlane) {
//     double top = nearPlane * tan(fovy * 0.5);
//     double right = top * aspect;
//
//     Matrix result = {0};
//     double rl = right;
//     double tb = top;
//     double nf = farPlane - nearPlane;
//
//     result.m0  = (float)(nearPlane / rl);
//     result.m5  = (float)(nearPlane / tb);
//     result.m10 = (float)(-(farPlane + nearPlane) / nf);         // ← same as RH, but
//     result.m11 = -1.0f;
//     result.m14 = (float)(-2.0 * nearPlane * farPlane / nf);     // ← flip Z
//     result.m15 = 0.0f;
//
//     return result;

RMAPI Matrix MatrixPerspectiveLH(double fovy, double aspect, double near, double far) {
    float f = 1.0f / tanf(fovy * 0.5f);

    Matrix m = {0};
    m.m0 = f / aspect;
    m.m5 = f;
    m.m10 = (far + near) / (near - far);
    m.m11 = -1.0f;
    m.m14 = (2 * near * far) / (near - far);

    return m;
}



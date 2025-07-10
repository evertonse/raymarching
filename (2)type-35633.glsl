
#version 420 core
      #ifndef lerp
         #define lerp mix
      #endif

      #ifndef PI
         #define PI 3.14159265358979323846
      #endif

      #ifndef TAU
         #define TAU PI * 2.
      #endif

      #ifndef EPSILON
         #define EPSILON 0.000001
      #endif

      #ifndef DEG2RAD
         #define DEG2RAD (PI/180.0)
      #endif

      #ifndef RAD2DEG
         #define RAD2DEG (180.0/PI)
      #endif
   
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 model;
uniform mat4 perspective;

out vec3 Normal;
out vec2 TexCoord;

float remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd) {
   float result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
   return result;
}

vec4 perspective_simplest(vec3 position) {
   return vec4(position.xy, position.z*position.z, position.z);
}

vec4 perspective_from_frustum(vec3 position, float fov_y_rad, float aspect) {
   float z_near = 0.1, z_far = 100.0;

   float f = tan(fov_y_rad * 0.5);
   float y_near = z_near * f;
   float y_far = z_far * f;

   float x_near = z_near * f;
   float x_far = z_far * f;

   float t = remap(position.z, z_near, z_far, 0.0, 1.0);
   float x_actual = remap(t, 0.0, 1.0, x_near, x_far);
   float y_actual = remap(t, 0.0, 1.0, y_near, y_far);

   // Linearly map position from frustum bounds to NDC [-1, 1]
   float x = remap(position.x, -x_actual, x_actual, -1.0, 1.0);
   float y = remap(position.y, -y_actual, y_actual, -1.0, 1.0);
   float z = remap(position.z, z_near,    z_far,    -1.0, 1.0);

   return vec4(x/aspect, y, z, 1.);
}


vec4 perspective_from_frustum(vec3 position) {
   float z_near = 0.1, z_far = 100.0;
   float y_near = 2.0, y_far = 100.0;
   float x_near = 2.0, x_far = 100.0;

   float t = remap(position.z, z_near, z_far, 0.0, 1.0);
   float x_actual = remap(t, 0.0, 1.0, x_near, x_far);
   float y_actual = remap(t, 0.0, 1.0, y_near, y_far);

   // Linearly map position from frustum bounds to NDC [-1, 1]
   float x = remap(position.x, -x_actual, x_actual, -1.0, 1.0);
   float y = remap(position.y, -y_actual, y_actual, -1.0, 1.0);
   float z = remap(position.z, z_near,    z_far,    -1.0, 1.0);

   return vec4(x/(16./8.), y, position.z, 1.);
}

vec4 perspective_from_fov(vec3 position, float fov_y_rad, float aspect, float z_near, float z_far) {
   // Compute Y bounds at near and far using FOV
   float project = tan(fov_y_rad * 0.5) * position.z;
   float x_ndc  = position.x / (aspect * project);
   float y_ndc  = position.y / project;

   float z_ndc = ((position.z - z_near)/abs(z_far-z_near))*2 - 1;
   // float z_ndc = remap(position.z, z_near, z_far, -1.0, 1.0);

   // Considering float precision and depth test, one of these might be actually better.
   // The first one of the returns below is more close with usual non linear z depth scaling seen when using mat4 matrix for perspective
   // We can add some effect of vanishing lines upwards and sideways as well, like when a building is too tall, we have the usual vanishing in z but also in y.
   // It also happens noticebly in x with fish eye lens I think.
   float y_depth = (position.y*0.05);
   float x_depth = (position.x*0.0);
   project += y_depth + x_depth;

   return vec4(position.x / aspect, position.y, z_ndc, project);
   return vec4(position.x / aspect, position.y, z_ndc*project, project);
   return vec4(x_ndc*position.z, y_ndc*position.z, z_ndc*position.z, position.z);
   return vec4(x_ndc, y_ndc, z_ndc, 1.0);
}


mat4 perspective_from_fov_lh(float fov_rad, float aspect, float znear, float zfar) {
    float f = 1.0 / tan(fov_rad * 0.5);

    return mat4(
        f / aspect, 0.0, 0.0,                                   0.0,
        0.0,        f,   0.0,                                   0.0,
        0.0,        0.0, (zfar + znear) / (znear - zfar),       1.0,
        0.0,        0.0, (2.0 * zfar * znear) / (znear - zfar), 0.0
    );
}

mat4 perspective_from_fov(float fov_rad, float aspect, float znear, float zfar) {
    float f = 1.0 / tan(fov_rad * 0.5);

    return mat4(
        f / aspect, 0.0, 0.0,                                    0.0,
        0.0,        f,   0.0,                                    0.0,
        0.0,        0.0, (zfar + znear) / (znear - zfar),       -1.0,
        0.0,        0.0, (2.0 * zfar * znear) / (znear - zfar),  0.0
    );
}



mat2 rotation(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

void main() {
   // float aspect = 1600./800.;
   float aspect = 1600./800.;
   float fov    = PI/3.;

   mat4 gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);

   vec4 position = vec4(position.xyz, 1.0);

   //
   // These are good with simplest perspective
   // vec3 translation = vec3(-.025, -.25, .50);
   // float scale      = 0.3;
   //

   vec3 translation = vec3(-36.55, -10.55, 50.50);
   float scale      = 10.3;

   if (true) { // do perspective
      // position.xz   *= rotation(-PI/0.365);
      position =   model*position;
      position.xyz  *= scale;
      position.xyz  += translation;

      //
      // TODO: Make this style of from frustum work with passing an fov, keep the remap solution tho
      // gl_Position = perspective_from_frustum(position.xyz);
      //

      // gl_Position = perspective * vec4(position.xy, position.z*-1., position.w);
      // gl_Position = gpu_perspective * vec4(position.xy, position.z*-1., position.w);
      // WARNING: This function is mostly the same except for some z-fighting shenanigans
      // gl_Position = perspective_from_fov(position.xyz, fov, aspect, 0.1, 100.);
      gl_Position = perspective_from_frustum(position.xyz, fov, aspect);
   } else {
      gl_Position = position;
   }

   TexCoord = uv;
   Normal   = normal;
}
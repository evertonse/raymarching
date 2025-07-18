
#version 460 core
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


uniform mat4 view;
uniform mat4 model;
uniform mat4 perspective;

// uniform vec3 camera_position;
uniform vec2 spherical;
uniform float u_time;

layout(std140, binding = 2) uniform Camera {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position; float _pad0;
};

// restrict ?
layout(std430, binding = 3) readonly buffer VertexData {
   vec3 positions[];
};

// restrict ?
layout(std430, binding = 4) readonly buffer IndexData {
   int indices[];
};

out vec3 Normal;
out vec2 TexCoord;
flat out int Boolean;

mat4 lookat_rh(vec3 eye, vec3 target, vec3 up) {
    // Calculate forward vector (negative Z axis)
    vec3 f = normalize(target - eye);
    // vec3 zaxis = normalize(target);
    // Calculate right vector (X axis)
    vec3 r = normalize(cross(up, f));
    // Calculate up vector (Y axis)
    vec3 u = normalize(cross(f, r));

    // Create view matrix (column-major)
    return mat4(
       vec4(r, 0.0),
       vec4(u, 0.0),
       vec4(f, 0.0),
       vec4(-dot(r, eye), -dot(u, eye), -dot(f, eye), 1.0)
    );
}

mat4 lookat(vec3 eye, vec3 target, vec3 up) {
    vec3 f = normalize(target - eye);      // forward
    vec3 r = normalize(cross(up, f));      // right
    vec3 u = cross(f, r);                  // up (already normalized by previous step)

    // Column-major layout
    return mat4(
        vec4(r.x, u.x, f.x, 0.0),
        vec4(r.y, u.y, f.y, 0.0),
        vec4(r.z, u.z, f.z, 0.0),
        vec4(-dot(r, eye), -dot(u, eye), -dot(f, eye), 1.0)
    );
}

vec3 spherical_to_cartesian(float theta, float phi) {
   // float x = sin(theta) * cos(phi);
   // float y = sin(theta) * sin(phi);
   // float z = cos(theta);
   // return vec3(x, y, z);

   // float x = sin(phi) * cos(theta);
   // float y = cos(phi);
   // float z = sin(phi) * sin(theta);

   float x =  sin(phi) * cos(theta);
   float y = -sin(phi) * sin(theta);
   float z =  cos(phi);
   return vec3(x, y, z);
}


mat4 view_from_spherical(vec3 position, float theta, float phi) {
   vec3 forward = vec3(0.0, 0.0, 1.0);
   vec3 right   = vec3(1.0, 0.0, 0.0);
   vec3 up      = vec3(0.0, 1.0, 0.0);

   forward = spherical_to_cartesian(spherical.y, 0.0);

   right   = cross(up, forward);
   up      = cross(forward, right);

   // forward = normalize(forward);
   // right   = normalize(right);
   // up      = normalize(up);


   mat4 rotate = mat4(
      right.x, up.x, forward.x, 0,
      right.y, up.y, forward.y, 0,
      right.z, up.z, forward.z, 0,
      0,       0,    0,         1.0
   );

   position = camera_position;

   mat4 translate = mat4(
      1.0,         0.0,         0.0,         0.0,
      0.0,         1.0,         0.0,         0.0,
      0.0,         0.0,         1.0,         0.0,
      -position.x, -position.y, -position.z, 1.0
   );

   mat4 view = rotate * translate;
   return view;
}

float remap(float value, float inputStart, float inputEnd, float outputStart, float outputEnd) {
   float result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
   return result;
}

float smooth_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   float t = clamp((value - in_min) / (in_max - in_min), 0.0, 1.0);
   t = t * t * (3.0 - 2.0 * t); // Smoothstep
   return lerp(out_min, out_max, t);
}

float pow_remap(float value, float in_min, float in_max, float out_min, float out_max, float gamma) {
   float t = clamp((value - in_min) / (in_max - in_min), 0.0, 1.0);
   t = pow(t, gamma); // gamma < 1: faster near, > 1: slower near
   return lerp(out_min, out_max, t);
}

float log_remap(float value, float in_min, float in_max, float out_min, float out_max) {
   float z = clamp((value - in_min) / (in_max - in_min), 0.0001, 1.0);
   float log_z = log(z * 9.0 + 1.0) / log(10.0); // range still [0, 1]
   return lerp(out_min, out_max, log_z);
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


vec3 camera_forward(vec2 r) {
   float theta = -r.x, phi = -r.y + PI/2;
   // float x =  sin(phi) * cos(theta);
   // float y = -sin(phi) * sin(theta);
   // float z =  cos(phi);

   // Original
   float x = sin(phi) * sin(theta);
   float y = cos(phi);
   float z = sin(phi) * cos(theta);

   return normalize(vec3(x, y, z));
}


mat2 rotation(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

void main() {
   // float aspect =1600./800.;
   float aspect = 1600./800.;
   float fov    = PI/3.;
   // vec4 position = vec4(positions[gl_VertexID], positions[gl_VertexID + 1], positions[gl_VertexID + 2], 1.0);
   // vec4 position = vec4(positions[gl_VertexID + 3], positions[gl_VertexID + 2], positions[gl_VertexID + 1], 1.0);
   // vec4 position = vec4(positions[indices[gl_VertexID]], 1.0);
   // vec4 position = vec4(positions[gl_VertexID], 1.);

   // vec4 position = vec4(positions, 1.0);
   vec4 position = vec4(position.xyz, 1.0);
   int positions_count = length(positions);

   if (positions_count == 39) {
      Boolean = true;
   } else {
      Boolean = false;
   }

   mat4 gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);


   //
   // These are good with simplest perspective
   // vec3 translation = vec3(-.025, -.25, .50);
   // float scale      = 0.3;
   //

   vec3 translation = vec3(-36.55, -10.55, 50.50);
   float scale      = 12.3;

   if (true) { // do perspective

      { // Model to World
         position = model*position;
         // position.xz   *= rotation(-PI/0.365);
         position.xyz  *= scale;
         position.xyz  += translation;
      }

      {  // World to Camera
         // position.xz   *= rotation(spherical.x);
         // position.zy   *= rotation(spherical.y);
         // position.xzy  -= camera_position;
         // mat4 view = view_from_spherical(vec3(0., 0., 0.), 0, 0.5);

         vec3 eye = vec3(30., 10., 0.);
         eye = camera_position*2;
         vec3 direction = vec3(0., 0., 1.);
         direction.xz *= rotation(sin(u_time)*(PI/4.));
         direction = spherical_to_cartesian(-spherical.y, spherical.x + PI/2);
         direction = camera_forward(spherical);
         mat4 view = lookat(eye, eye + direction, vec3(0., 1., 0.));
         position = view * position;
      }


      //
      // TODO: Make this style of from frustum work with passing an fov, keep the remap solution tho
      // gl_Position = perspective_from_frustum(position.xyz);
      //

      // gl_Position = position;
      // gl_Position = perspective * vec4(position.xy, position.z*-1., position.w);
      // gl_Position = gpu_perspective * vec4(position.xy, position.z*-1., position.w);
      // WARNING: This function is mostly the same except for some z-fighting shenanigans
      gl_Position = perspective_from_fov(position.xyz, fov, aspect, 0.1, 100.);
      // gl_Position = perspective_from_frustum(position.xyz, fov, aspect);
   } else {
      gl_Position = position;
   }

   TexCoord = uv;
   Normal   = normal;
} 
#version 460 core
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
   

in vec3 Normal;
in vec2 TexCoord;
flat in int Boolean;

layout(location = 0) out vec4 FragColor; // outputting to the color attachment 0

layout(binding = 4) uniform sampler2D tex;
void main() {

   vec3  light   = normalize(vec3(2., 1., 1.));
   float percent = max(0.3, dot(Normal, light));
   FragColor     = texture(tex, TexCoord);

   // FragColor = vec4(0.2, 0.3, 0.2, 1.0)*2.;

   if (true) {
      FragColor *= percent;
   }
   // FragColor.xyz += vec3(.1, .1, .1);
   FragColor.w = 1.0;

   if (Boolean) {
      FragColor.r = 1.0;
   }
} 
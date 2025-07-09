#pragma vertex
#version 420 core
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


vec4 perspective_from_frustum(vec3 position) {
   float z_near = 0.1, z_far = 50.0;
   float y_near = 5.0, y_far = 100.0;
   float x_near = 5.0, x_far = 100.0;

   float t = remap(position.z, z_near, z_far, 0.0, 1.0);
   float x_actual = remap(t, 0.0, 1.0, x_near, x_far);
   float y_actual = remap(t, 0.0, 1.0, y_near, y_far);

   // Linearly map position from frustum bounds to NDC [-1, 1]
   float x = remap(position.x, -x_actual, x_actual, -1.0, 1.0);
   float y = remap(position.y, -y_actual, y_actual, -1.0, 1.0);
   float z = remap(position.z, z_near, z_far, -1.0, 1.0);

   return vec4(x, y, z, 1.);
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


mat2 rotation(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}

void main() {
   // float aspect = 1600./800.;
   float aspect = 800./1600.;
   mat4 gpu_perspective = perspective_from_fov_lh(PI/3., aspect, 1000.0, 0.01);

   vec4 position     = vec4(position.xyz, 1.0);

   //
   // These are good with simplest perspective
   // vec3 translation = vec3(-.025, -.25, .50);
   // float scale      = 0.3;
   //

   vec3 translation = vec3(-.25, -.25, 6.50);
   float scale      = 10.3;

   if (true) { // do perspective
      position.xz  *= rotation(-PI/.365);
      position.xyz *= scale;
      position.xyz += translation;
      // gl_Position = perspective * position;
      // gl_Position = gpu_perspective * position;
      gl_Position = perspective_from_frustum(position.xyz);
   } else {
      gl_Position = position;
   }

   TexCoord = uv;
   Normal   = normal;
}


#pragma fragment
#version 420 core

in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;
layout(binding = 4) uniform sampler2D tex;
void main() {
   vec3  light = normalize(vec3(1., 1., 1.));
   float percent = max(0.3, dot(Normal, light));
   FragColor = texture(tex, TexCoord);

   // FragColor = vec4(0.2, 0.3, 0.2, 1.0)*2.;

   if (false) {
      FragColor *= percent;
   }
   FragColor.w = 1.0;
}

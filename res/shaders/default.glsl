#pragma vertex
#version 420 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 matrix;

out vec3 Normal;
out vec2 TexCoord;



mat4 perspective(float fov_rad, float aspect, float znear, float zfar) {
    float f = 1.0 / tan(fov_rad * 0.5);

    return mat4(
        f / aspect, 0.0, 0.0,                                   0.0,
        0.0,        f,   0.0,                                   0.0,
        0.0,        0.0, (zfar + znear) / (znear - zfar),      -1.0,
        0.0,        0.0, (2.0 * zfar * znear) / (znear - zfar), 0.0
    );
}

void main() {
   // vec3 translation = vec3(0., 0., 1.5);
   // mat4 perspective_matrix = perspective(PI/2., 16./9., 0.1, 100.0);
   // mat4 perspective_matrix = perspective(PI/3., 800/600., 0.01, 1000.0);
   mat4 perspective_matrix = (matrix);

   if (true) {
      vec4 position     = vec4(position.xyz, 1.0);
      vec4 translation  = vec4(0.25, -0.25, 25.0, 1.0);
      float scale       = 50.6;
      // position    = translation + scale * matrix * position;
      position    = translation + scale * position;
      gl_Position = perspective_matrix * position;
   } else {
      vec4 position = vec4(position, 1.0);
      gl_Position =  perspective_matrix * position;
   }

   TexCoord = uv;
   Normal = normal;
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

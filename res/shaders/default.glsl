#pragma vertex
#version 420 core
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;

uniform mat4 matrix;

out vec3 Normal;
out vec2 TexCoord;

void main() {
   // vec3 translation = vec3(0., 0., 1.5);
   if (false) {
      vec3 translation = vec3(-0.25, -0.25, 0.);
      float scale = 1.6;
      vec4 position = vec4(translation + scale * position, 1.0);
      gl_Position = position;
   } else {
      vec4 position = vec4(position, 1.0);
      gl_Position = matrix * position;
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
   vec3 light = normalize(vec3(1., 1., 1.));
   // FragColor = vec4(TexCoord, 1.0, 1.0);
   // FragColor = vec4(Normal, 1.0);
   float percent = max(0.3, dot(Normal, light));
   FragColor = texture(tex, TexCoord) * percent;
   FragColor.w = 1.0;
}


#version 420 core
      #define PI 3.14159
      #define TAU PI * 2.
      #define lerp mix
   

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
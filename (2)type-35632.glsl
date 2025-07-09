
#version 420 core
      #ifndef lerp
         #define lerp 3.14159265358979323846
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
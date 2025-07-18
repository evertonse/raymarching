#define PI 3.14159
#define TAU PI * 2.
#define lerp mix

float t;
void mainImage(out vec4 O, in vec2 coordinate) {
   t = iTime * .825;
   vec2 uv0 = (2.*coordinate - iResolution.xy)/iResolution.y;
   vec2 uv = uv0;
   vec3 finalColor = vec3(0);

   for(int idx = 0; idx < 3; idx++){
      uv = sin(uv);
      uv *= 2.;
      uv = fract(uv);
      uv -= 0.5;

      // uv = cos(uv);


      vec2 m = iMouse.xy/iResolution.xy;
      
      float d = length(uv)-sin(iTime+PI);
      d *= exp(-length(uv0));
      d = sin(d*10.  + iTime);
      d = abs(d);
      // float val = smoothstep(0.1, 0.3, d);
      float val = pow(0.14/d, 1.8);

      vec3 color = vec3(1., sin(-length(uv0) + iTime)*2., 3.*sin(length(uv0)*iTime+PI))*val;
      finalColor += color;
   }
   

   O = vec4(finalColor.xyz/1.4, 1.);
}

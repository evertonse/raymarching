
float raymarch(vec3 pos, vec3 dir);
vec3 camera(vec2 uv);
void rotate(inout vec2 v, float angle);
float sdTorus(vec3 p, vec2 t, float phase);
float mylength(vec2 p);

#define LIGHT
#define HOLES
#define FUNKY

float t;      // time
vec3 ret_col; // color
#ifdef LIGHT
vec3 h; // light amount
#endif

#define RAYMARCH_MAX_STEPS 200
#define NEAR 0.000001
#define FAR 10000.0

#define PI 3.14159
#define TAU PI * 2.

vec3 normalSphere(vec3 p);

vec3 normalScene(vec3 p);

void mainImage(out vec4 c_out, in vec2 f) {
   // t = iTime * .125;
   t = iTime * .825;
   vec3 col = vec3(0., 0., 0.);
   vec2 R = iResolution.xy;

   vec2 uv = vec2(f - R.xy / 2.) / R.y;
   // vec3 dir = camera(uv);

   vec3 camera_pos = vec3(0.0, 1.0, 0.0);
   vec3 camera_dir = normalize(vec3(uv.x, uv.y, 0.9));
   float d = raymarch(camera_pos, camera_dir);

   vec3 intersection = camera_pos + camera_dir*d;
   vec3 light_pos = vec3(sin(t), 8.*(1.+sin(t)*0.5), -cos(t)*10);
   // vec3 light_pos = vec3(1, 8*sin(t), -10);
   vec3 light_dir = normalize(light_pos - intersection);


   vec3 normal = normalScene(intersection);
   float diffuse = dot(light_dir, normal)/1.0;

   // Distance to the scene from intersection point marched towards light direction.
   float dl = raymarch(intersection+normal*0.1, light_dir);

   if (dl < length(intersection - light_pos)) {
      diffuse /= 2;
   }

   c_out.xyz = vec3(clamp(diffuse, 0.0, 1.0));

   d /= 25.0;
   // c_out = vec4(vec3(d), 1.0);
   
   c_out.w = 1.0;
   return;
}

/*
** Leon's mod polar from : https://www.shadertoy.com/view/XsByWd
*/

vec2 modA(vec2 p, float count) {
   float an = TAU / count;
   float a = atan(p.y, p.x) + an * .5;
   a = mod(a, an) - an * .5;
   return vec2(cos(a), sin(a)) * length(p);
}

/*
** end mod polar
*/
float sdSphere(vec3 p) {
   float radius = 1;
   vec4  sphere = vec4(0, 1, 6, radius);
   return length(p - sphere.xyz) - sphere.w;
}

vec3 normalSphere(vec3 p) {
   float radius = 1;
   vec4  sphere = vec4(0, 1, 6, radius);
   return (p - sphere.xyz) / sphere.w;
}


float sdAxisAlignedPlane(vec3 p) {
   return p.y;
}

float sdScene(vec3 p) {
   float dAxisAlignedPlane = sdAxisAlignedPlane(p);
   float dSphere = sdSphere(p);
   return min(dAxisAlignedPlane, dSphere);
}

vec3 normalScene(vec3 p) {
   float dAxisAlignedPlane = sdAxisAlignedPlane(p);
   float dSphere = sdSphere(p);
   if (dAxisAlignedPlane < dSphere) {
      
      return vec3(0., 1., 0.);
   }

   return normalSphere(p);
}


float raymarch(vec3 pos, vec3 dir) {
   vec3 p = vec3(0.0);
   float dist = 0.0;
   for (int i = 0; i < RAYMARCH_MAX_STEPS; ++i) {
      p = pos + dir * dist;
      float sd = sdScene(p);
      dist = dist + sd;
      if (sd < NEAR || dist > FAR) {
         break;
      }
   }
   return dist;
}

float mylength(vec2 p) {
   float ret;

   p = p * p * p * p;
   p = p * p;
   ret = (p.x + p.y);
   ret = pow(ret, 1. / 8.);

   return ret;
}

// Utilities

void rotate(inout vec2 v, float angle) { v = vec2(cos(angle) * v.x + sin(angle) * v.y, -sin(angle) * v.x + cos(angle) * v.y); }

vec2 rot(vec2 p, vec2 ang) {
   float c = cos(ang.x);
   float s = sin(ang.y);
   mat2 m = mat2(c, -s, s, c);

   return (p * m);
}

vec3 camera(vec2 uv) {
   float fov = 1.;
   vec3 forw = vec3(0.0, 0.0, -1.0);
   vec3 right = vec3(1.0, 0.0, 0.0);
   vec3 up = vec3(0.0, 1.0, 0.0);

   return (normalize((uv.x) * right + (uv.y) * up + fov * forw));
}

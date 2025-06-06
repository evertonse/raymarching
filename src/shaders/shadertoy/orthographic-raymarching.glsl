
#define sdf sdScene


#define RAYMARCH_MAX_STEPS 400
// #define NEAR 0.000001
float NEAR = 0.001;
float FAR = 1000.0;
float time;      // time
#define PI 3.14159
#define TAU PI * 2.
#define lerp mix

// Max component
float maxc(vec2 v) {
    return max(v.x, v.y);
}

float maxc(vec3 v) {
    return max(max(v.x, v.y), v.z);
}

float maxc(vec4 v) {
    return max(max(max(v.x, v.y), v.z), v.w);
}


float softshadow(vec3 pos, vec3 dir, float near, float far, float k);
float shadow(vec3 pos, vec3 dir, float near, float far, float k);

float raymarch(vec3 pos, vec3 dir);

float sdTorus(vec3 p, vec2 t, float phase);
float sdScene(vec3 p);

mat2  rotation(float a);
float dot2( in vec3 v ) { return dot(v,v); }



float random(float seed) {
    return fract(sin(seed) * 43758.5453);
}

float sdTorus(vec3 p, float r1, float r2) {
   float x = length(p.xz) - r1;
   float y = p.y;

   float d = sqrt(y*y + x*x) - r2;
   return d;
}

float sigmoid(float x) {
   return 1./(1. + exp(x));
}

// Desmos: \frac{\tanh\left(x-0.5493\right)}{2}+0.5
// Zero maps to 0.25 and smoothly go up to 1 from that
float tanh_skewd(float x) {
   return tanh(x-0.5493)/2. + 0.5;
}

vec3 brdf(vec3 L, vec3 V, vec3 N) {
    int n = 64;
    vec3 H = normalize(L+V);
    float val = pow(max(0,dot(N,H)),n);
    if (true)
        val = val / dot(N,L);
    return vec3(val);
}


vec3 normalScene(vec3 position) {
    float epsilon = 0.001;
    vec3 gradient = vec3(
        sdf(position + vec3(epsilon, 0, 0)) - sdf(position + vec3(-epsilon, 0, 0)),
        sdf(position + vec3(0, epsilon, 0)) - sdf(position + vec3(0, -epsilon, 0)),
        sdf(position + vec3(0, 0, epsilon)) - sdf(position + vec3(0, 0, -epsilon))
    );
    return normalize(gradient);
}

vec3 normalSceneSimpleDerivation(vec3 p) {
    float eps = 0.0001;
    
    float dx = sdScene(vec3(p.x + eps, p.y, p.z)) - sdScene(vec3(p.x - eps, p.y, p.z));
    float dy = sdScene(vec3(p.x, p.y + eps, p.z)) - sdScene(vec3(p.x, p.y - eps, p.z));
    float dz = sdScene(vec3(p.x, p.y, p.z + eps)) - sdScene(vec3(p.x, p.y, p.z - eps));
    
    return normalize(vec3(dx, dy, dz));
}

// Tetrahedral sampling approach with only 4 SDF calls instead of 6:
vec3 normalSceneFast(vec3 p) {
    float eps = 0.0005773; // 1/sqrt(3)
    return normalize(
        eps * vec3(
            sdScene(p + vec3( eps, -eps, -eps)) - sdScene(p + vec3(-eps, -eps,  eps)),
            sdScene(p + vec3(-eps,  eps, -eps)) - sdScene(p + vec3(-eps, -eps,  eps)),
            sdScene(p + vec3(-eps, -eps,  eps)) - sdScene(p + vec3(-eps,  eps, -eps))
        )
    );
}



float sdSegment(vec3 P, vec3 A, vec3 B) {
   // https://youtu.be/PMltMdi1Wzg
   vec3 ab = B-A;
   float t = clamp(dot(P-A, ab)/ dot(ab, ab), 0., 1.);
   // vec3  Q = lerp(A, ab, t);
   vec3  C = A + t*ab;
   float d = length(P-C);
   return d;
}

float sdCapsule(vec3 P, vec3 A, vec3 B, float radius) {
   float d = sdSegment(P, A, B) - radius;
   return d;
}

float sdCapsule2(vec3 P, vec3 A, vec3 B, float radius) {
   vec3 ab = B-A;
   float t = clamp(dot(P-A, ab)/ dot(ab, ab), 0., 1.);
   vec3  C = A + t*ab;
   // float d = length(P-C) - radius*(sin(C.z*5)/2 + 1);
   // float d = length(P-C) - radius*((sin(t* PI*2. + PI)/2 + 1) + (sin(t*PI*10.)/2 + 1));
   float d = length(P-C) - (radius*tanh_skewd(t));
   // float d = length(P-C) - radius*(sin(t)t+0.5);
   return d;
}

// #define sdBox sdBoxV1
// #define sdBox sdBoxV2
#define sdBox sdBoxV3

// My derivation
float sdBoxV1(vec3 p, vec3 dim) {
   float x1 = dim.x/2.0, x2 = -x1;
   float y1 = dim.y/2.0, y2 = -y1;
   float z1 = dim.z/2.0, z2 = -z1;
   // use bvec3 instead
   bool in_bounds_x = p.x <= x1 && p.x >= x2;
   bool in_bounds_y = p.y <= y1 && p.y >= y2;
   bool in_bounds_z = p.z <= z1 && p.z >= z2;

   if (in_bounds_x && in_bounds_y && in_bounds_z) {
      return -min(x1-p.x, min(y1-p.y, z1-p.z));
   }

   if (in_bounds_x && in_bounds_y) {
      return abs(p.z) - z1;
   }

   if (in_bounds_y && in_bounds_z) {
      return abs(p.x) - x1;
   }

   if (in_bounds_z && in_bounds_x) {
      return abs(p.y) - y1;
   }

   if (in_bounds_x) {
      return min(
         min(
            length(p - vec3(p.x, y1, z1)),
            length(p - vec3(p.x, y2, z1))
         ),
         min(
            length(p - vec3(p.x, y1, z2)),
            length(p - vec3(p.x, y2, z2))
         )
      );
   }

   if (in_bounds_y) {
      return min(
         min(
            length(p - vec3(x1, p.y, z1)),
            length(p - vec3(x2, p.y, z1))
         ),
         min(
            length(p - vec3(x1, p.y, z2)),
            length(p - vec3(x2, p.y, z2))
         )
      );
   }

   if (in_bounds_z) {
      return min(
         min(
            length(p - vec3(x1, y1, p.z)),
            length(p - vec3(x2, y1, p.z))
         ),
         min(
            length(p - vec3(x1, y2, p.z)),
            length(p - vec3(x2, y2, p.z))
         )
      );
   }

   return min(
      min(
         min(
            length(p - vec3(x1, y1, z1)),
            length(p - vec3(x1, y1, z2))
         ),
         min(
            length(p - vec3(x1, y2, z1)),
            length(p - vec3(x1, y2, z2))
         )
      ),
      min(
         min(
            length(p - vec3(x2, y1, z1)),
            length(p - vec3(x2, y1, z2))
         ),
         min(
            length(p - vec3(x2, y2, z1)),
            length(p - vec3(x2, y2, z2))
         )
      )
   );
}

float sdBoxV2(vec3 p, vec3 dim) {
   float x = dim.x/2.0;
   float y = dim.y/2.0;
   float z = dim.z/2.0;

   // Move everything to 1 quadrant and by symmetry distances will be correct
   p = abs(p);

   bool in_bounds_x = p.x <= x;
   bool in_bounds_y = p.y <= y;
   bool in_bounds_z = p.z <= z;

   if (in_bounds_x && in_bounds_y && in_bounds_z) {
      return -min(x-p.x, min(y-p.y, z-p.z));
   }

   if (in_bounds_x && in_bounds_y) {
      return p.z - z;
   }

   if (in_bounds_y && in_bounds_z) {
      return p.x - x;
   }

   if (in_bounds_z && in_bounds_x) {
      return p.y - y;
   }

   if (in_bounds_x) {
      return length(p - vec3(p.x, y, z));
   }

   if (in_bounds_y) {
      return length(p - vec3(x, p.y, z));
   }

   if (in_bounds_z) {
      return length(p - vec3(x, y, p.z));
   }
   return length(p - vec3(x, y, z));
}

float sdBoxV3(vec3 p, vec3 dim) {

   // Move everything to 1st octanct (all positive) and by symmetry distances will be correct
   p = abs(p);

   // Box representing 1st octanct
   vec3 octanct = dim/2.0;

   // If any of the axis of 'p' is within the bounds of the octant, adjust that value to align with the corresponding axis of the octanct.
   // Then, perform a length operation using the corner where all three edges meet.
   // Ajustment can be done by snapping values that fall below the octant boundaries to those boundaries.
   p = max(p, octanct);
   // Then distance to the corner, since there's the only case left as it's never inbound anymore.
   return length(p - octanct);
}

#define sdCylinder sdCylinderV2

float sdCylinderV1(vec3 p, float height, float radius) {
   p = abs(p);
   if (p.y > height/2.) {
      vec3 dir = normalize(vec3(p.x, 0, p.z));
      vec3 pos = vec3(0, height/2., 0);
      return length(max(p - (pos + dir*radius), 0));
   }
   return length(vec2(p.x, p.z)) - radius;
}

float sdCylinderV2(vec3 p, float height, float radius) {
   p = abs(p);
   vec3 dir = normalize(vec3(p.x, 0, p.z));
   vec3 pos = vec3(0, height/2., 0);
   return length(max(p - (pos + dir*radius), 0));
}



vec3 normalSphere(vec3 p);

void render(out vec4 color, vec3 camera_pos, vec3 camera_dir);


vec3 R1(vec2 uv, vec3 p, vec3 l, float z) {
    vec3 f = normalize(l-p),
        r = normalize(cross(vec3(0,1,0), f)),
        u = cross(f,r),
        c = p+f*z,
        i = c + uv.x*r + uv.y*u,
        d = normalize(i-p);
    return d;
}

vec3 cameraDir(vec2 uv, vec3 pos, vec3 target, float pespective) {
   vec3 forward = normalize(target - pos);
   vec3 right   = normalize(cross(vec3(0.0, 1.0, 0.0), forward));
   vec3 up      = normalize(cross(forward, right));

   return normalize(
        uv.x       * right
      + uv.y       * up
      + pespective * forward
   );
}

void mainImage(out vec4 color, in vec2 f) {
   time = 0.8*iTime;
   vec2 uv = vec2(f - iResolution.xy / 2.) / iResolution.y;
   // vec3 camera_pos = vec3(0.0, 4.0, -5.);
   vec3 camera_pos = iPosition;


   vec3 camera_dir =  normalize(vec3(uv.x, uv.y, camera_pos.z));

   vec2 m = iMouse.xy/iResolution.xy;
   bool clicking = (iMouse.z == 1.0f);
#if 0
   if (clicking) {
      camera_pos.yz *= rotation(-m.y+.4);
      // camera_pos.xz *= rotation(time*.2-m.x*6.2831);
      camera_pos.xz *= rotation(.2-m.x*6.2831);
      camera_dir = R1(uv, camera_pos, vec3(0,0,0), .7);
   } else {
      camera_pos.xz *= rotation(time*.2-1*6.2831);
   }
   camera_dir = R1(uv, camera_pos, vec3(0,0,0), .7);
#endif


   {
      // camera_pos = iPosition;
      // camera_pos.z /= 100;
      // camera_pos.x /= 100;
      mat2 camera_rotation = rotation(-PI/8.);
      // vec3 camera_dir =  normalize(vec3(0, 0, camera_pos.z + 6));
      // camera_dir =  normalize(vec3(10*uv.x, 10*uv.y, 0) - camera_pos);
      // camera_pos = vec3(uv.x/100, uv.y/100+3, -1.);
      // camera_dir =  normalize(vec3(1 + uv.x, 1, 1));
      // camera_pos = vec3(1, 3, -10.);

      const float widen = 9.;
      camera_pos =  vec3(uv.x*widen, uv.y*widen, -10);
      camera_pos.xy *= (sin(iTime*0.4)/2 + 1);
      camera_pos.y += 2.75;


      // vec3 canvas_pos =  vec3(camera_pos.xy, 0);
      vec3 canvas_pos =  vec3(uv.x*widen, uv.y*widen, 0);
      // canvas_pos.xz *= rotation(PI/4.*sin(iTime));
      // camera_dir =  vec3(0, 0, 1);
      camera_dir = normalize(canvas_pos - camera_pos);

      // camera_dir =  normalize(vec3(uv.x, uv.y, 1));
      if (clicking) {
         // camera_pos.xz *= rotation(time*.2-m.x*6.2831);
         camera_pos.xz *= rotation(.2-m.x*6.2831);
         camera_dir.xz *= rotation(.2-m.x*6.2831);

         // camera_pos.yz *= rotation(-m.y+.4);
         // camera_dir.yz *= rotation(-m.y+.4);
      }
   }

   render(color, camera_pos, camera_dir);
   return;
}

void render(out vec4 color, vec3 camera_pos, vec3 camera_dir) {
   float d = raymarch(camera_pos, camera_dir);

   vec3 intersection = camera_pos + camera_dir*d;
   // vec3 light_pos = vec3(sin(time), 8.*(1.+sin(time)*0.5), -cos(time)*10);
   vec3 light_pos = vec3(1, 8, 3);
   vec3 light_dir = normalize(light_pos - intersection);


   vec3 normal = normalScene(intersection);
   float diffuse = dot(light_dir, normal)/1.0;
   float spec = 0;
   if (d < FAR) {
      // spec = brdf(camera_dir, -light_dir, normal).x;
      spec = brdf(normalize(intersection + camera_pos), normalize(intersection + light_pos), normal).x;
   }

   // float dl = raymarch(intersection+normal*0.1, light_dir);
   // vec3 intersection_offset = normal*NEAR;
   vec3 intersection_offset = normal*0.1;
   // NEAR =<< 2.;
   
   // Distance to the scene from intersection point marched towards light direction.
   // NOTE: Should we check if we're way past the light origin? because technically we could've hit something way beyond the light and it would count as a shadow point.
   float dl = raymarch(intersection + intersection_offset, light_dir);
   // float shadow_result = shadow(intersection + intersection_offset, light_dir, NEAR/2, FAR, 8);
   float shadow_result = softshadow(intersection + intersection_offset, light_dir, NEAR/2, FAR, 64);
   float percent = shadow_result;
   // percent = ;

   diffuse *= clamp(percent, 0., 1.);
   spec *= clamp(percent, 0., 1.);
   // diffuse *= clamp(percent, 0., 1.);
   if (dl < length(intersection - light_pos)) {
      // diffuse *= clamp(percent, 0., 1.);
   }

   color.xyz = vec3(clamp(diffuse, 0.0, 1.) + spec/1.);
   if (shadow_result > 1.) {
      // color.xyz = vec3(0.0);
      // color.y = 1.0;
   }
   
   color.w = 1.0;
   // color.xyz = pow(color.xyz, vec3(.4545));	// gamma correction
   color.xyz = pow(color.xyz, vec3(.8545));	// gamma correction
   return;
}


float sdSphere(vec3 p, vec3 center, float radius) {
   return length(p - center.xyz) - radius;
}

vec3 normalSphere(vec3 p) {
   float radius = 1;
   vec4  sphere = vec4(0, 1, 6, radius);
   return (p - sphere.xyz) / sphere.w;
}


float sdAxisAlignedPlane(vec3 p) {
   return p.y;
}


float sdPlane(vec3 p, vec3 Q, vec3 normal) {
   vec3 n = normalize(normal);
   // Since length(n) == 1 and dot(n, n) = length(n)^2 we can drop
   // float d = dot(p-Q, n)/dot(n,n);
   float d = dot(p-Q, n);
   return d;
}

float areaTriangle(vec3 A, vec3 B, vec3 C) {
   return length(cross(B-A, C-A))/2.;
}

// IQ's triangle sdf
float sdTriangleIQ(in vec3 p, in vec3 v1, in vec3 v2, in vec3 v3) {
    vec3 v21 = v2 - v1; vec3 p1 = p - v1;
    vec3 v32 = v3 - v2; vec3 p2 = p - v2;
    vec3 v13 = v1 - v3; vec3 p3 = p - v3;
    vec3 nor = cross( v21, v13 );

    return sqrt( (sign(dot(cross(v21,nor),p1)) + 
                  sign(dot(cross(v32,nor),p2)) + 
                  sign(dot(cross(v13,nor),p3))<2.0) 
                  ?
                  min( min( 
                  dot2(v21*clamp(dot(v21,p1)/dot2(v21),0.0,1.0)-p1), 
                  dot2(v32*clamp(dot(v32,p2)/dot2(v32),0.0,1.0)-p2) ), 
                  dot2(v13*clamp(dot(v13,p3)/dot2(v13),0.0,1.0)-p3) )
                  :
                  dot(nor,p1)*dot(nor,p1)/dot2(nor) );
}


// My deviration
float sdTriangle(vec3 p, vec3 A, vec3 B, vec3 C) {
   // Calculate triangle normal
   vec3 n = normalize(cross(B - A, C - A));

   // Distance from point to triangle plane
   float dPlane = sdPlane(p, A, n);

   // Project point onto triangle plane, -n (minus n) because we're going into the plane direction.
   vec3 W = p - n * dPlane;


   // P = Au + Bv + Cw;
   // total = area(A, B, C)
   // u = area(P, B, C)/total
   // v = area(P, A, C)/total
   // w = area(P, C, B)/total
   float totalArea = areaTriangle(A, B, C);

   // Calculate barycentric coordinates
   float u = areaTriangle(W, B, C) / totalArea; // Weight for vertex A
   float v = areaTriangle(W, C, A) / totalArea; // Weight for vertex B
   float w = areaTriangle(W, A, B) / totalArea; // Weight for vertex C

   // Check if point is inside triangle
   // All barycentric coordinates should be >= 0 and sum to 1
   float eps = 0.001;

   // This below are all valid checks:
   // bool inside = u >= 0.0 && v >= 0.0 && w >= 0.0 && abs(u + v + w - 1.0) < eps;
   // bool inside = abs(u + v + w - 1.0) < eps;
   // bool inside = abs(areaTriangle(W, B, C) + areaTriangle(W, C, A) + areaTriangle(W, A, B) - totalArea) < eps;
   bool inside = (areaTriangle(W, B, C) + areaTriangle(W, C, A) + areaTriangle(W, A, B) < totalArea + eps);

   // This right below is not enought we need to check it:
   //    bool inside = u >= 0.0 && v >= 0.0 && w >= 0.0;

   if (inside) {
      // If not absolute it's bugged we need only absolute distance
      return abs(dPlane); // Distance to plane if inside triangle projection
   }

   // If outside, find minimum distance to triangle edges
   float d1 = sdSegment(p, A, B);
   float d2 = sdSegment(p, B, C);
   float d3 = sdSegment(p, C, A);

   return min(min(d1, d2), d3);
}

float sdScene(vec3 p) {
   // float dplane = sdPlane(p, vec3(0,3.9,0), vec3(sin(time)*9, 1, 0));
   float dPlane = sdPlane(p, vec3(0,2,0), vec3(0.71, 0.71, 0));
   float dAxisAlignedPlane = sdAxisAlignedPlane(p);

   float dTriangle = -0.009 + sdTriangle(p, vec3(-.5, 2, 1), vec3(0, 1.75+2, 0), vec3(.5, 2, 1));

   vec3 posBox = p - vec3(2,1,1);
   posBox.zx *= rotation(PI/5.);
   posBox.zy *= rotation(PI/5.);

   vec3 posBox2 = p - vec3(0,1,1);
   // posBox2.zy *= rotation(-PI/5.);
   float dBox = min(sdBox(posBox2, vec3(1,1,1)*2), sdBox(posBox, vec3(1,0.5,2.6)));

   vec3  posCylinder = p - vec3(-2,1.5,1);
   posCylinder.xy *= rotation(45);
   float dCylinder = sdCylinder(posCylinder, 2.0, 0.4);

   // float dCapsule = sdCapsule(p, vec3(-2., 1.3+sin(time), 4), vec3(2., .3, 8), 0.095);
   float capsuleThickness = 5.;
   float dCapsule = min(
         sdCapsule(p, vec3(-2., 1.3+sin(time), 4), vec3(2., .3, 8), 0.195),
         sdCapsule(p, vec3(-2., .3, 8), vec3(2., 1.3+sin(time), 4), 0.195)
   );

   float dTorus  = sdTorus(p - vec3(4., 0.89, 6.), 1.8, .12);
   float dSphere = sdSphere(p, vec3(0, 1, 6), 1);

   float d = dAxisAlignedPlane;
   

   d = min(d, dCylinder);
   d = min(d, dBox);
   d = min(d, dTriangle);
   d = min(d, dSphere);
   d = min(d, dTorus);
   d = max(d, -dCapsule);

   float dcaps = sdCapsule2(p -vec3(1,1,1), vec3(-2., 1.9, 8), vec3(2., 1.3, 4), 0.495);
   d = min(d, dcaps);
   
   // d = max(d, -dCapsule);
   // d = min(d, dTorus);

   return d;
}

float raymarch(vec3 pos, vec3 dir) {
   float dist = 0.0;
   for (int i = 0; i < RAYMARCH_MAX_STEPS; ++i) {
      float sd = sdScene(pos + dir*dist);
      dist += sd;
      // if (abs(sd) < NEAR || dist > FAR) { // We do drop fps with this absolute, make sure to use it when you needit
      if (sd < NEAR || dist > FAR) {
         break;
      }
   }
   return dist;
}


float shadow(vec3 pos, vec3 dir, float near, float far, float k) {
   float dist = near;
   float res = 1.0;
   // void(k);
   for (int i = 0; i < RAYMARCH_MAX_STEPS && dist < far; ++i) {
      float sd = sdScene(pos + dir*dist);
      dist += sd;
      if (sd < near) {
         res = 1./(exp(-6.9*dist +4.)+1);
         break;
      }
      if (sd > far) {
         res = 1.0;
         break;
      }
   }
   return res;
   // return atan(2*res)*PI/4.4;
}

float softshadow(vec3 pos, vec3 dir, float near, float far, float k) {
   float res = 1.0;
   float dist = near;
   for (int i = 0; i < RAYMARCH_MAX_STEPS && dist < far; ++i) {
      float sd = sdScene(pos + dir*dist);
      dist += sd;
      if (sd < near) {
         res = 0.0;
         break;
      }
      res = min(res, k*sd / dist);
   }
   return res;
}

mat2 rotation(float a) {
    float s = sin(a);
    float c = cos(a);
    return mat2(c, -s, s, c);
}



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
layout(std140, binding = 2) uniform Camera2 {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position; float _pad0;
    vec3 camera_direction; float _pad1;
} camera;

struct Light {
    vec3 position; float pad0;
    vec3 ambient;  float pad1;
    vec3 diffuse;  float pad2;
    vec3 specular; float pad4;
};

struct Camera {
    vec3  position; float pad0;
    float theta, phi, pad1, pad2;
};


layout(std140, binding = 4) uniform Per_Frame {
    mat4 model, view, perspective;
    Light  light;
    Camera camera;
    float elapsed_time, delta_time;
} per_frame;


layout(std140, binding = 5) uniform Ub_Data_Buffer {
    vec4 random_data;
} ub_data_buffer;
// You can call positions_xyz.lenght() to get the the count of positions
layout(std430, binding = 3) buffer VertexData {
   float positions_xyz[];
};
layout(std430, binding = 5) buffer IndexData {
   float indices[];
};

uniform mat4 view;
uniform mat4 model;
uniform mat4 perspective;
uniform bool is_light;

uniform vec3 camera_position;
uniform vec2 spherical;
uniform float u_time;


out vec3 Position;
out vec3 Normal;
out vec2 TexCoord;
out flat int special;

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
mat4 matrix_translation_row(vec3 translation) {
    return mat4(
        1.0, 0.0, 0.0, translation.x,
        0.0, 1.0, 0.0, translation.y,
        0.0, 0.0, 1.0, translation.z,
        0.0, 0.0, 0.0, 1.0
    );
}

mat4 matrix_translation(vec3 translationVector) {
    return mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        translationVector.x, translationVector.y, translationVector.z, 1.0
    );
}


mat4 matrix_scale(vec3 scale) {
    return mat4(
        scale.x, 0.0,     0.0,     0.0,
        0.0,     scale.y, 0.0,     0.0,
        0.0,     0.0,     scale.z, 0.0,
        0.0,     0.0,     0.0,     1.0
    );
}

mat4 matrix_rotation(vec3 axis, float angle) {
    // Normalize the axis vector
    axis = normalize(axis);

    float c = cos(angle);
    float s = sin(angle);
    float t = 1.0 - c;

    float x = axis.x;
    float y = axis.y;
    float z = axis.z;

    return mat4(
        t * x * x + c,       t * x * y - s * z,    t * x * z + s * y,    0.0,
        t * x * y + s * z,   t * y * y + c,        t * y * z - s * x,    0.0,
        t * x * z - s * y,   t * y * z + s * x,    t * z * z + c,        0.0,
        0.0,                  0.0,                  0.0,                 1.0
    );
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

vec3 pull_position(int id) {
   return vec3(
      positions_xyz[id*3 + 0],
      positions_xyz[id*3 + 1],
      positions_xyz[id*3 + 2]
   );

}
#define PULLING

const float aspect = 1600./800.;
const float fov    = PI/3.;

void main() {
   special = 0;

#ifdef PULLING
   vec4 position = vec4(pull_position(gl_VertexID), 1.0);
#else
   vec4 position = vec4(position.xyz + vec3(10), 1.0);
#endif

   if (is_light) {
   }

   float positions_count = positions_xyz.length();
   mat4 gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);
   vec3 translation = vec3(-36.55, -10.55, 50.50);
   float scale      = 12.3;

   if (is_light) {
      // scale = 20.3;
   }

   // World position send to next stage
   position = per_frame.model*position;
   Position = position.xyz;
   if (true) {
      // See more about the normal matrix: http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
      Normal = mat3(transpose(inverse(per_frame.model))) * normal; // Apply mat3 to "drop" the translation portion
      // Normal = ((transpose(inverse(per_frame.model)) * vec4(normal, 0.)).xyz);
   } else {
      Normal = normal.xyz;
   }


   if (length(position.xyz) < 10.) {
      special = 1;
      // position = matrix_translation(translation)*matrix_rotation(vec3(1.), PI/2.) * matrix_scale(vec3(scale))*model*position;
      // position =  * per_frame.model*position;
   }
   // position = per_frame.model * matrix_rotation(vec3(1.), PI/2.) * position;
   // position.xyz  *= scale;
   // position.xyz  += translation;
   // position.xyz  += translation/2.;

   {  // World to Camera
      // position.xz   *= rotation(spherical.x);
      // position.zy   *= rotation(spherical.y);
      // position.xzy  -= camera_position;
      // mat4 view = view_from_spherical(vec3(0., 0., 0.), 0, 0.5);

      vec3 eye = per_frame.camera.position;
      vec3 direction = vec3(0., 0., 1.);
      direction = spherical_to_cartesian(-spherical.y, spherical.x + PI/2);
      direction = camera_forward(spherical);
      // direction.xz *= rotation(sin(per_frame.elapsed_time));
      // direction = camera_forward(spherical);
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
   gl_Position = perspective_from_fov(position.xyz, fov, aspect, 0.1, 100.); // Appears to be infinite in depth
   // gl_Position = perspective_from_frustum(position.xyz, fov, aspect);

   TexCoord = uv;
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
   

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
in flat int special;
uniform float u_time;

layout(location = 0) out vec4 FragColor; // Outputting to the Color Attachment 0 in the Framebuffer
layout(binding  = 3) uniform sampler2D diffuse_texture;
layout(binding  = 4) uniform sampler2D specular_texture;
layout(binding  = 5) uniform sampler2D emissive_texture;

uniform bool has_specular;
uniform bool has_emissive;
uniform bool is_light;
uniform vec3 camera_position;
layout(std140, binding = 2) uniform Camera2 {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position; float _pad0;
    vec3 camera_direction; float _pad1;
} camera;

struct Light {
    vec3 position; float pad0;
    vec3 ambient;  float pad1;
    vec3 diffuse;  float pad2;
    vec3 specular; float pad4;
};

struct Camera {
    vec3  position; float pad0;
    float theta, phi, pad1, pad2;
};


layout(std140, binding = 4) uniform Per_Frame {
    mat4 model, view, perspective;
    Light  light;
    Camera camera;
    float elapsed_time, delta_time;
} per_frame;


layout(std140, binding = 5) uniform Ub_Data_Buffer {
    vec4 random_data;
} ub_data_buffer;

vec3 brdf_blinn_phong(
      vec3 light_direction, vec3 view_direction, vec3 normal,
      vec3 diffuse_color,       vec3 specular_color,
      vec3 light_diffuse_color, vec3 light_specular_color, vec3 light_ambient_color,
      float specular_exponent,  float attenuation
) {

   vec3 position = Position;

   // TODO: use half vector instead
   vec3 wi = normalize(light_direction);
   vec3 wo = normalize(view_direction);
   vec3 n  = normalize(normal);

   // vec3 ambient_color = diffuse_color * specular_color;
   vec3 ambient_color =  0.715160 * diffuse_color  + 0.062671 * specular_color;
   // vec3 ambient_color = vec3(0.212671*diffuse_color.r, 0.715160*diffuse_color.g, 0.072169*diffuse_color.b);


   // Table of materials and constants for ambient: http://devernay.free.fr/cours/opengl/materials.html
   float ambient_intesity  = attenuation * 0.35 * (0.212671*ambient_color.r + 0.715160*ambient_color.g + 0.072169*ambient_color.b)/(0.212671*diffuse_color.r + 0.715160*diffuse_color.r + 0.072169*diffuse_color.r);
   float diffuse_intesity  = attenuation * 0.5;
   float specular_intesity = attenuation * 0.25;

   const bool use_half_vector = true;
   float specular_term = 0;
   if (use_half_vector) {
      vec3 h = normalize(wo + wi);
      specular_term = dot(n, h);
   } else {
      vec3 r = -reflect(wi, normal);
      specular_term = dot(r, wo);
   }

   vec3 diffuse  = light_diffuse_color  * diffuse_color  * max(0, dot(wi, n));
   vec3 specular = light_specular_color * specular_color * pow(max(0, specular_term), specular_exponent);
   vec3 ambient  = light_ambient_color  * ambient_color;

   return  (diffuse_intesity  * diffuse)
         + (specular_intesity * specular)
         + (ambient_intesity  * ambient);
}

float n = 10; // 1 100
float ior = 1.5; // 1 2.5
bool include_Fresnel = false;
bool divide_by_NdotL = true;

vec3 BRDF( vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y )
{
    vec3 H = normalize(L+V);

    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);

    float x = acos(NdotH) * n;
    float D = exp( -x*x);
    float G = (NdotV < NdotL) ?
        ((2*NdotV*NdotH < VdotH) ?
         2*NdotH / VdotH :
         1.0 / NdotV)
        :
        ((2*NdotL*NdotH < VdotH) ?
         2*NdotH*NdotL / (VdotH*NdotV) :
         1.0 / NdotV);

    // fresnel
    float c = VdotH;
    float g = sqrt(ior*ior + c*c - 1);
    float F = 0.5 * pow(g-c,2) / pow(g+c,2) * (1 + pow(c*(g+c)-1,2) / pow(c*(g-c)+1,2));

    float val = NdotH < 0 ? 0.0 : D * G * (include_Fresnel ? F : 1.0);

    if (divide_by_NdotL)
        val = val / dot(N,L);
    return vec3(val);
}


vec3 brdf_blinn_phong(
      vec3 light_direction, vec3 view_direction, vec3 normal,
      vec3 diffuse_color,       vec3 specular_color,
      float specular_exponent,  float attenuation
) {
   return brdf_blinn_phong(light_direction, view_direction, normal, diffuse_color, specular_color, vec3(1.), vec3(1.), vec3(1.), specular_exponent, attenuation);
}


vec3 gamma_correction(vec3 colour) {
   float gamma = 1.0;
   return pow(colour, vec3(1. / gamma));
}

void main() {
   vec3 position = Position;
   vec3 normal   = normalize(Normal);


   vec3 light_direction = normalize(per_frame.light.position - position);
   vec3 view_direction  = normalize(per_frame.camera.position - position);

   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;

   float distance_to_light = length(position - per_frame.light.position);
   float distance_to_view  = length(position - vec3(per_frame.camera.position.x, 0., per_frame.camera.position.z)); // Ignoring height of view

   if (special == 1) {
      FragColor.r = 1.0;
   }

#if 0
   vec4 rand = ub_data_buffer.random_data;
   if (rand.x == 69.) {
      FragColor.g = 1.0;
   }

   if (rand.x == 68.) {
      FragColor.b = 1.0;
   }
#endif


   vec3 light_diffuse_color  = per_frame.light.diffuse;
   vec3 light_ambient_color  = per_frame.light.ambient;
   vec3 light_specular_color = per_frame.light.specular;
   const bool rain_bow_light = false;

   if (rain_bow_light) {
      light_diffuse_color  = vec3(sin(per_frame.elapsed_time*1.3)/2. + 1.0, sin(per_frame.elapsed_time*2)/4. + 0.5, sin(per_frame.elapsed_time*0.7)/4. + 0.5);
      light_ambient_color  = light_diffuse_color * vec3(0.2f);
      light_specular_color = vec3(0.92f);

   }


   if (has_specular) {
      specular_color = vec3(1.0);
      light_specular_color = vec3(1.0);
      specular_color  = 2*texture(specular_texture, TexCoord).xyz;
   }

   float attenuation_distance = clamp(50/distance_to_light, 0.20, 1.0);
   vec3 color =
      brdf_blinn_phong(
         light_direction, view_direction, normal,
         diffuse_color, specular_color,
         light_diffuse_color, light_ambient_color, light_specular_color,
         64., attenuation_distance
      );

   const bool test_elapsed_time = false;
   if (test_elapsed_time) {
      FragColor = vec4(sin(per_frame.elapsed_time), per_frame.delta_time*100, .0, 1.); return;
   }

   if (has_emissive && has_specular) {
      // color += (attenuation_distance * texture(emissive_texture, TexCoord).xyz);
      if ((specular_color.z + specular_color.y + specular_color.x) > 0.1) {
         const float time_factor = sin(per_frame.elapsed_time * 2.9)/2. + 0.5;
         // color += specular_color + time_factor * texture(emissive_texture, TexCoord).xyz;
         const vec3 emissive_color = texture(emissive_texture, TexCoord).xyz;
         color += specular_color * (emissive_color.y + emissive_color.x + emissive_color.z);
      }
      // color += (specular_color * texture(emissive_texture, TexCoord).xyz);
      // FragColor.xyz += (vec3(0.2)-specular_color/2) * texture(emissive_texture, TexCoord).xyz;
      // FragColor.xyz = texture(emissive_texture, TexCoord).xyz;
      // FragColor.xyz = vec3(1.);
   }

   float attenuation_alpha = clamp(896./distance_to_view, 0.2, 1.0);
   FragColor = vec4(color, attenuation_alpha);



   if (is_light) {
      // FragColor = vec4(light_color, 1.0);
      FragColor = vec4(light_ambient_color, 1.0);
   }

   FragColor.xyz = gamma_correction(FragColor.xyz);
} 

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
// You can call vertex_buffer.length() to get the the count of positions
layout(std430, binding = 3) buffer VertexData {
   float vertex_buffer[];
};
layout(std430, binding = 12) buffer Animation_Matrices {
   mat4 geometry_to_model[];
};

struct Joint_Data {
   ivec4 joint_idxs;     // index into geometry_to_model
   vec4  joint_weights;  // \sum_over_(i=4){joint_weights[i] * bone_idxs[i]}
};

layout(std430, binding = 9) buffer Animation_Bones {
   Joint_Data joint_data[];
  // for (int i = 0; i < 4; ++i) {
  //       mat4 bone_transform = geometry_to_model[bone_idxs[i]];
  //       position += bone_weights[i] * (bone_transform * vec4(position, 1.0));
  //   }
  //
  //   gl_Position = uModelViewProjection * vec4(position, 1.0);
};
layout(std430, binding = 5) buffer IndexData {
   float indices[];
};

uniform mat4 view;
uniform mat4 model;
uniform mat4 perspective;
uniform bool is_light;
uniform int has_animation = -1;

uniform vec3 camera_position;
uniform vec2 spherical;


out Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TexCoord;
};

out Flat {
   flat int special;
};

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

mat4 matrix_transform(vec3 translation, vec3 scale, vec4 rotation) {
    mat4 matrix = mat4(1.0);

    // Apply translation
    matrix[3] = vec4(translation, 1.0);

    // Apply rotation
    if (rotation.w != 0.0) {
        vec3 axis = normalize(rotation.xyz);
        float angle = rotation.w;

        float c = cos(angle);
        float s = sin(angle);
        float t = 1.0 - c;

        vec3 x = vec3(
            t * axis.x * axis.x + c,
            t * axis.x * axis.y - s * axis.z,
            t * axis.x * axis.z + s * axis.y
        );

        vec3 y = vec3(
            t * axis.x * axis.y + s * axis.z,
            t * axis.y * axis.y + c,
            t * axis.y * axis.z - s * axis.x
        );

        vec3 z = vec3(
            t * axis.x * axis.z - s * axis.y,
            t * axis.y * axis.z + s * axis.x,
            t * axis.z * axis.z + c
        );

        mat4 rotationMatrix = mat4(
            vec4(x, 0.0),
            vec4(y, 0.0),
            vec4(z, 0.0),
            vec4(0.0, 0.0, 0.0, 1.0)
        );

        matrix = matrix * rotationMatrix;
    }

    // Apply scale
    matrix[0][0] *= scale.x;
    matrix[1][1] *= scale.y;
    matrix[2][2] *= scale.z;

    return matrix;
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


#define PULLING

#ifdef PULLING
vec3 pull_position(int id) {
   return vec3(
      vertex_buffer[id*3 + 0],
      vertex_buffer[id*3 + 1],
      vertex_buffer[id*3 + 2]
   );

}

vec3 pull_normal(int id) {
    int num_vertices = vertex_buffer.length() / 8;  // Total vertices
    int normal_offset = num_vertices * 3;           // Offset to normals section
    // return normal;
    return vec3(
        vertex_buffer[id*3 + 0 + normal_offset],
        vertex_buffer[id*3 + 1 + normal_offset],
        vertex_buffer[id*3 + 2 + normal_offset]
    );
}

vec2 pull_uv(int id) {
    int num_vertices = vertex_buffer.length() / 8;  // Total vertices
    int uv_offset = num_vertices * 6;               // Offset to UV section (after positions + normals)
    
    return vec2(
        vertex_buffer[id*2 + 0 + uv_offset], 
        vertex_buffer[id*2 + 1 + uv_offset]
    );
}
#endif

const float aspect = 1600./800.;
const float fov    = PI/3.;
// const float fov    = PI/4;

void main() {
   special = 0;

#ifdef PULLING
   vec4 position = vec4(pull_position(gl_VertexID), 1.0);
   vec3 normal   = pull_normal(gl_VertexID);
   vec2 uv       = pull_uv(gl_VertexID);
   // uv = vec2(0);
#else
   vec4 position = vec4(position.xyz,  1.0);
#endif

   float positions_count = vertex_buffer.length();
   mat4  gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);

   // World position send to next stage
   // position.xz *= rotation(per_frame.elapsed_time * 0.2);
   mat4 model = per_frame.model;
   if (has_animation >= 0) {
      // position = geometry_to_model[has_animation]*vec3(0);
      vec4 translation = geometry_to_model[has_animation] * vec4(0., 0., 0., 1.);
      translation.x += 150.;
      translation.y += 20.;
      mat4 model = matrix_transform(translation.xyz, vec3(1.), vec4(1));
      position.xyz *= 10;
      position = position + translation;
      // position = model * position;
   } else {
      if (has_animation == -69) {
         ivec4 joint_idxs    = joint_data[gl_VertexID].joint_idxs;
         vec4  joint_weights = joint_data[gl_VertexID].joint_weights;
         if (length(joint_weights) != 0) {
         }
         position =
              joint_weights[0] * (geometry_to_model[joint_idxs[0]] * position)
            + joint_weights[1] * (geometry_to_model[joint_idxs[1]] * position)
            + joint_weights[2] * (geometry_to_model[joint_idxs[2]] * position)
            + joint_weights[3] * (geometry_to_model[joint_idxs[3]] * position);
         // position = model * position;
      } else {
         position = model * position;
      }
   }

   { // Send to next shader
      // Everything is sent in World Space
      Position = position.xyz;
      // See more about the normal matrix: http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
      if (true) {
         // Apply mat3 to "drop" the translation portion
         Normal = mat3(transpose(inverse(model))) * normal;
         // Normal = ((transpose(inverse(per_frame.model)) * vec4(normal, 0.)).xyz);
      } else {
         Normal = normal.xyz;
      }
   }



   {  // World to Camera
      // mat4 view = view_from_spherical(vec3(0., 0., 0.), 0, 0.5);
      vec3 eye = per_frame.camera.position;
      vec3 direction = vec3(0., 0., 1.);
      // direction = -spherical_to_cartesian(-spherical.y, spherical.x + PI/2);
      direction = camera_forward(spherical);
      mat4 view = lookat(eye, eye + direction, vec3(0., 1., 0.));
      position = view * position;
   }


   gl_Position = perspective_from_fov(position.xyz, fov, aspect, 0.1, 100.); // Appears to be infinite in depth
   // gl_Position = per_frame.perspective * vec4(position.xy, position.z*-1., position.w);
   // gl_Position = gpu_perspective * vec4(position.xy, position.z*-1., position.w);
   // WARNING: This function is mostly the same except for some z-fighting shenanigans
   // gl_Position = perspective_from_frustum(position.xyz, fov, aspect);

   mat4 a = geometry_to_model[0];
   if (geometry_to_model.length() == 0) {
      // special = 1;
   }

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
   

in Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TexCoord;
};

in Flat {
   flat int special;
};

layout(location = 0) out vec4 FragColor; // Outputting to the Color Attachment 0 in the Framebuffer
layout(binding  = 3) uniform sampler2D diffuse_texture;
layout(binding  = 4) uniform sampler2D specular_texture;
layout(binding  = 5) uniform sampler2D emissive_texture;

uniform bool has_specular;
uniform bool has_emissive;
uniform bool is_light;
uniform vec3 camera_position;
uniform vec2 spherical;

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

vec3 brdf_blinn_phong(
      vec3 light_direction, vec3 view_direction, vec3 normal,
      vec3 diffuse_color,       vec3 specular_color,
      vec3 light_diffuse_color, vec3 light_specular_color, vec3 light_ambient_color,
      float specular_exponent
) {

   vec3 wi = normalize(light_direction);
   vec3 wo = normalize(view_direction);
   vec3 n  = normalize(normal);

   // vec3 ambient_color = diffuse_color * specular_color;
   vec3 ambient_color =  0.55160 * diffuse_color  + 0.082671 * specular_color;
   // vec3 ambient_color = vec3(0.212671*diffuse_color.r, 0.715160*diffuse_color.g, 0.072169*diffuse_color.b);


   // Table of materials and constants for ambient: http://devernay.free.fr/cours/opengl/materials.html
   float ambient_intesity  = 0.2 * (0.212671*ambient_color.r + 0.715160*ambient_color.g + 0.072169*ambient_color.b)/(0.1 + (0.212671*diffuse_color.r + 0.715160*diffuse_color.r + 0.072169*diffuse_color.r));
   float diffuse_intesity  = 0.5;
   float specular_intesity = 0.35;

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

   return  vec3(0.)
         + (diffuse_intesity  * diffuse)
         + (specular_intesity * specular)
         + (ambient_intesity  * ambient)
   ;
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

float light_attenuation(vec3 light_position, vec3 fragment_position) {
   // See to get some values: http://www.ogre3d.org/tikiwiki/tiki-index.php?page=-Point+Light+Attenuation
   const float Kc = 1.0;
   const float Kl = 0.007;
   const float Kq = 0.0002;
   const float min_attenuation = 0.00, max_attenuation = 1.0;

   float d = length(fragment_position - light_position);
   float denominator = Kc + Kl*d + Kq * pow(d, 2.);
   return clamp(1./denominator, min_attenuation, max_attenuation);
}


vec3 gamma_correction(vec3 colour) {
   float gamma = 1.0;
   return pow(colour, vec3(1. / gamma));
}


vec3 direction_light() {
   vec3 position = Position;
   vec3 normal   = normalize(Normal);
   vec3 light_direction = normalize(vec3(1., 1., 1.));
   vec3 view_direction  = normalize(per_frame.camera.position - position);

   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   // vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;
   vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;


   vec3 light_diffuse_color  = per_frame.light.diffuse;
   vec3 light_ambient_color  = per_frame.light.ambient;
   vec3 light_specular_color = per_frame.light.specular;

   if (has_specular) {
      specular_color  = texture(specular_texture, TexCoord).xyz;
   }

   vec3 color = brdf_blinn_phong(
      light_direction, view_direction, normal,
      diffuse_color, specular_color,
      light_diffuse_color, light_ambient_color, light_specular_color,
      32.
   );

   if (has_emissive && has_specular) {
      // color += (attenuation_distance * texture(emissive_texture, TexCoord).xyz);
      if ((specular_color.z + specular_color.y + specular_color.x) > 0.1) {
         const float time_factor = sin(per_frame.elapsed_time * 2.9)/2. + 0.5;
         // color += specular_color + time_factor * texture(emissive_texture, TexCoord).xyz;
         const vec3 emissive_color = texture(emissive_texture, TexCoord).xyz;
         color += specular_color * (emissive_color.y + emissive_color.x + emissive_color.z);
      }
   }
   return color;
}

vec3 spot_light_smooth() {
   vec3 position = Position;
   vec3 normal = normalize(Normal);

   // Flashlight properties
   vec3 light_position = per_frame.camera.position;
   vec3 light_direction = camera_forward(spherical); // Direction flashlight is pointing

   // Vector from fragment to light
   vec3 frag_to_light_direction = normalize(light_position - position);

   // Spotlight cone parameters
   float inner_cutoff = cos(radians(12.5)); // Inner cone angle (12.5 degrees)
   float outer_cutoff = cos(radians(17.5)); // Outer cone angle (17.5 degrees)
   float epsilon_cutoff = inner_cutoff - outer_cutoff;

   // Angle, but in cosine, between light direction and fragment direction
   float theta = dot(frag_to_light_direction, normalize(-light_direction));

   // Spotlight intensity with smooth falloff
   float intensity = clamp((theta - outer_cutoff) / epsilon_cutoff, 0.0, 1.0);

   // Early exit if outside spotlight cone
   if (theta < outer_cutoff) {
      return 0.1 * per_frame.light.ambient * texture(diffuse_texture, TexCoord).xyz;
   }

   vec3 view_direction = normalize(per_frame.camera.position - position);
   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   vec3 specular_color = vec3(0.8) + 0.2 * diffuse_color;

   if (has_specular) {
      specular_color = 1.0 * texture(specular_texture, TexCoord).xyz;
   }

   float attenuation = light_attenuation(light_position, position);
   vec3 color = attenuation * brdf_blinn_phong(
         light_direction, view_direction, normal,
         diffuse_color, specular_color,
         per_frame.light.diffuse, per_frame.light.ambient,
         has_specular ? vec3(1.0) : per_frame.light.specular,
         64.0
   );

   // Apply spotlight intensity
   color *= intensity;

   // Handle emissive materials
   if (has_emissive && has_specular) {
      vec3 emissive_color = texture(emissive_texture, TexCoord).xyz;
      color += specular_color * (emissive_color.y + emissive_color.x + emissive_color.z);
   }

   return color;
}

float spot_light(
   vec3 fragment_position,
   vec3 spotlight_position, vec3 spotlight_direction,
   float angle, float angle_increment
) {
   vec3 position = fragment_position;
   vec3 light_position = spotlight_position;

   // Light direction (from fragment to light)
   vec3 light_direction = normalize(light_position - position);

   // camera_forward point to the scene, so it's right where we're looking
   spotlight_direction = normalize(spotlight_direction);

   // Spotlight cone checking
   float inner_cutoff  = cos(radians(angle));
   float outer_cutoff  = cos(radians(angle + angle_increment));
   float theta         = dot(-spotlight_direction, light_direction);
   const float min_intensity = 0.1;
   const float max_intensity = 1.0;

   // Early exit for fragments outside spotlight
   if (theta < outer_cutoff) {
      return min_intensity;
   }

   // Smooth spotlight falloff
   float epsilon = inner_cutoff - outer_cutoff;
   float intensity = clamp((theta - outer_cutoff) / epsilon, min_intensity, max_intensity);
   // float intensity = smoothstep(0.0, 1.0, (theta - outer_cutoff) / epsilon);
   return intensity;

#if 0
   // Calculate lighting (replace with your BRDF)
   float ndotl = max(dot(normal, light_direction), 0.0);
   vec3 view_direction = normalize(per_frame.camera.position - position);

   // Basic Blinn-Phong example
   vec3 half_vector = normalize(light_direction + view_direction);
   float ndoth = max(dot(normal, half_vector), 0.0);
   float specular = pow(ndoth, 32.0);

   vec3 final_color = intensity * (diffuse_color * ndotl + vec3(specular) * 0.3);

   return final_color + 0.1 * per_frame.light.ambient * diffuse_color;
#endif

}

vec3 spot_light_hard() {
   vec3 position = Position;
   vec3 normal   = normalize(Normal);



   // Flashlight means the light position starts at the camera position
   vec3 light_position  = per_frame.camera.position;
   // light_position.y += 3.;

   vec3 pointing_direction = camera_forward(spherical);
   // vec3 light_direction    = normalize(light_position - position);
   vec3 light_direction    = pointing_direction;

   // Spotlight cone checking
   // float cutoff = cos(radians(12.5));
   float cutoff = cos(radians(12.5));
   vec3 light_to_frag = normalize(position - light_position);
   float theta = dot(pointing_direction, light_to_frag);
   // float theta = dot(normalize(-pointing_direction), normalize(light_position - position));

   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   // Remember that we're working with cosines of angles so '<' is used instead of more intuitive '>'
   if (theta < cutoff) {
      return 0.1 * per_frame.light.ambient * diffuse_color;
   } else {
      return 1.1 * per_frame.light.ambient * diffuse_color;
   }

   vec3 view_direction  = normalize(per_frame.camera.position - position);
   vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;

   vec3 light_diffuse_color  = per_frame.light.diffuse;
   vec3 light_ambient_color  = per_frame.light.ambient;
   vec3 light_specular_color = per_frame.light.specular;

   if (has_specular) {
      light_specular_color = vec3(1.0);
      specular_color  = 1.4*texture(specular_texture, TexCoord).xyz;
   }

   float attenuation = light_attenuation(light_position, position);
   vec3 color = attenuation * brdf_blinn_phong(
      light_direction, view_direction,
      normal,
      diffuse_color, specular_color,
      light_diffuse_color, light_ambient_color, light_specular_color,
      64.
   );

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
   if (is_light) {
      return light_ambient_color;
   }
   return color;
}

float point_light(vec3 light_position, vec3 fragment_positon) {
   float attenuation = light_attenuation(light_position, fragment_positon);
   return attenuation;
}


// Calculate color as if light is a point light but doesn't do any attenuation
vec3 calculate_color(Light light, vec3 light_direction, vec3 fragment_position, vec3 view_position, vec3 normal) {
   vec3 position = fragment_position;
   normal = normalize(normal);

   vec3 view_direction  = normalize(view_position - fragment_position);

   vec3 fragment_diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   vec3 fragment_specular_color = vec3(0.8) + 0.2*fragment_diffuse_color;

   vec3 light_diffuse_color  = light.diffuse;
   vec3 light_ambient_color  = light.ambient;
   vec3 light_specular_color = light.specular;

   const bool rainbow = false;
   if (rainbow) {
      light_diffuse_color  = vec3(sin(per_frame.elapsed_time*1.3)/2. + 1.0, sin(per_frame.elapsed_time*2)/4. + 0.5, sin(per_frame.elapsed_time*0.7)/4. + 0.5);
      light_ambient_color  = light_diffuse_color * vec3(0.2f);
      light_specular_color = vec3(0.92f);

   }

   if (has_specular) {
   // if (false && has_specular) {
      light_specular_color = vec3(1.0);
      fragment_specular_color = vec3(1.0);
      fragment_specular_color = texture(specular_texture, TexCoord).xyz;
   }

   float attenuation = light_attenuation(light.position, position);
   vec3 color = brdf_blinn_phong (
         light_direction, view_direction, normal,
         fragment_diffuse_color, fragment_specular_color,
         light_diffuse_color, light_ambient_color, light_specular_color,
         64.0
   );


   const bool test_elapsed_time = false;
   if (test_elapsed_time) {
      return vec3(sin(per_frame.elapsed_time), sin(per_frame.delta_time*1.2 +  PI/2.), sin(per_frame.elapsed_time*2.7 + PI/4.0));
   }


   if (has_emissive && has_specular) {
      // color += (attenuation_distance * texture(emissive_texture, TexCoord).xyz);
      if ((fragment_specular_color.z + fragment_specular_color.y + fragment_specular_color.x) > 0.1) {
         const float time_factor = sin(per_frame.elapsed_time * 2.9)/2. + 0.5;
         // color += specular_color + time_factor * texture(emissive_texture, TexCoord).xyz;
         const vec3 emissive_color = texture(emissive_texture, TexCoord).xyz;
         color += fragment_specular_color * (emissive_color.y + emissive_color.x + emissive_color.z);
      }
   }
   if (is_light) {
      return light_ambient_color;
   }
   return color;
}

void main() {
   // FragColor = vec4(gl_FragCoord.z);
   // return;
   // vec3 color = direction_light();
   // vec3 color = point_light();

   // vec3 color = spot_light_smooth();
   // vec3 color = spot_light();
   vec3 position = Position;

   vec3 camera_direction = camera_forward(spherical);
   vec3 camera_position = per_frame.camera.position;
   float intensity = spot_light(
      position,          // fragment_position
      camera_position,   // spotlight_position
      camera_direction,  // spotlight_direction,
      19.5, 12.0         // cutoff in degrees
   );


   // vec3 color = calculate_color(per_frame.light, position, per_frame.camera.position, Normal);
   vec3 color = vec3(0);

   {
      Light point_lights[3];
      // Initialize the struct members
      point_lights[0] = per_frame.light;

      point_lights[1].position = camera_position + vec3(0., 7., 0.);
      const bool pink_spotlight = false;
      if (pink_spotlight) {
         point_lights[1].ambient  = vec3(1.0, 0.09, 0.89);
         point_lights[1].diffuse  = vec3(1.0, 0.09, 0.89);
         point_lights[1].specular = vec3(1.0, 0.89, 1.0);
      } else {
         point_lights[1].specular = vec3(1.0);
         point_lights[1].ambient  = vec3(1.0);
         point_lights[1].diffuse  = vec3(1.0);
      }


      point_lights[2].position = vec3(0., 10., 0.);
      point_lights[2].ambient  = vec3(1.0, 0.89, 0.0);
      point_lights[2].diffuse  = vec3(1.0, 0.89, 0.0);
      point_lights[2].specular = vec3(1.0, 0.89, 0.0);

      for (int idx = 0; idx < point_lights.length(); idx += 1) {
         Light light = point_lights[idx];
         float attenuation = point_light(light.position, position);

         vec3 light_direction = normalize(light.position - position);
         if (idx == 1) {
            attenuation *= intensity;
         } else {
            light_direction = normalize(light.position - position);
         }

         color += attenuation * calculate_color(light, light_direction, position, camera_position, Normal);
         // color += calculate_color(light, light_direction, position, camera_position, Normal);
      }
   }

   if (special > 0) {
      color = vec3(1);
   }

   float distance_to_view  = length(position - vec3(per_frame.camera.position.x, 0., per_frame.camera.position.z)); // Ignoring height of view
   float attenuation_alpha = clamp(distance_to_view/distance_to_view, 0.2, 1.0);
   FragColor = vec4(color, attenuation_alpha);
   FragColor.xyz = gamma_correction(FragColor.xyz);
} 
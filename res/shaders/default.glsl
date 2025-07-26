#pragma vertex
#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;



#include "./buffers/uniform.glsl"
#include "./buffers/positions_xyz.glsl"
#include "./buffers/indices.glsl"

uniform mat4 view;
uniform mat4 model;
uniform mat4 perspective;
uniform bool is_light;

// uniform vec3 camera_position;
uniform vec2 spherical;
uniform float u_time;


out vec3 Normal;
out vec2 TexCoord;
out flat int special;

#include "./src/coordinates.glsl"
#include "./src/remaps.glsl"
#include "./src/perspective.glsl"
#include "./src/transformations.glsl"
#include "./src/view.glsl"


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


#include "./src/rotation.glsl"

vec3 pull_position(int id) {
   return vec3(
      positions_xyz[id*3 + 0],
      positions_xyz[id*3 + 1],
      positions_xyz[id*3 + 2]
   );

}
// #define PULLING

const float aspect = 1600./800.;
const float fov    = PI/3.;

void main() {
   special = 0;
#ifdef PULLING
   vec4 position = vec4(pull_position(gl_VertexID), 1.0);
#else
   vec4 position = vec4(position.xyz, 1.0);
#endif

   if (is_light) {
      // position = vec4(ub_data.light_position.xyz, 1.0);
   }

   float positions_count = positions_xyz.length();
   mat4 gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);
   vec3 translation = vec3(-36.55, -10.55, 50.50);
   float scale      = 12.3;

   if (is_light) {
      // scale = 20.3;
   }

   position = ub_data.model*position;


   if (length(position.xyz) < 10.) {
      special = 1;
      // position = matrix_translation(translation)*matrix_rotation(vec3(1.), PI/2.) * matrix_scale(vec3(scale))*model*position;
      // position =  * ub_data.model*position;
   }
   // position = ub_data.model * matrix_rotation(vec3(1.), PI/2.) * position;
   // position.xyz  *= scale;
   // position.xyz  += translation;
   // position.xyz  += translation/2.;

   {  // World to Camera
      // position.xz   *= rotation(spherical.x);
      // position.zy   *= rotation(spherical.y);
      // position.xzy  -= camera_position;
      // mat4 view = view_from_spherical(vec3(0., 0., 0.), 0, 0.5);

      vec3 eye = ub_data.camera_position*5;
      vec3 direction = vec3(0., 0., 1.);
      direction = spherical_to_cartesian(-spherical.y, spherical.x + PI/2);
      direction = camera_forward(spherical);
      // direction.xz *= rotation(sin(ub_data.elapsed_time));
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
   Normal   = normal;
}


#pragma fragment
#version 460 core

in vec3 Normal;
in vec2 TexCoord;
in flat int special;

layout(location = 0) out vec4 FragColor; // Outputting to the Color Attachment 0 in the Framebuffer

layout(binding = 4) uniform sampler2D tex;

uniform bool is_light;


#include "./buffers/uniform.glsl"
#include "./brdf/blinn-phong.glsl"

void main() {
   vec3 light_direction = normalize(vec3(2., 1., 1.));

   FragColor     = texture(tex, TexCoord);
   FragColor.w = 1.0;


   if (is_light) {
      // FragColor = vec4(light_color, 1.0);
      FragColor = vec4(ub_data.light_color, 1.0);
   }

   if (special == 1) {
      // FragColor = vec4(light_color, 1.0);
      FragColor.r = 1.0;
   }

   vec4 rand = ub_data_buffer.random_data;
   if (rand.x == 69.) {
      FragColor.g = 1.0;
   }

   if (rand.x == 68.) {
      FragColor.b = 1.0;
   }


#if 0
   FragColor = brdf_blinn_phong(light_direction, vec3 view_direction, vec3 normal, vec3 diffuse_color, vec3 specular_color, float alpha);
#endif
}

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

uniform vec3 camera_position;
uniform vec2 spherical;
uniform float u_time;


out vec3 Position;
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
}


#pragma fragment
#version 460 core

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


#include "./buffers/uniform.glsl"
#include "./brdf/blinn-phong.glsl"


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
}

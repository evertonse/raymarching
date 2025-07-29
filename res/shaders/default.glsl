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


out vec3 Position;
out vec3 Normal;
out vec2 TexCoord;
out flat int special;


#include "./src/coordinates.glsl"
#include "./src/remaps.glsl"
#include "./src/perspective.glsl"
#include "./src/transformations.glsl"
#include "./src/view.glsl"
#include "./src/camera.glsl"




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
// const float fov    = PI/4;

void main() {
   special = 0;

#ifdef PULLING
   vec4 position = vec4(pull_position(gl_VertexID), 1.0);
#else
   vec4 position = vec4(position.xyz + vec3(10), 1.0);
#endif

   float positions_count = positions_xyz.length();
   mat4  gpu_perspective = perspective_from_fov(fov, aspect, 0.1, 100.);

   // World position send to next stage
   position.xz *= rotation(per_frame.elapsed_time * 0.2);
   position = per_frame.model*position;

   { // Send to next shader
      // Everything is sent in World Space
      Position = position.xyz;
      // See more about the normal matrix: http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
      if (true) {
         // Apply mat3 to "drop" the translation portion
         Normal = mat3(transpose(inverse(per_frame.model))) * normal;
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

   TexCoord = uv;
}

///////////////////////////////////////////////////////////////////////////////////////
//...................................................................................//
//...................................................................................//
//...................................................................................//
///////////////////////////////////////////////////////////////////////////////////////

#pragma fragment
#version 460 core

in vec3 Position;
in vec3 Normal;
in vec2 TexCoord;
in flat int special;

layout(location = 0) out vec4 FragColor; // Outputting to the Color Attachment 0 in the Framebuffer
layout(binding  = 3) uniform sampler2D diffuse_texture;
layout(binding  = 4) uniform sampler2D specular_texture;
layout(binding  = 5) uniform sampler2D emissive_texture;

uniform bool has_specular;
uniform bool has_emissive;
uniform bool is_light;
uniform vec3 camera_position;
uniform vec2 spherical;


#include "./src/camera.glsl"
#include "./buffers/uniform.glsl"
#include "./brdf/blinn-phong.glsl"

float light_attenuation(vec3 light_position, vec3 fragment_position) {
   // See to get some values: http://www.ogre3d.org/tikiwiki/tiki-index.php?page=-Point+Light+Attenuation
   const float Kc = 1.0;
   const float Kl = 0.007;
   const float Kq = 0.0002;
   const float min_attenuation = 0.07, max_attenuation = 1.0;

   float d = length(fragment_position - light_position);
   float denominator = Kc + Kl*d + Kq * pow(d, 2.);
   return clamp(1./denominator, min_attenuation, max_attenuation);
}


vec3 gamma_correction(vec3 colour) {
   float gamma = 1.0;
   return pow(colour, vec3(1. / gamma));

}

vec3 point_light() {
   vec3 position = Position;
   vec3 normal   = normalize(Normal);


   vec3 light_direction = normalize(per_frame.light.position - position);
   vec3 view_direction  = normalize(per_frame.camera.position - position);

   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;


   if (special == 1) {
      FragColor.r = 1.0;
   }

   vec3 light_diffuse_color  = per_frame.light.diffuse;
   vec3 light_ambient_color  = per_frame.light.ambient;
   vec3 light_specular_color = per_frame.light.specular;
   const bool rain_bow_light = true;

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

   float attenuation = light_attenuation(per_frame.light.position, position);
   vec3 color =
      brdf_blinn_phong(
         light_direction, view_direction, normal,
         diffuse_color, specular_color,
         light_diffuse_color, light_ambient_color, light_specular_color,
         64., attenuation
      );

   const bool test_elapsed_time = false;
   if (test_elapsed_time) {
      return vec3(sin(per_frame.elapsed_time), per_frame.delta_time*100, .0);
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
   if (is_light) {
      return light_ambient_color;
   }
   return color;
}

vec3 direction_light() {
   vec3 position = Position;
   vec3 normal   = normalize(Normal);
   vec3 light_direction = normalize(vec3(1., 1., 1.));
   vec3 view_direction  = normalize(per_frame.camera.position - position);

   vec3 diffuse_color  = texture(diffuse_texture, TexCoord).xyz;
   vec3 specular_color = vec3(0.8) + 0.2*diffuse_color;


   vec3 light_diffuse_color  = per_frame.light.diffuse;
   vec3 light_ambient_color  = per_frame.light.ambient;
   vec3 light_specular_color = per_frame.light.specular;

   if (has_specular) {
      specular_color = vec3(1.0);
      light_specular_color = vec3(1.0);
      specular_color  = texture(specular_texture, TexCoord).xyz;
   }

   vec3 color = brdf_blinn_phong(
      light_direction, view_direction, normal,
      diffuse_color, specular_color,
      light_diffuse_color, light_ambient_color, light_specular_color,
      64., 1.0
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
   float epsilon = inner_cutoff - outer_cutoff;

   // Angle, but in cosine, between light direction and fragment direction
   float theta = dot(frag_to_light_direction, normalize(-light_direction));

   // Spotlight intensity with smooth falloff
   float intensity = clamp((theta - outer_cutoff) / epsilon, 0.0, 1.0);

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
   vec3 color = brdf_blinn_phong(
         light_direction, view_direction, normal,
         diffuse_color, specular_color,
         per_frame.light.diffuse, per_frame.light.ambient,
         has_specular ? vec3(1.0) : per_frame.light.specular,
         64.0, attenuation
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

vec3 spot_light() {
   vec3 position = Position;
   vec3 normal = normalize(Normal);
   vec3 light_position = per_frame.camera.position;

   // Fixed: Calculate light direction (from fragment to light)
   vec3 light_direction = normalize(light_position - position);

   // Fixed: Use light direction for spotlight cone check
   vec3 spotlight_direction = normalize(camera_forward(spherical));

   // Spotlight cone checking
   float cutoff = cos(radians(12.5));
   float outer_cutoff = cos(radians(15.0));

   // Fixed: Dot product between spotlight direction and light direction
   float theta = dot(-spotlight_direction, light_direction);

   vec3 diffuse_color = texture(diffuse_texture, TexCoord).xyz;

   // Early exit for fragments outside spotlight
   if (theta < outer_cutoff) {
      return 0.1 * per_frame.light.ambient * diffuse_color;
   }

   // Smooth spotlight falloff
   float epsilon = cutoff - outer_cutoff;
   float intensity = clamp((theta - outer_cutoff) / epsilon, 0.0, 1.0);

   // Calculate lighting (replace with your BRDF)
   float ndotl = max(dot(normal, light_direction), 0.0);
   vec3 view_direction = normalize(per_frame.camera.position - position);

   // Basic Blinn-Phong example
   vec3 half_vector = normalize(light_direction + view_direction);
   float ndoth = max(dot(normal, half_vector), 0.0);
   float specular = pow(ndoth, 32.0);

   vec3 final_color = intensity * (diffuse_color * ndotl + vec3(specular) * 0.3);

   return final_color + 0.1 * per_frame.light.ambient * diffuse_color;
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
   vec3 color = brdf_blinn_phong(
      light_direction, view_direction,
      normal,
      diffuse_color, specular_color,
      light_diffuse_color, light_ambient_color, light_specular_color,
      64., attenuation
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

void main() {
   // vec3 color = direction_light();
   // vec3 color = point_light();

   vec3 color = spot_light_smooth();
   // vec3 color = spot_light();
   // vec3 color = spot_light_hard();

   vec3 position = Position;
   float distance_to_view  = length(position - vec3(per_frame.camera.position.x, 0., per_frame.camera.position.z)); // Ignoring height of view
   float attenuation_alpha = clamp(distance_to_view/distance_to_view, 0.2, 1.0);
   FragColor = vec4(color, 1.0);
   FragColor.xyz = gamma_correction(FragColor.xyz);
}

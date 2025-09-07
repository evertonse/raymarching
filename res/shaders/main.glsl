#pragma vertex
#version 460 core

// NOTE: Not doing vertex pulling is incorrect right now. Vertex Pulling ONLY
// layout(location = 0) in vec3 position;
// layout(location = 1) in vec3 normal;
// layout(location = 2) in vec2 uv;
#include "src/renderer/shared/types.glsl"  // DrawCommand is defined here.
#include "src/renderer/shared/defines.glsl"
#include "./src/buffers.glsl"


uniform mat4 view;
uniform mat4 model;
uniform mat4 perspective;
uniform bool is_light;
uniform int  has_animation = -1;

uniform vec3 camera_position;
uniform vec2 spherical;

layout (location = 0) out Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TextureCoordinate;
};

layout (location = 4) out Flat {
   flat uint material_index;
};

#include "./src/coordinates.glsl"
#include "./src/remaps.glsl"
#include "./src/perspective.glsl"
#include "./src/transformations.glsl"
#include "./src/view.glsl"
#include "./src/camera.glsl"
#include "./src/rotation.glsl"


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
   const int num_vertices = vertex_buffer.length() / 8;  // Total vertices
   const int normal_offset = num_vertices * 3;           // Offset to normals section
   return vec3(
      vertex_buffer[id*3 + 0 + normal_offset],
      vertex_buffer[id*3 + 1 + normal_offset],
      vertex_buffer[id*3 + 2 + normal_offset]
   );
}

vec2 pull_uv(int id) {
   const int num_vertices = vertex_buffer.length() / 8;  // Total vertices
   const int uv_offset = num_vertices * 6;               // Offset to UV section (after positions + normals)
   return vec2(
       vertex_buffer[id*2 + 0 + uv_offset],
       vertex_buffer[id*2 + 1 + uv_offset]
   );
}
#endif

// const float aspect = 1600./800.;
// const float aspect = 2560./1080.;
float aspect = 1152./ 486.;


const float fov    = PI/3.;
const float near_plane = 0.005;
const float far_plane = 256.000000;

void main() {
   material_index = -1;
   aspect = per_frame.camera.aspect;

#ifdef PULLING
   vec4 position = vec4(pull_position(gl_VertexID), 1.0);
   vec3 normal   = pull_normal(gl_VertexID);
   vec2 uv       = pull_uv(gl_VertexID);
   Draw_Command draw_command = draw_commands[gl_DrawID];
   material_index = int(draw_command.material_index);
#else
   vec4 position = vec4(position.xyz,  1.0);
#endif

   mat4 gpu_perspective = perspective_from_fov(fov, per_frame.camera.aspect, 0.1, 100.);
   mat4 model = instances[gl_BaseInstance + gl_InstanceID].model_matrix;

   if (draw_command.has_joints == 1) {
      uint geometry_to_model_offset = instances[gl_BaseInstance + gl_InstanceID].geometry_to_model_offset;

      ivec4 joint_indices = joint_vertices[draw_command.joints_offset + gl_VertexID - gl_BaseVertex].joint_indices;
      vec4  joint_weights = joint_vertices[draw_command.joints_offset + gl_VertexID - gl_BaseVertex].joint_weights;
      if (false) {
         position =
              joint_weights[0] * (geometry_to_model[geometry_to_model_offset + joint_indices[0]] * position)
            + joint_weights[1] * (geometry_to_model[geometry_to_model_offset + joint_indices[1]] * position)
            + joint_weights[2] * (geometry_to_model[geometry_to_model_offset + joint_indices[2]] * position)
            + joint_weights[3] * (geometry_to_model[geometry_to_model_offset + joint_indices[3]] * position)
         ;
      } else {
         mat4 joint_transform =
              joint_weights[0] * geometry_to_model[geometry_to_model_offset + joint_indices[0]]
            + joint_weights[1] * geometry_to_model[geometry_to_model_offset + joint_indices[1]]
            + joint_weights[2] * geometry_to_model[geometry_to_model_offset + joint_indices[2]]
            + joint_weights[3] * geometry_to_model[geometry_to_model_offset + joint_indices[3]]
         ;
         model = model * joint_transform;
      }



   }

   position = model * position;

   {  // Send to next shader
      // Everything is sent in World Space
      Position = position.xyz;
      // See more about the normal matrix: http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
      if (true) {
         // Apply mat3 to "drop" the translation portion
         Normal = mat3(transpose(inverse(model))) * normal;
      } else {
         Normal = normal.xyz;
      }
      TextureCoordinate = uv;
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


   { // Camera to Clip
      gl_Position = perspective_from_fov(position.xyz, fov, aspect, near_plane, far_plane); // Appears to be infinite in depth
   }
   // gl_Position = per_frame.perspective * vec4(position.xy, position.z*-1., position.w);
   // gl_Position = gpu_perspective * vec4(position.xy, position.z*-1., position.w);
   // WARNING: This function is mostly the same except for some z-fighting shenanigans
   // gl_Position = perspective_from_frustum(position.xyz, fov, aspect);
}


///////////////////////////////////////////////////////////////////////////////////////
//...................................................................................//
//...................................................................................//
//...................................................................................//
///////////////////////////////////////////////////////////////////////////////////////

#pragma fragment
#version 460 core
#extension GL_ARB_bindless_texture : enable

// Beware that Early-Z is disabled if your fragment shader does any of:
// discard / alpha test behavior
// alpha blending enabled
// writes gl_FragDepth
// side effects (imageStore/atomics in FS)
// In that case, instancing multiplies your fragment cost N times.

// TODO: Match these by location as well
layout (location = 0) in Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TextureCoordinate;
};

layout (location = 4) in Flat {
   flat uint material_index;
};

layout(location = 0) out vec4 FragColor; // Outputting to the Color Attachment 0 in the Framebuffer
layout(binding  = 3) uniform sampler2D diffuse_texture;
layout(binding  = 4) uniform sampler2D specular_texture;
layout(binding  = 5) uniform sampler2D emissive_texture;

bool has_specular = false;
bool has_emissive = false;
uniform bool is_light;
uniform vec3 camera_position;
uniform vec2 spherical;
uniform int is_special = 0;


#include "src/renderer/shared/types.glsl"  // DrawCommand is defined here.
#include "src/renderer/shared/defines.glsl"
#include "./src/buffers.glsl"

#include "./src/coordinates.glsl"
#include "./src/view.glsl"
#include "./src/camera.glsl"
#include "./brdf/blinn-phong.glsl"

float point_light_light_attenuation(vec3 light_position, vec3 fragment_position) {
   // See to get some values: http://www.ogre3d.org/tikiwiki/tiki-index.php?page=-Point+Light+Attenuation
   const float Kc = 1.0;
   const float Kl = 0.007;
   const float Kq = 0.0002;
   const float min_attenuation = 0.00, max_attenuation = 1.0;

   float d = length(fragment_position - light_position);
   float denominator = Kc + Kl*d + Kq * pow(d, 2.);
   return clamp(1./denominator, min_attenuation, max_attenuation);
}

vec3 gamma_correct(vec3 colour) {
   const float gamma = 2.2;
   return pow(colour, vec3(1. / gamma));
}

vec3 gamma_correct_texture(vec3 colour) {
   const float gamma = 2.2;
   return pow(colour, vec3(gamma));
}

vec3 apply_contrast(vec3 colour, float contrast) {
   return (colour - 0.5) * contrast + 0.5;
}

vec3 tonemap_filmic_backend(vec3 x) {
   // Constants from Hable's "Filmic Tonemapping Operators" talk
   // https://www.slideshare.net/slideshow/hable-john-uncharted2-hdr-lighting/3602588
   // const float A = 0.15;
   // const float B = 0.50;
   // const float C = 0.10;
   // const float D = 0.20;
   // const float E = 0.02;
   // const float F = 0.30;

   const float A = 0.22; // Shoulder Strength
   const float B = 0.30; // Linear Strength
   const float C = 0.10; // Linear Angle
   const float D = 0.20; // Toe Strength
   const float E = 0.01; // Toe Numberator
   const float F = 0.30; // Toe Denominator
   return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 tonemap_filmic(vec3 color, float exposure) {
   // Exposure bias tweak
   color = tonemap_filmic_backend(color * exposure);
   // white point (11.2 the default value)
   float white_scale = 1.0 / tonemap_filmic_backend(vec3(7.2)).r;
   return color * white_scale;
}

vec3 tonemap_aces(const vec3 x) { // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;
   return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec3 tonemap_reinhard(const vec3 x) {
   // reinhard tone mapping
   return x / (x + vec3(1.0));
}

vec3 tonemap_reinhard(const vec3 hdr_color, float exposure) {
   vec3 mapped = vec3(1.0) - exp(-hdr_color * exposure);
   return mapped;
}

// ------------------------------------------------------------
// Uncharted 2 Filmic Tonemap
// ------------------------------------------------------------
vec3 tonemap_uncharted(vec3 x) {
   float A = 0.15;
   float B = 0.50;
   float C = 0.10;
   float D = 0.20;
   float E = 0.02;
   float F = 0.30;
   float W = 11.2; // white scale

   x = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
   float white_scale = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
   return x / white_scale;
}

// ------------------------------------------------------------
// ACES Tonemap (Unity style)
// ------------------------------------------------------------
vec3 tonemap_aces_unity(vec3 x) {
   const mat3 aces_input_matrix = mat3(
      0.59719, 0.35458, 0.04823,
      0.07600, 0.90834, 0.01566,
      0.02840, 0.13383, 0.83777
   );

   const mat3 aces_output_matrix = mat3(
      1.60475, -0.53108, -0.07367,
      -0.10208,  1.10813, -0.00605,
      -0.00327, -0.07276,  1.07602
   );

   x = aces_input_matrix * x;

   x = (x * (x + 0.0245786) - 0.000090537) /
       (x * (0.983729 * x + 0.4329510) + 0.238081);

   x = aces_output_matrix * x;
   return clamp(x, 0.0, 1.0);
}

// ------------------------------------------------------------
// ACES Tonemap (Unreal Engine style)
// ------------------------------------------------------------
vec3 tonemap_aces_unreal(vec3 x) {
   // Source: Unreal Engine 4 ACES implementation
   x *= 0.6; // exposure bias
   const float a = 2.51;
   const float b = 0.03;
   const float c = 2.43;
   const float d = 0.59;
   const float e = 0.14;

   return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}


#ifdef FIX
vec3 spot_light_smooth(vec3 frag_to_light_direction) {
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

   if (theta < outer_cutoff) {
      return 0.1;
   }

   return intensity;
}
#endif

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

}

float point_light(vec3 light_position, vec3 fragment_positon) {
   float attenuation = point_light_light_attenuation(light_position, fragment_positon);
   return attenuation;
}
struct Fragment {
   vec3 diffuse_color;
   vec3 specular_color;
   vec3 emissive_color;
   vec3 position;
   vec3 normal;
};


// Calculate color as if light is a point light but doesn't do any attenuation
vec3 calculate_color(
      in Light light, in vec3 light_direction,
      in Fragment fragment, in vec3 view_position) {

   vec3 position = fragment.position;
   vec3 normal = normalize(fragment.normal);

   vec3 view_direction  = normalize(view_position - fragment.position);

   vec3 fragment_diffuse_color  = fragment.diffuse_color;
   // vec3 fragment_diffuse_color  = vec3(1);
   // vec3 fragment_specular_color = vec3(0.7) + 0.2*fragment_diffuse_color;
   vec3 fragment_specular_color = fragment.specular_color;

   vec3 light_diffuse_color  = light.diffuse;
   vec3 light_ambient_color  = light.ambient;
   vec3 light_specular_color = light.specular;

   const bool rainbow = false;
   if (rainbow) {
      light_diffuse_color  = vec3(sin(per_frame.elapsed_time*1.3)/2. + 1.0, sin(per_frame.elapsed_time*2)/4. + 0.5, sin(per_frame.elapsed_time*0.7)/4. + 0.5);
      light_ambient_color  = light_diffuse_color * vec3(0.2f);
      light_specular_color = vec3(0.92f);

   }

   float attenuation = point_light_light_attenuation(light.position, position);
   vec3 color = brdf_blinn_phong(
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
      // color += (attenuation_distance * texture(emissive_texture, TextureCoordinate).xyz);
      if ((fragment_specular_color.z + fragment_specular_color.y + fragment_specular_color.x) > 0.1) {
         const float time_factor = sin(per_frame.elapsed_time * 2.9)/2. + 0.5;
         // color += specular_color + time_factor * texture(emissive_texture, TextureCoordinate).xyz;
         const vec3 emissive_color = texture(emissive_texture, TextureCoordinate).xyz;
         color += fragment_specular_color * (emissive_color.y + emissive_color.x + emissive_color.z);
      }
   }
   if (is_light) {
      return light_ambient_color;
   }
   return color;
}

#define return_white FragColor.xyzw = vec4(1.); return

void main() {
   // vec3 color = vec3(gl_FragCoord.z);

   vec3 color = vec3(0.);
   vec3 diffuse_color  = vec3(1.);
   vec3 specular_color = vec3(0.);
   vec3 emissive_color = vec3(0.);
   float alpha_channel = 1.0;

   // return_white;


   Material material = materials[material_index];
   if (material.diffuse_handle != uvec2(0)) {
      // TODO: Mode gamma_correction to after sbti loading
      vec4 dtexture = vec4(1.);
      if (material.normal_handle != uvec2(0)) {
         dtexture = texture(sampler2D(material.normal_handle), TextureCoordinate*2.);
      } else {
         dtexture = texture(sampler2D(material.diffuse_handle), TextureCoordinate);
      }

      // dtexture = texture(diffuse_texture, TextureCoordinate);
      diffuse_color = dtexture.xyz;
      diffuse_color = gamma_correct_texture(diffuse_color);
      alpha_channel = dtexture.w;

   }

   if (alpha_channel < 0.0) {
      discard;
   }

   if (material.specular_handle != uvec2(0)) {
      vec4 stexture   = texture(sampler2D(material.specular_handle), TextureCoordinate);
      specular_color = stexture.xyz;
      has_specular = true;
   }


   Fragment fragment;
   fragment.position = Position;
   fragment.normal   = Normal;
   fragment.diffuse_color  = diffuse_color;
   fragment.specular_color = specular_color;
   fragment.emissive_color = emissive_color;

   FragColor.xyzw = vec4(color, 1.0);
   FragColor.xyz = fragment.diffuse_color;


   vec3 position = Position;

   vec3 camera_direction = camera_forward(spherical);
   vec3 camera_position = per_frame.camera.position;
   float intensity = spot_light(
      fragment.position,      // fragment_position
      camera_position,        // spotlight_position
      camera_direction,       // spotlight_direction,
      19.5, 12.0              // cutoff in degrees
   );



   {
      Light point_lights[3];
      // Initialize the struct members
      point_lights[0] = per_frame.light;
      point_lights[0].diffuse = vec3(2.);

      point_lights[1].position = camera_position + vec3(0., 7., 0.);
      const bool pink_spotlight = false;
      const float dim_factor = 1.1;

      point_lights[0].ambient  *= 1/dim_factor;
      point_lights[0].diffuse  *= 1/dim_factor;
      point_lights[0].specular *= 1/dim_factor;

      if (pink_spotlight) {
         point_lights[1].ambient  = vec3(1.0, 0.09, 0.89)/dim_factor;
         point_lights[1].diffuse  = vec3(1.0, 0.09, 0.89)/dim_factor;
         point_lights[1].specular = vec3(1.0, 0.89, 1.0) /dim_factor;
      } else {
         point_lights[1].specular = vec3(1.0)/dim_factor;
         point_lights[1].ambient  = vec3(1.0)/dim_factor;
         point_lights[1].diffuse  = vec3(1.0)/dim_factor;
      }


      point_lights[2].position = vec3(0., 10., 0.);
      point_lights[2].ambient  = vec3(2.0, 0.89, 1.0)/dim_factor;
      point_lights[2].diffuse  = vec3(2.0, 0.89, 1.0)/dim_factor;
      point_lights[2].specular = vec3(2.0, 0.89, 1.0)/dim_factor;

      for (int idx = 0; idx < point_lights.length(); idx += 1) {
         Light light = point_lights[idx];
         float attenuation = point_light(light.position, position);

         vec3 light_direction = normalize(light.position - position);
         if (idx == 1) {
            attenuation *= intensity;
         } else if (idx == 1) {
            light_direction = vec3(1.);
         }

         // color += attenuation * calculate_color(light, light_direction, fragment, camera_position, Normal);
         color += attenuation * calculate_color(light, light_direction, fragment, Normal);
         // color += calculate_color(light, light_direction, position, camera_position, Normal);
      }
   }

   float distance_to_view  = length(position - vec3(per_frame.camera.position.x, 0., per_frame.camera.position.z)); // Ignoring height of view
   float attenuation_alpha = clamp(distance_to_view/distance_to_view, 0.2, 1.0);
   FragColor = vec4(color, attenuation_alpha);

   // Gamma correction should come later?
   // FragColor.xyz = tonemap_aces(FragColor.xyz);
   // FragColor.xyz = tonemap_aces_unity(FragColor.xyz);
   FragColor.xyz = tonemap_aces_unreal(FragColor.xyz);


   // FragColor.xyz = tonemap_filmic(FragColor.xyz, 1.0);
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz);
   // const float exposure = 0.8;
   // FragColor.xyz = tonemap_reinhard(FragColor.xyz, exposure);

   FragColor.xyz = gamma_correct(FragColor.xyz);
   // FragColor.xyz = apply_contrast(FragColor.xyz, 1.079);
   FragColor.w *= max(alpha_channel, 0.1);

}

#pragma fragment
#version 460 core
#extension GL_ARB_bindless_texture : enable

#include "res/shaders/common.glsl"


// TODO: Match these by location as well
layout (location = 0) in #include "./pipe.glsl";




layout(location = FRAMEBUFFER_ATTACHMENTH_COLOR)    out vec4 color_attachment; // Outputting to the Color Attachment 0 in the Framebuffer
layout(location = FRAMEBUFFER_ATTACHMENTH_NORMAL)   out vec4 normal_attachment;
layout(location = FRAMEBUFFER_ATTACHMENTH_POSITION) out vec4 position_attachment;

layout(binding = 3) uniform sampler2D diffuse_texture;
layout(binding = 4) uniform sampler2D specular_texture;
layout(binding = 5) uniform sampler2D emissive_texture;
layout(binding = 6) uniform sampler2D height_map;
layout(binding = 7) uniform sampler2D height_max_mipmap;

bool has_specular = false;
bool has_emissive = false;
vec2 spherical;

vec3 camera_position;


#include "src/renderer/shared/types.glsl"  // DrawCommand is defined here.
#include "src/renderer/shared/defines.glsl"
#include "res/shaders/src/buffers.glsl"

#include "res/shaders/src/coordinates.glsl"
#include "res/shaders/src/camera.glsl"
#include "res/shaders/src/tbn.glsl"

#include "res/shaders/src/parallax.glsl"


void main() {

   position_attachment.rgb = vec3(
      remap(ViewSpacePosition.z, near_plane, far_plane, 0.0, 1.0)
   );
   position_attachment = vec4(ViewSpacePosition, 1.);
   // Visualizing Camera Space normal should change when camera moves direction https://discussions.unity.com/t/view-space-normals-affected-by-camera-rotation/661176
   normal_attachment = vec4(normalize(ViewSpaceNormal), 1.);

   spherical = vec2(per_frame.camera.phi, per_frame.camera.theta);
   position_attachment.rgb = ViewSpacePosition;
   vec3 color = vec3(0.);
   vec3 diffuse_color  = vec3(1.);
   vec3 specular_color = vec3(0.);
   vec3 emissive_color = vec3(0.);
   float alpha_channel = 1.0;
   vec3 normal = ViewSpaceNormal;
   vec3 position = ViewSpacePosition;
   float shadow = 1.0;

   vec3 camera_direction = camera_forward(spherical);
   vec3 camera_position = per_frame.camera.position;
   vec2 uv = TextureCoordinate;


   if (true && !gl_FrontFacing) {
      // NOTE: Shading rn is strange on Alleya model because we get back facing triangles poping up due to animation 
      normal = -normal; // flip normals on backfaces for double sided materials
   }

   Material material = materials[material_index];
   if (material.diffuse_handle != uvec2(0)) {
      // TODO: Mode gamma_correction to after sbti loading
      vec4 dtexture = vec4(1.);
      dtexture = texture(sampler2D(material.diffuse_handle), uv);
      diffuse_color = dtexture.xyz;
      diffuse_color = gamma_correct_texture(diffuse_color);
      alpha_channel = dtexture.w;
   }

   if (true && material.normal_handle != uvec2(0)) {
      const float uv_factor = 1.;
      // const float uv_factor = 5.;
      // const float uv_factor = 7.;

      const vec2 uv = uv*uv_factor;
      {
         // vec4 dtexture = texture(sampler2D(material.diffuse_handle), uv);
         // diffuse_color = dtexture.xyz;
         // diffuse_color = gamma_correct_texture(diffuse_color);
         // diffuse_color = vec3(1.);
      }
      vec4 normal_texel = texture(sampler2D(material.normal_handle), uv);
   }
   // TODO: What exactly do we need from color to check in
   color_attachment.xyzw = vec4(diffuse_color, alpha_channel);
   normal_attachment.rgb = ViewSpaceNormal; // Maybe we should get from Tagent Space normal map then back?
}

#pragma fragment
#version 460 core
#extension GL_ARB_bindless_texture : enable

#include "res/shaders/common.glsl"


// TODO: Match these by location as well
layout(location = 0) in #include "./src/pipe.glsl" in_vertex;


layout(location = FRAMEBUFFER_ATTACHMENTH_COLOR)    out vec4 color_attachment; // Outputting to the Color Attachment 0 in the Framebuffer
layout(binding = BINDING_AMBIENT_OCCLUSION_TEXTURE) uniform sampler2D ambient_occlusion_texture;


bool has_specular = false;
bool has_emissive = false;
vec2 spherical;
uniform bool is_light;
uniform vec3 camera_position;
uniform int is_special = 0;
vec4 visibility = vec4(0,0,0, 1.);


#include "src/renderer/shared/types.glsl"  // DrawCommand is defined here.
#include "src/renderer/shared/defines.glsl"
#include "./src/buffers.glsl"

#include "./src/coordinates.glsl"
#include "./src/camera.glsl"
#include "./brdf/blinn-phong.glsl"

float point_light_attenuation(vec3 light_position, vec3 fragment_position, float range) {
   // For now Idk what do to about 'range'
   // See to get some values: http://www.ogre3d.org/tikiwiki/tiki-index.php?page=-Point+Light+Attenuation
   const float Kc = 1.0;
   const float Kl = 0.007;
   const float Kq = 0.0002;
   const float min_attenuation = 0.00, max_attenuation = 1.0;

   float d = length(fragment_position - light_position);
   float denominator = Kc + Kl*d + Kq * pow(d, 2.);
   return clamp(1./denominator, min_attenuation, max_attenuation);
}

Light lights[5];
void init_lights(vec3 camera_position, vec3 camera_direction) {
   const bool pink_spotlight = false;
   const float dim_factor = 1.0;

   // Global point light taken from per_frame.light and boosted) ---
   lights[0].type = LIGHT_TYPE_POINT;
   lights[0].position = vec3(0);
   lights[0].color = vec4(1.);
   lights[0].range = 100.0;
   lights[0].casts_shadows = 1;
   lights[0].radius = 0.01; // hard shadows

   // Light 1 Spotlight attached to camera ---
   lights[1].type = LIGHT_TYPE_SPOT;
   lights[1].forward = camera_direction;
   lights[1].position = camera_position + vec3(0.0, 7.0, 0.0);
   lights[1].color = vec4(vec3(15), true ? 0:1.5); // pink_spotlight is false
   lights[1].range = 150.0;
   lights[1].cone_angle = radians(1.5);
   lights[1].cone_angle_increment = radians(15.0);
   lights[1].casts_shadows = 1;
   lights[1].radius = 0.05;

   // Light 2: Overhead pink point light
   lights[2].type = LIGHT_TYPE_POINT;
   lights[2].position = vec3(0.0, 10.0, 0.0);
   lights[2].color = vec4(vec3(2.0, 0.89, 1.0) / dim_factor, 1.0);
   lights[2].range = 80.0;
   lights[2].casts_shadows = 1;
   lights[2].radius = 0.1;

   // Sinusoidal
   lights[3].type = LIGHT_TYPE_POINT;
   lights[3].position = vec3(100.0 - (100.0 + 300.0) * ((sin(per_frame.elapsed_time) + 1.0) / 2.0), 50.0, 0.0);
   lights[3].color = vec4(2.0, 0.5, 2.0, 1.2); // diffuse was 2.0, ambient 1.0 → take dominant
   lights[3].range = 120.0;
   lights[3].casts_shadows = 1;
   lights[3].radius = 0.2;

   // Directional light
   lights[4].type = LIGHT_TYPE_DIRECTIONAL;
   lights[4].forward = normalize(-vec3(1.0, -1.0, 0.0)); // already a direction
   lights[4].color = vec4(2.9, 2.9, 2.9, 1.0);            // uniform colour
   lights[4].range = 0.0;                                 // not used for directional
   lights[4].cone_angle = 0.0;
   lights[4].cone_angle_increment = 0.0;
   lights[4].casts_shadows = 1;
   lights[4].radius = 0.0;
}

vec3 apply_contrast(vec3 colour, float contrast) {
   return (colour - 0.5) * contrast + 0.5;
}

float spot_light(
   vec3  fragment_position,
   vec3  light_position,
   vec3  light_forward,
   float cone_angle_radians, float cone_angle_increment_radians
) {
   const float cos_inner = cos(cone_angle_radians);
   const float cos_outer = cos(cone_angle_radians + cone_angle_increment_radians);

   vec3  to_fragment = normalize(fragment_position - light_position);
   float cos_theta   = dot(to_fragment, normalize(light_forward));

   float t = saturate((cos_theta - cos_outer) / (cos_inner - cos_outer));  // [0,1]
   float intensity = t;
   if (false) {
      intensity = pow(t, 2.);
      intensity = log(t);
   }
   intensity = smoothstep(0.0, 1.0, t);
   return intensity;
}

struct Fragment {
   vec3 diffuse_color;
   vec3 specular_color;
   vec3 emissive_color;
   vec3 position;
   vec3 normal;
};


vec3 calculate_color(
   in const Fragment fragment,
   in const vec3 light_position, in const vec3 light_direction, in const vec3 light_color,
   in const vec3 view_position
) {

   vec3 view_direction  = normalize(view_position - fragment.position);

   vec3 color = brdf_blinn_phong(
      light_direction,            // in vec3  light_direction,
      view_direction,             // in vec3  view_direction,
      normalize(fragment.normal), // in vec3  normal,
      fragment.diffuse_color,     // in vec3  diffuse_color,
      fragment.specular_color,    // in vec3  specular_color,
      light_color,                // in vec3  light_color,
      16,                         // in float specular_exponent,
      visibility.w,               // in float ambient_occlusion_factor,   // 0=fully occluded, 1=fully open
      visibility.rgb,             // in vec3  indirect_irradiance,        // irradiance from SSDO, no albedo applied
      false                       // in bool  direct_only
   );

   return color;
}


#include "res/shaders/src/tbn.glsl"



// return true if intersection found (t >= 0), out 'hit' is the intersection point
bool intersect_plane(vec3 ray_origin, vec3 ray_dir, vec3 plane_point, vec3 plane_normal, out vec3 hit) {
   float denominator = dot(ray_dir, plane_normal);
   const float EPS = 1e-6;
   if (abs(denominator) < EPS) {
      // parallel, no reliable intersection
      return false;
   }
   float t = dot(plane_point - ray_origin, plane_normal) / denominator;
   // we might want t >= 0 if ray only forward
   if (t < 0.0)
      return false;
   hit = ray_origin + t * ray_dir;
   return true;
}


#include "./src/parallax.glsl"
#include "./src/custom_instance_rendering.glsl"

void main() {

   uvec2 screen_size = uvec2(per_frame.screen_width, per_frame.screen_height);
   // vec2 screen_uv = (vec2(gl_FragCoord.xy)) / vec2(screen_size);
   vec2 screen_uv = vec2(gl_FragCoord.xy +  0.5) / vec2(screen_size);
   // visibility = sample_texture_bicubic(ambient_occlusion_texture, screen_uv);
   spherical = vec2(per_frame.camera.phi, per_frame.camera.theta);

   // color_attachment = vec4(1,0,1,1);
   // return;
   vec3 color = vec3(0.);
   vec3 diffuse_color  = vec3(1.);
   vec3 specular_color = vec3(0.);
   vec3 emissive_color = vec3(0.);
   float alpha_channel = 1.0;
   vec3 normal = normalize(in_vertex.normal);
   vec3 position = in_vertex.position;
   float shadow = 1.0;

   vec3 camera_direction = camera_forward(spherical);
   vec3 camera_position = per_frame.camera.position;
   init_lights(camera_position, camera_direction);
   vec2 uv = in_vertex.TextureCoordinate;

   if (in_vertex.instance_rendering_mode > 0) {
      // TODO: Expand parameters
      color_attachment = custom(uv, screen_uv, in_vertex.render_state, in_vertex.instance_rendering_mode, in_vertex.color_tint, in_vertex.custom_1, in_vertex.custom_2);
      return;
   }


   if (false && !gl_FrontFacing) {
      // NOTE: Shading rn is strange on Alleya model because we get back facing triangles poping up due to animation 
      normal = -normal; // flip normals on backfaces for double sided materials
   }

   Material material = materials[in_vertex.material_index];

   const bool has_height_map = true && material.height_handle != uvec2(0);
   const bool has_normal_map = true && material.normal_handle != uvec2(0);
   mat3 TBN;
   if (has_height_map ||  has_normal_map) {
      if (1 == in_vertex.has_tangents) {
         // NOTE: Maybe sending (position - camera_position) is better for floating precision since to work with smaller numbers.
         //       This works because camera_position is constant its derivative wrt to screen coordinates is zero;
         // TBN = cotangent_frame(position, normalize(normal), uv);
         // TBN = tangent_frame_mikkelsen(position, normalize(normal), uv);
         TBN = mat3(in_vertex.tangent, in_vertex.bitangent, normal);
      } else {
         // After interpolation, T may have drifted slightly non-perpendicular to N
         // Re-orthogonalize here, not in the vertex shader
         // vec3 T = (in_vertex.tangent - normal * dot(in_vertex.tangent, normal));
         vec3 N = normalize(in_vertex.normal);
         vec3 T = normalize(in_vertex.tangent);
         //  Re-orthogonalize T with respect to N after interpolation
         //  Interpolation can cause T to drift slightly out of the tangent plane
         T = normalize(T - normal * dot(T, N));
         vec3 B = cross(N, T) * in_vertex.tangent_w_sign; // tangent.w is handedness sign
         TBN = mat3(T, B, N);
      }
   }


   if (has_height_map) {
      const mat3 TTBN = false ? inverse(TBN) : transpose(TBN);
      const float height_scale = 1.0, height_bias = 0.0;
      const sampler2D height_map = sampler2D(material.height_handle);
      float height = sample_texture(height_map, in_vertex.TextureCoordinate).r * -1 + 1;
      vec3 position_in_tanget_space = TTBN * position;
      vec3 camera_position_in_tangent_space = TTBN * camera_position;

      vec3 tangent_space_view_direction = normalize(position_in_tanget_space - vec3(camera_position_in_tangent_space.xy, height));
      // vec3 tangent_space_view_direction = normalize(position_in_tanget_space - camera_position_in_tangent_space);

      vec2 original_uv = in_vertex.TextureCoordinate;
      float parallaxHeight;
      // uv = parallaxMapping_relief(height_map, tangent_space_view_direction, original_uv, parallaxHeight);
      // uv = parallaxMapping_occlusion(height_map, tangent_space_view_direction, original_uv, parallaxHeight);
      // uv3 = parallax_uv2(original_uv, tangent_space_view_direction2, height_map);
      // // uv = parallax_uv(original_uv, tangent_space_view_direction2, height_map);
      // vec2 uv4 = ParallaxMapping(uv, tangent_space_view_direction2, height_map,
      //     depth_steps,          // minimum number of steps
      //     depth_steps,          // maximum number of steps
      //     depth_scale
      // );
      //
      //
      // vec3 ro = tangent_space_position;
      // vec3 rd = -normalize(tangent_space_view_direction2);
      // int maxSteps = 12;
      // float tMax = 49.;
      //
      // vec2 uv1 = parallaxMapping(
      //     original_uv, // base UV coordinates
      //     rd,          // view direction in tangent space (pointing into surface)
      //     height_map,  // height map (R channel)
      //     depth_steps,          // minimum number of steps
      //     depth_steps,          // maximum number of steps
      //     -depth_scale,// parallax scale (depth scale)
      //     0,           // parallax bias
      //     int(depth_steps)           // refinement steps (binary search)
      // );
      //
      // vec3 new_position_ts;
      // float out_shadow;
      // // vec3 light_dir_ts = TTBN * -camera_direction;
      // vec3 light_dir_ts = normalize(TTBN *  camera_direction);
      //
      // float g_DepthScale = 0.3;
      // // float g_DepthScale = 1.9;
      // // float g_DepthScale = 0.9;
      //
      //
      vec3 p_ = vec3(original_uv, 0);
      vec3 v_ = normalize(tangent_space_view_direction);
      vec3 ray_direction = normalize(TTBN * (camera_position - position));

      vec3 to_light_ts;   // light direction in tangent space
      vec3  out_pos_ts;   // intersection (u,v,depth)
      float out_shadow;   // shadow factor (0.0 lit, 1.0 shadow)
      vec3 ro = position_in_tanget_space;
      vec2 uv2 = relief_mapping(
         original_uv,
         ro,         // current fragment position in tangent space
         ray_direction,
         height_map,
         2.15,
         to_light_ts,         // light direction in tangent space
         out_pos_ts,      // intersection (u,v,depth)
         out_shadow      // shadow factor (0.0 lit, 1.0 shadow)
      );

      vec2 uv1;
      {
         vec3 p = vec3(original_uv, 0);
         vec3 v = normalize(TTBN * (position - camera_position));
         uv1 = ray_intersect_relaxedcone(height_map, p, v);
      }

      vec2 uv3;
      {
         vec3 p = vec3(original_uv, 0);
         vec3 v = normalize(TTBN * (position - camera_position));
         uv3 = ray_intersect_relaxedcone2(height_map, p, v);
      }

      uv = uv2;
      uv = uv1;
      uv = uv3;

      // shadow = (1. - out_shadow*0.3);
      // // shadow = 1.0;
      //
      //
      // position = new_position_ts;
      // position = TBN * new_position_ts;
      // position = TTBN * new_position_ts;
      // uv = uv1;
      // uv = uv2;
      // uv = uv20;
      // uv = uv6;
      // uv = uv20;
      // uv = uv2;
      // uv = uv21;
      // uv = uv;

      // return_color(TBN * vec3(uv, height_texel));
      // normal *= tangent_space_position.z;
   }

   if (has_normal_map) {
      vec4 normal_texel = sample_texture(sampler2D(material.normal_handle), uv);
      normal = normalize(TBN * normalize(normal_texel.rgb * 2.0 - 1.0));
   }

   if (true && material.diffuse_handle != uvec2(0)) {
      // TODO: Mode gamma_correction to after sbti loading
      vec4 dtexture = sample_texture(sampler2D(material.diffuse_handle), uv);
      diffuse_color = srgb_to_linear(dtexture.xyz);
      alpha_channel = dtexture.w;
   }

   if (alpha_channel < 0.005) {
      // NOTE: Beware that Early-Z is disabled if your fragment shader does any of:
      //       discard / alpha test behavior
      //       alpha blending enabled
      //       writes gl_FragDepth
      //       side effects (imageStore/atomics in FS)
      //       In that case, instancing multiplies in fragment cost N times.
      //       That means that this is slowing down every single geometry even if there's no transparency on it
      // discard;
   }


   if (true && material.specular_handle != uvec2(0)) {
      vec4 stexture  = texture(sampler2D(material.specular_handle), uv);
      specular_color = stexture.xyz;
      has_specular = true;
   }


   Fragment fragment;
   fragment.position = position;
   fragment.normal   = normalize(normal);
   fragment.diffuse_color  = diffuse_color;
   fragment.specular_color = specular_color + vec3(0.18);
   fragment.emissive_color = emissive_color;
   color_attachment.xyzw = vec4(color, alpha_channel);




   for (int idx = 0; idx < lights.length(); ++idx) {
      Light light = lights[idx];

      float attenuation = 1.0;
      // Attenuation: distance for point/spot, 1.0 for directional
      if (light.type == LIGHT_TYPE_POINT || light.type == LIGHT_TYPE_SPOT) {
         attenuation = point_light_attenuation(light.position, fragment.position, light.range);
      }

      // Spotlight cone factor (only for spots)
      if (light.type == LIGHT_TYPE_SPOT) {

         attenuation *= spot_light(
            fragment.position,                          // fragment_position
            light.position,                             // spotlight_position
            light.forward.rgb,                            // spotlight_direction,
            light.cone_angle, light.cone_angle_increment
         );
      }

      // Direction to light
      vec3 light_direction = light.type == LIGHT_TYPE_DIRECTIONAL ? normalize(light.forward.xyz) // direction already stored
                                                                  : normalize(light.position - fragment.position);
      // attenuation = 0.0;

      // Accumulate colour
      color += shadow * attenuation * calculate_color(fragment, light.position, light_direction, light.color.a * light.color.rgb, camera_position);
   }

   float distance_to_view  = length(position - vec3(per_frame.camera.position.x, 0., per_frame.camera.position.z)); // Ignoring height of view
   float attenuation_alpha = clamp(distance_to_view/distance_to_view, 0.2, 1.0);
   color_attachment = vec4(color, attenuation_alpha);
   color_attachment.w = alpha_channel;
   color_attachment *= in_vertex.color_tint;
}



#pragma vertex
#version 460 core

// NOTE: Not doing vertex pulling is incorrect right now. Vertex Pulling ONLY
// layout(location = 0) in vec3 position;
// layout(location = 1) in vec3 normal;
// layout(location = 2) in vec2 uv;
#include "res/shaders/common.glsl"
#include "src/renderer/shared/types.glsl"  // drawcommand is defined here.
#include "src/renderer/shared/defines.glsl"

#include "res/shaders/src/buffers.glsl"

#include "res/shaders/src/coordinates.glsl"
#include "res/shaders/src/perspective.glsl"
#include "res/shaders/src/transformations.glsl"
#include "res/shaders/src/camera.glsl"
#include "res/shaders/src/rotation.glsl"



layout (location = 0) uniform vec4 push_constants[1];
layout (location = 0) out #include "./pipe.glsl" out_vertex;


vec3 pull_position(int id) {
   const int base = 0;
   return vec3(
      vertex_buffer[base + id*3 + 0],
      vertex_buffer[base + id*3 + 1],
      vertex_buffer[base + id*3 + 2]
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

vec4 pull_tangent(int id) {
   return vec4(vertex_tangents[id]);
}

void main() {
   vec3 camera_position      = per_frame.camera.position;
   vec2 spherical            = vec2(per_frame.camera.phi, per_frame.camera.theta);
   vec4 position             = vec4(pull_position(gl_VertexID), 1.0);
   vec3 normal               = pull_normal(gl_VertexID);
   vec2 uv                   = pull_uv(gl_VertexID);
   Draw_Command draw_command = draw_commands[gl_DrawID];
   highp mat4 model          = instances[gl_BaseInstance + gl_InstanceID].model_matrix;

   vec4 tangent              = vertex_tangents2[draw_command.tangents_offset + gl_VertexID - gl_BaseVertex];
   vec3 bitangent            = vec3(0.0);


   if (draw_command.has_joints == 1) {
      uint geometry_to_model_offset = instances[gl_BaseInstance + gl_InstanceID].geometry_to_model_offset;
      ivec4 joint_indices = joint_vertices[draw_command.joints_offset + gl_VertexID - gl_BaseVertex].joint_indices;
      vec4  joint_weights = joint_vertices[draw_command.joints_offset + gl_VertexID - gl_BaseVertex].joint_weights;
      if (false) {
         // Directly modified the position (bad, but we need for debugging sometimes)
         position =
              joint_weights[0] * (geometry_to_model[geometry_to_model_offset + joint_indices[0]] * position)
            + joint_weights[1] * (geometry_to_model[geometry_to_model_offset + joint_indices[1]] * position)
            + joint_weights[2] * (geometry_to_model[geometry_to_model_offset + joint_indices[2]] * position)
            + joint_weights[3] * (geometry_to_model[geometry_to_model_offset + joint_indices[3]] * position)
         ;
      } else {
         // Create the actual joint_transform and incorporate onto the model matrix
         highp mat4 joint_transform =
              joint_weights[0] * geometry_to_model[geometry_to_model_offset + joint_indices[0]]
            + joint_weights[1] * geometry_to_model[geometry_to_model_offset + joint_indices[1]]
            + joint_weights[2] * geometry_to_model[geometry_to_model_offset + joint_indices[2]]
            + joint_weights[3] * geometry_to_model[geometry_to_model_offset + joint_indices[3]]
         ;
         model = model * joint_transform;
      }
   }
   position = model * position;

   vec3 position_in_camera_space = camera_project(position, per_frame.camera.position, spherical).rgb;

   // See more about the normal matrix: http://www.lighthouse3d.com/tutorials/glsl-12-tutorial/the-normal-matrix/
   // Apply mat3 to "drop" the translation portion
   mat3 normal_matrix = transpose(inverse(mat3(model))); // was mat3(transpose(inverse(model)));

   // Normalize at the end as per: https://github.com/KhronosGroup/glTF/issues/2056#issuecomment-1213795031
   normal = normalize(normal_matrix * normal);
   if (1 == draw_command.has_tangents) {
      // Normalize TBN vectors before interpolation, per MikkTSpace. See: http://www.mikktspace.com/
      tangent.xyz = normalize(normal_matrix * tangent.xyz);
      tangent.xyz = normalize(tangent.xyz - dot(tangent.xyz, normal) * normal);
      // re-orthogonalize T with respect to N
      bitangent = normalize(cross(normal, tangent.xyz) * tangent.w);
   }

   {  // Send to next shader

      out_vertex.TextureCoordinate = uv;
      out_vertex.material_index = int(draw_command.material_index);
      out_vertex.ViewSpacePosition = position_in_camera_space.rgb;
      out_vertex.ViewSpaceNormal = camera_project(vec4(normal, 0.), camera_position, spherical).rgb;
   }

   {  // Camera to Clip
      // gl_Position = per_frame.perspective * vec4(position.xy, position.z*-1., position.w);
      // gl_Position = perspective_from_frustum(position.xyz, fov, aspect, near_plane, far_plane);
      // gl_Position = perspective_from_fov(fov, per_frame.camera.aspect, near_plane, far_plane) * vec4(position.xy, position.z*-1., position.w);

      // This one has infinite draw distance
      gl_Position = perspective_from_fov(position_in_camera_space.xyz, fov, per_frame.camera.aspect, near_plane, far_plane); // Appears to be infinite in depth
   }
}

#ifndef BUFFERS_HEADER
#define BUFFERS_HEADER

#include "res/shaders/common.glsl"
#include "src/renderer/shared/types.glsl"


#define RESTRICT restrict
// You can call vertex_buffer.length() to get the the count of positions
layout(std430, binding = BINDING_VERTEX_BUFFER) readonly RESTRICT buffer Vertex_Buffer {
   float vertex_buffer[];
};

layout(std430, binding = BINDING_VERTEX_TANGENT) readonly RESTRICT buffer Vertex_Tanget_Buffer {
   vec4 vertex_tangents[];
};

layout(std430, binding = BINDING_TANGENTS_BUFFER) readonly RESTRICT buffer Tangents_Buffer {
   vec4 vertex_tangents2[];
};

layout(std430, binding = BINDING_DRAW_COMMAND) readonly RESTRICT buffer Draw_Command_Buffer {
   Draw_Command draw_commands[];
};

layout(std430, binding = BINDING_MATERIAL) readonly RESTRICT buffer Material_Buffer {
   Material materials[];
};

layout(std430, binding = BINDING_INSTANCE_BUFFER) readonly RESTRICT buffer Instance_Buffer {
   Instance instances[];
};

// TODO: Check this is still under the correct size for ubo
layout(std140, binding = BINDING_PER_FRAME) uniform Per_Frame_Buffer {
   Per_Frame per_frame;
};


layout(std430, binding = BINDING_ANIMATION_MATRICES) readonly RESTRICT buffer Animation_Matrices_Buffer {
   mat4 geometry_to_model[]; // Geometry (vertices) to Model Space. We have one per joint of all renderables and its instances of the current scene
};

layout(std430, binding = BINDING_JOINT_BUFFER) readonly RESTRICT buffer Joint_Buffer {
   Joint_Vertex joint_vertices[];
  // for (int i = 0; i < 4; ++i) {
  //       mat4 bone_transform = geometry_to_model[bone_idxs[i]];
  //       position += bone_weights[i] * (bone_transform * vec4(position, 1.0));
  //   }
  //
  //   gl_Position = uModelViewProjection * vec4(position, 1.0);
};

layout(std430, binding = BINDING_INDICES_BUFFER) readonly RESTRICT buffer Indices_Buffer {
   float indices[];
};

#endif // BUFFERS_HEADER

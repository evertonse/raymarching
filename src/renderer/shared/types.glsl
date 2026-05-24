#ifndef SHARED_TYPES_HEADER
// This sequence can't change, and to be in this order and must come first.
// TODO: change instace to instances to match others
#define DRAW_COMMAND_BASE                                           \
   uint indices_count;                                              \
   uint instance_count;    /* For instanced rendering (usually 1)*/ \
   uint indices_offset;    /* Start index in index *not byte*    */ \
   uint vertices_offset;   /* Base Vertex in index *not byte*    */ \
   uint instance_offset    /* Base Instance in index *not byte*  */

struct Draw_Command {
   DRAW_COMMAND_BASE;
   uint material_index;
   uint vertices_count;
   uint tangents_offset, has_tangents;
   uint joints_offset, has_joints;
   uint is_inverleaved;
};


// NOTE: This way of doing thing imply uber shaders instead of creating a shader changing api from the manager.c
//       Will just get this working and see if it explodes in types or even if it lags later on.
#define Instance_Rendering_Mode uint
#define INSTANCE_RENDERING_MODE_NORMAL 0
#define INSTANCE_RENDERING_MODE_CUSTOM 1 // NOTE: Anything bigger than 0 will be treated as custom which you can use to identify a specific way that you may wanna render.


// TODO: Migrate to material_index for instance instead of the full draw command
struct Instance {
   mat4 model_matrix;
   uint geometry_to_model_offset; Instance_Rendering_Mode instance_rendering_mode; uint pad_1, pad_2;
   vec4 color_tint;
   vec4 custom_1, custom_2; // You get 2 custom vec4 to use as data any sorta data, simple.
};

struct Material {
   uvec2 diffuse_handle;
   uvec2 specular_handle;
   uvec2 emissive_handle;
   uvec2 normal_handle;
};

struct Joint_Vertex {
   ivec4 joint_indices;  // index into geometry_to_model
   vec4  joint_weights;  // \sum_over_(i=4){joint_weights[i] * bone_idxs[i]}
};

/*
   // How MDI sorta is
   unsigned int * indices = (unsigned int *)ELEMENT_ARRAY_BUFFER;
   for DrawElementsIndirectCommand cmd in GL_DRAW_INDIRECT_BUFFER {
      for uint i = 0; i < cmd.count; ++i {
         int gl_VertexID = indices[cmd.firstIndex + i] + cmd.baseVertex;
      }
   }
*/

/*
   Source: https://ktstephano.github.io/rendering/opengl/mdi

   gl_VertexID
      Vertex index with first index and base vertex offset

   gl_InstanceID
      Current instance whenever instanceCount > 1, else 0

   gl_DrawID
      The current draw command index we are on inside of the GL_DRAW_INDIRECT_BUFFER.
      So if you submitted 30 draw commands in the buffer, this value will range from 0 to 29.
      Useful for a situation such as needing to access a different transform matrix depending on the current draw command number.

   gl_BaseVertex
      Base vertex of current draw command

   gl_BaseInstance
      Base instance of current draw command (can use this to pass in any integer data you want if not using instanced vertex attributes)
*/


#define SHARED_TYPES_HEADER

#endif // SHARED_TYPES_HEADER

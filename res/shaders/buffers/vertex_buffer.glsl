// You can call vertex_buffer.length() to get the the count of positions
layout(std430, binding = 3) readonly buffer VertexData {
   float vertex_buffer[];
};

// This sequence can't change, and to be in this order and must come first.
#define DRAW_COMMAND_BASE                                           \
   uint index_count;                                                \
   uint instance_count;    /* For instanced rendering (usually 1)*/ \
   uint index_offset;      /* Start index in index *not byte*    */ \
   uint vertex_offset;     /* Base Vertex in index *not byte*    */ \
   uint instance_offset    /* Base Instance in index *not byte*  */

struct Draw_Command {
   DRAW_COMMAND_BASE;
   int  material_index;
   int  vertex_count;
   int  has_joints;
   int  is_inverleaved;
};

layout(std430, binding = 8) buffer DrawCommand {
   Draw_Command draw_commands[];
};

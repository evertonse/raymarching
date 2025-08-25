// You can call vertex_buffer.length() to get the the count of positions
layout(std430, binding = 3) buffer VertexData {
   float vertex_buffer[];
};


struct Draw_Command_Base {
   uint index_count;
   uint instance_count;
   uint index_offset;
   uint vertex_offset;
   uint instance_offset;
};

struct Draw_Command {
   Draw_Command_Base base;
   int  material_index;
   int  vertex_count;
   bool has_joints;
   bool is_inverleaved;
};

layout(std430, binding = 19) buffer DrawCommand {
   Draw_Command draw_commands[];
};

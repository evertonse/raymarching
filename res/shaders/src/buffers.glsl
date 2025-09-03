// You can call vertex_buffer.length() to get the the count of positions
layout(std430, binding = BINDING_VERTEX_BUFFER) readonly buffer Vertex_Buffer {
   float vertex_buffer[];
};

layout(std430, binding = BINDING_DRAW_COMMAND) readonly buffer Draw_Command_Buffer {
   Draw_Command draw_commands[];
};

layout(std430, binding = BINDING_MATERIAL) readonly buffer Material_Buffer {
   Material materials[];
};

layout(std430, binding = BINDING_INSTANCE_BUFFER) readonly buffer Instance_Buffer {
   Instance instances[];
};

layout(std140, binding = BINDING_CAMERA) uniform Camera_Buffer {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position;  float pad0;
    vec3 camera_direction; float pad1;
} camera;

struct Light {
    vec3 position; float pad0;
    vec3 ambient;  float pad1;
    vec3 diffuse;  float pad2;
    vec3 specular; float pad4;
};

struct Camera {
    vec3  position; float pad0;
    float theta, phi, pad1, pad2;
};


layout(std140, binding = BINDING_PER_FRAME) uniform Per_Frame {
    mat4 model, view, perspective;
    Light  light;
    Camera camera;
    float elapsed_time, delta_time;
} per_frame;

layout(std430, binding = BINDING_ANIMATION_MATRICES) buffer Animation_Matrices_Buffer {
   mat4 geometry_to_model[]; // Geometry (vertices) to Model Space.
};

layout(std430, binding = BINDING_JOINT_BUFFER) buffer Joint_Buffer {
   Joint_Vertex joint_vertices[];
  // for (int i = 0; i < 4; ++i) {
  //       mat4 bone_transform = geometry_to_model[bone_idxs[i]];
  //       position += bone_weights[i] * (bone_transform * vec4(position, 1.0));
  //   }
  //
  //   gl_Position = uModelViewProjection * vec4(position, 1.0);
};

layout(std430, binding = BINDING_INDICES_BUFFER) buffer Indices_Buffer {
   float indices[];
};
// buffers end

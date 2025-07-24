layout(std140, binding = 2) uniform Camera {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position; float _pad0;
    vec3 camera_direction; float _pad1;
};

layout(std140, binding = 4) uniform Ub_Data {
    mat4 model, view, perspective;
    // float cx, cy, cz, pad0;
    vec3 camera_position; float pad0;
    vec3 light_position;  float pad1;
    vec3 light_color;     float pad2;
    float theta, phi; float elapsed_time, delta_time;
} ub_data;

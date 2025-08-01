layout(std140, binding = 2) uniform Camera2 {
    // mat4 view;
    // mat4 proj;
    vec3 camera_position; float _pad0;
    vec3 camera_direction; float _pad1;
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


layout(std140, binding = 4) uniform Per_Frame {
    mat4 model, view, perspective;
    Light  light;
    Camera camera;
    float elapsed_time, delta_time;
} per_frame;


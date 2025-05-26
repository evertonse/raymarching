 #version 460 core

layout(rgba32f, binding = 0)
    uniform writeonly image2D output_image;

layout(local_size_x = 16, local_size_y = 16) in;

// Implemented ShaderToy Parameters
uniform float iTime;
uniform vec3  iResolution;

// Unimplemented ShaderToy Parameters
vec4 iMouse;
int iFrame;

// ShaderToy main function
void mainImage(out vec4, in vec2);

void main() {

    iMouse = vec4(0);
    iFrame = 0;

    ivec2 pixel_coord = ivec2(gl_GlobalInvocationID.xy);

    if ( pixel_coord.x >= imageSize(output_image).x
      || pixel_coord.y >= imageSize(output_image).y )
    {
        return;
    }

    ivec2 tex_size = imageSize(output_image); // Serves as 'iResolution' as well if cast to vec2

    // These two lines doesn't matter if we're raymarching in shadertoy style anyway
    // But will keep here to keep opengl compiler checking this
    vec2 normalized_coord = vec2(pixel_coord) / vec2(tex_size);
    vec4 O = vec4(normalized_coord, 0.0, 1.0);

    vec2 I = vec2(pixel_coord);
    mainImage(O, I);
    imageStore(output_image, pixel_coord, O);
}

// Some options to test out, just uncomment and press F
// #include "src/shaders/shadertoy/Hearts.glsl"
// #include "./src/shaders/shadertoy/ray-marching-primitives.glsl"
#include "./src/shaders/shadertoy/March.glsl"
// #include "src/shaders/shadertoy/twinkling-tunnel.glsl"

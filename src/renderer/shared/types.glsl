#ifndef SHARED_TYPES_HEADER
#define SHARED_TYPES_HEADER

#if defined(__STDC__)
#  define STRUCT struct
   // Match glsl types. These have to exist though, this is not plug an play.
   typedef u64        uvec2;
   typedef u32        uint;
   typedef Vector4Int ivec4;
   typedef Vector4    vec4;
   typedef Vector3    vec3;
   typedef float16    mat4;
#else
#  define STRUCT
#endif

// This sequence can't change, and to be in this order and must come first.
// TODO: change instace to instances to match others
#define DRAW_COMMAND_BASE                                               \
   uint indices_count;                                                  \
   uint instance_count;    /* For instanced rendering (usually 1)    */ \
   uint indices_offset;    /* Start index in index *not in bytes*    */ \
   uint vertices_offset;   /* Base Vertex in index *not in bytes*    */ \
   uint instance_offset    /* Base Instance in index *not in bytes*  */

struct Draw_Command {
   DRAW_COMMAND_BASE;
   uint material_index;
   uint vertices_count;
   uint tangents_offset, has_tangents;
   uint joints_offset, has_joints;
   uint is_inverleaved; // unused
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
   uvec2 diffuse_handle          ;
   uvec2 specular_handle         ;
   uvec2 roughness_handle        ;
   uvec2 emissive_handle         ;
   uvec2 normal_handle           ;
   uvec2 height_handle           ;
   uvec2 ambient_occlusion_handle;

   // float metallic;            // 0.0 = dielectric, 1.0 = metal
   // float roughness;           // 0.0 = mirror,     1.0 = diffuse
   // float index_of_refraction; // for dielectrics
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


#define LIGHT_TYPE_DIRECTIONAL 0u
#define LIGHT_TYPE_POINT       1u
#define LIGHT_TYPE_SPOT        2u

// Maybe better packing?
struct Light {
   // direction for spot lights it mean the cone opens from this direction it pierces the cones in the the middle of its base  and direction lights
   // direction for directional lights just means the direction. Position is ignored.
   vec3 forward; float pad0;

   vec3 position;   // world position (for spot/point lights)
   uint type;       // LIGHT_TYPE_*
   vec4 color;      // .rgb=RGB .w = intensity that multiplyes color (we're in hdr)


   float range; // Attenuation distance (spot light)

   // For spotlight avoid invalid state by having cone_angle_increment instead of outer_angle
   float cone_angle;
   float cone_angle_increment;

   float shadowBias; // Depth bias
   // Shadows
   uint  shadow_map_index; // Index into shadow atlas or array texture
   uint  casts_shadows;    // Boolean flag
   float radius;    // Light source radius for soft shadows
   float pad1;
};


struct Camera {
   vec3 position; float pad0;
   float theta, phi, aspect, pad1;
};


struct Per_Frame {
   STRUCT Camera camera;
   float elapsed_time, delta_time; uint screen_width, screen_height;
};

#if defined(__STDC__)
   typedef struct Gpu_Camera Gpu_Camera;
   typedef struct Per_Frame  Per_Frame;
   typedef struct Joint_Vertex Joint_Vertex;
   typedef struct Draw_Command Draw_Command;
   typedef struct Material     Material;
#endif

#if 0
struct Ibl {
   samplerCube irradiance_map; // Pre integrated diffuse irradiance
   samplerCube radiance_map;   // Pre integrated specular radiance
   sampler2D brdf_lut;         // 2D BRDF lookup table

   float intensity;
   uint pad0;
   uint pad1;
   uint pad2;
};
#endif
#endif // SHARED_TYPES_HEADER

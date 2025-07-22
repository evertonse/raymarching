#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "./shader.c"
#include "./mesh.c"
#include "./texture.c"
#include "./buffer.c"
#include "./nuklear.c"


typedef struct {
   union {
      f32 position[3];
      Vector3 position_v3;
   };

   union {
      f32 normal[3];
      Vector3 normal_v3;
   };
   union {
      f32 uv[2];
      Vector2 uv_v2;
   };
} Vertex;

typedef struct {
    GLuint handle;
    Vertex_Buffer vb;
    Index_Buffer ib;
} Vertex_Array;


typedef struct {
    i32 x;                // Rectangle top-left corner position x
    i32 y;                // Rectangle top-left corner position y
    i32 width;            // Rectangle width
    i32 height;           // Rectangle height
} Rectanglei32;



inline bool is_valid_vertex_array(Vertex_Array va) {
    if (0 == va.handle) {
       trace_info("Vertex Array is has zero handle");
       return false;
    }

    if (!is_valid_vertex_buffer(va.vb)) {
       trace_info("Vertex Array has bad vertex buffer");
       return false;
    }

    if (!is_valid_index_buffer(va.ib)) {
       trace_info("Vertex Array has bad index buffer");
       return false;
    }

    #ifdef _DEBUG
    GLint va_valid;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &va_valid);
    return va_valid == va.handle;
    #else
    return true;
    #endif
}


// Rectanglei32
inline bool is_valid_rectangle(Rectanglei32 r) {
    return r.width > 0 && r.height > 0;
}


// The last element buffer object that gets bound while a VAO is bound, is stored as the VAO's element buffer object. Binding to a VAO then also automatically binds that EBO.
Vertex_Array create_vertex_array(const Vertex *vertices, usz vertex_count, const u32 *indices, usz indices_count) {
   Vertex_Array va = {0};

   glCreateVertexArrays(1, &va.handle);

   va.vb = create_vertex_buffer(vertices, vertex_count * size_of(Vertex) , vertex_count);
   va.ib = create_index_buffer(indices, indices_count);
   glVertexArrayElementBuffer(va.handle, va.ib.buffer.handle);

   // Vertex attributes
   glVertexArrayVertexBuffer(va.handle, 0, va.vb.buffer.handle, 0, size_of(Vertex));

   glEnableVertexArrayAttrib(va.handle, 0);
   glVertexArrayAttribFormat(va.handle, 0, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, position));
   glVertexArrayAttribBinding(va.handle, 0, 0);

   glEnableVertexArrayAttrib(va.handle, 1);
   glVertexArrayAttribFormat(va.handle, 1, 3, GL_FLOAT, GL_FALSE, offset_of(Vertex, normal));
   glVertexArrayAttribBinding(va.handle, 1, 0);

   glEnableVertexArrayAttrib(va.handle, 2);
   glVertexArrayAttribFormat(va.handle, 2, 2, GL_FLOAT, GL_FALSE, offset_of(Vertex, uv));
   glVertexArrayAttribBinding(va.handle, 2, 0);

   return va;
}

Vertex_Array create_vertex_array_from_mesh(const Mesh *mesh) {
   Vertex_Array va = {0};

   assert(mesh != NULL);
   assert(mesh->vertices != NULL);
   assert(mesh->normals != NULL);
   assert(mesh->uvs != NULL);
   assert(mesh->indices != NULL);
   assert(mesh->vertices_count > 0);
   assert(mesh->indices_count > 0);
   assert_msg(mesh->vertices_count == mesh->normals_count && mesh->vertices_count == mesh->uvs_count, "vertices=%d normals=%d uvs=%d", mesh->vertices_count,mesh->normals_count, mesh->uvs_count);

   // Calculate sizes
   usz vertex_size = mesh->vertices_count * size_of(Vector3);
   usz normal_size = mesh->normals_count  * size_of(Vector3);
   usz uv_size     = mesh->uvs_count      * size_of(Vector2);
   usz total_size  = vertex_size + normal_size + uv_size;

   // Create VAO
   glCreateVertexArrays(1, &va.handle);

   // Create and upload VBO
   va.vb = create_vertex_buffer(nullptr, total_size + size_of(f32), mesh->vertices_count);

   isz offset = 0;

   if (false) { // Make first float be the vertices count. But I don't think we need that even if we're using as storage buffer
      f32 vertex_count = (f32)mesh->vertices_count;
      offset = update_buffer(va.vb.buffer, &vertex_count, size_of(vertex_count), offset); // metadata the first element is
   }

   isz positions_offset = offset;
   offset = update_buffer(va.vb.buffer, mesh->vertices, vertex_size, offset);

   isz normals_offset = offset;
   offset = update_buffer(va.vb.buffer, mesh->normals,  normal_size, offset);

   isz uvs_offset = offset;
   offset = update_buffer(va.vb.buffer, mesh->uvs,      uv_size,     offset);


   // Link VBO to VAO (positions)
   glEnableVertexArrayAttrib(va.handle, 0);
   glVertexArrayVertexBuffer(va.handle, 0, va.vb.buffer.handle, positions_offset, size_of(Vector3));
   glVertexArrayAttribFormat(va.handle, 0, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 0, 0);

   // Normals (offset binding)
   glEnableVertexArrayAttrib(va.handle, 1);
   glVertexArrayVertexBuffer(va.handle, 1, va.vb.buffer.handle, normals_offset, size_of(Vector3));
   glVertexArrayAttribFormat(va.handle, 1, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 1, 1);

   // UVs
   glEnableVertexArrayAttrib(va.handle, 2);
   glVertexArrayVertexBuffer(va.handle, 2, va.vb.buffer.handle, uvs_offset, size_of(Vector2));
   glVertexArrayAttribFormat(va.handle, 2, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(va.handle, 2, 2);


   // Create and upload index buffer and link to va
   va.ib = create_index_buffer(mesh->indices, mesh->indices_count);
   glVertexArrayElementBuffer(va.handle, va.ib.buffer.handle);
   return va;
}

void bind_vertex_array(const Vertex_Array va) {
   glBindVertexArray(va.handle);
}

#include "framebuffer.c"

static const char* human_readable_size(i64 bytes) {
    static char output[32];
    static const char *units[] = {"B", "KB", "MB", "GB"};
    f64 size = (f64)bytes;
    int unit_index = 0;

    while (size >= 1024.0 && unit_index < 3) {
        size /= 1024.0;
        unit_index++;
    }

    snprintf(output, size_of(output), "%.2f %s", size, units[unit_index]);
    return output;
}

void print_opengl_resource_limits(void) {
   GLint value;

   trace_info("=== OpenGL Resource Limits ===\n\n");

   // TEXTURES
   glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
   printf("Max texture image units per fragment shader: %d\n", value);

   glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
   printf("Max combined texture image units (all shader stages): %d\n", value);

   glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &value);
   printf("Max texture units in vertex shader: %d\n", value);

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
   printf("Max 2D texture size: %dx%d\n", value, value);

   glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value);
   printf("Max 3D texture size: %dx%dx%d\n", value, value, value);

   glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &value);
   printf("Max cube map size: %dx%d\n", value, value);

   glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &value);
   printf("Max array texture layers: %d\n", value);

   // UNIFORMS
   glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
   printf("Max vertex shader uniforms (floats): %d\n", value);

   glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &value);
   printf("Max fragment shader uniforms (floats): %d\n", value);

   glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &value);
   printf("Max combined uniform blocks across all stages: %d\n", value);

   glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
   printf("Max uniform buffer binding points: %d\n", value);

   glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
   printf("Max size of a single UBO: %s\n", human_readable_size(value));

   // SHADER STORAGE BUFFERS (SSBOs)
   glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &value);
   printf("Max SSBO binding points: %d\n", value);

   glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
   printf("Max SSBO block size: %s\n", human_readable_size(value));

   // ATTRIBUTES & VARYINGS
   glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
   printf("Max vertex attributes (vec3 pos, vec3 normal, etc): %d\n", value);

   glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &value);
   printf("Max varying components between vertex & fragment shaders: %d\n", value);

   // FRAMEBUFFERS
   glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
   printf("Max framebuffer color attachments: %d\n", value);

   glGetIntegerv(GL_MAX_DRAW_BUFFERS, &value);
   printf("Max draw buffers (MRT): %d\n", value);

   // IMAGE UNITS
   glGetIntegerv(GL_MAX_IMAGE_UNITS, &value);
   printf("Max image units for shaders: %d\n", value);

   // COMPUTE SHADER
   glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value);
   printf("Max compute work group invocations: %d\n", value);

   GLint wg_size[3];
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &wg_size[0]);
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &wg_size[1]);
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &wg_size[2]);
   printf("Max compute work group sizes: [%d, %d, %d]\n", wg_size[0], wg_size[1], wg_size[2]);

   // TRANSFORM FEEDBACK
   glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &value);
   printf("Max transform feedback separate attribs: %d\n", value);

   glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, &value);
   printf("Max transform feedback components: %d\n", value);

   // RECOMMENDED DRAW COUNTS
   glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
   printf("Max recommended glDrawElements vertices: %d\n", value);

   glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &value);
   printf("Max recommended glDrawElements indices: %d\n", value);

   // VIEWPORT
   GLint dims[2];
   glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
   printf("Max viewport dimensions: %d x %d\n", dims[0], dims[1]);

   printf("\n=================================\n\n");
}

// https://learnopengl.com/In-Practice/Debugging
void glDebugOutput(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* userParam)
{
	// ignore non-significant error/warning codes
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204
		|| id == 131222
		) return;
	if (type == GL_DEBUG_TYPE_PERFORMANCE) return;


   const char* source_str = "unknown";
	switch (source) {
	case GL_DEBUG_SOURCE_API:             source_str = ("API"); break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   source_str = ("Window System"); break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: source_str = ("Shader Compiler"); break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     source_str = ("Third Party"); break;
	case GL_DEBUG_SOURCE_APPLICATION:     source_str = ("Application"); break;
	case GL_DEBUG_SOURCE_OTHER:           source_str = ("Other"); break;
	}


   const char* type_str = "unknown";
	switch (type) {
	case GL_DEBUG_TYPE_ERROR:               type_str = ("Error"); break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: type_str = ("Deprecated Behaviour"); break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  type_str = ("Undefined Behaviour"); break;
	case GL_DEBUG_TYPE_PORTABILITY:         type_str = ("Portability"); break;
	case GL_DEBUG_TYPE_PERFORMANCE:         type_str = ("Performance"); break;
	case GL_DEBUG_TYPE_MARKER:              type_str = ("Marker"); break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          type_str = ("Push Group"); break;
	case GL_DEBUG_TYPE_POP_GROUP:           type_str = ("Pop Group"); break;
	case GL_DEBUG_TYPE_OTHER:               type_str = ("Other"); break;
	}

   const char* severity_str = "unknown";
	switch (severity) {
	case GL_DEBUG_SEVERITY_HIGH:         severity_str = ("high"); break;
	case GL_DEBUG_SEVERITY_MEDIUM:       severity_str = ("medium"); break;
	case GL_DEBUG_SEVERITY_LOW:          severity_str = ("low"); break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: severity_str = ("notification"); break;
	}

	trace_info("OpenGL debug message (%d) (severity = %s) (type = ) (source = ) '%s'\n ", id, severity_str, type_str, source_str, message);
}

void enable_error_report() {
   trace_info("=== OpenGL error reporting enable ===\n");
   glEnable(GL_DEBUG_OUTPUT);
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
   glDebugMessageCallback(glDebugOutput, nullptr);
   glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
   glDebugMessageCallback(0, nullptr);
}


void init_renderer(void) {
   assert_msg(__state.renderer.initialized == false, "Renderer initialized twice?");
   __state.renderer.initialized  = true;
   int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
   if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
      enable_error_report();
   }

   print_opengl_resource_limits();

   { // Some expected settings
      glEnable(GL_DEPTH_TEST);
      glEnable(GL_BLEND);
      glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDisable(GL_MULTISAMPLE);
      glDisable(GL_CULL_FACE);
      // glCullFace(GL_BACK);          // Cull back faces
      glFrontFace(GL_CCW);             // GL_CCW to define front faces as counter-clockwise
   }
}

void shutdown_renderer(void) {
}

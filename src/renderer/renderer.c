#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./shared/defines.glsl"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "./shader.c"
#include "./mesh.c"
#include "./texture.c"
#include "./animation.c"
#include "./model.c"
#include "./buffer.c"
#include "./nuklear.c"
#include "./text.c"
#include "./manager.c"

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
} Rectangle_I32;


inline bool is_valid_vertex_array(Vertex_Array va) {
   if (0 == va.handle) {
      trace_info("Vertex Array has zero handle");
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

#ifdef RENDERER_DEBUG
   if (!glIsVertexArray(va.handle)) {
      trace_info("Vertex Array handle is not valid");
      return false;
   }
#endif
   return true;
}


// Rectangle_I32
inline bool is_valid_rectangle(Rectangle_I32 r) {
    return r.width > 0 && r.height > 0;
}


// The last element buffer object that gets bound while a VAO is bound, is stored as the VAO's element buffer object. Binding to a VAO then also automatically binds that EBO.
Vertex_Array create_vertex_array(const Vertex *vertices, usz vertex_count, const u32 *indices, usz indices_count) {
   Vertex_Array va = {0};

   glCreateVertexArrays(1, &va.handle);

   va.vb = create_vertex_buffer(vertices, vertex_count * size_of(Vertex), vertex_count);
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


void set_redererer_mode(Renderer_Mode mode) {
   // WARN: We gotta make sure the actuall state and our's aren't desync.
   //       Sometimes we might be using a lib that changes the gl state.
   if (mode == __state.renderer.mode) {
      return;
   }

   if (RENDERER_MODE_WIREFRAME == mode) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   }
   else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
   }
   __state.renderer.mode = mode;
}


Vertex_Array create_vertex_array_from_arrays(Vector3 *positions, Vector3 *normals, Vector2* uvs, isz count, u32* indices, isz indices_count) {
   Vertex_Array va = {0};
   // Calculate sizes
   usz positions_size = count * size_of(positions[0]);
   usz normals_size   = count * size_of(normals[0]);
   usz uvs_size       = count * size_of(uvs[0]);
   usz total_size     = positions_size + normals_size + uvs_size;

   // Create VAO
   glCreateVertexArrays(1, &va.handle);

   // Create and upload VBO. Plus one just in case we need to add one more float to query the size of the array from shaders
   // But buffers can be queried with '.length()' from shader. I just dk if it's portable?

   const bool is_continuous_buffer =
         (u64)positions + positions_size == (u64)normals
      && (u64)normals   + normals_size == (u64)uvs;
   ;

   isz positions_offset = 0;
   isz normals_offset   = positions_size;
   isz uvs_offset       = positions_size + normals_size;

   if (is_continuous_buffer) {
      trace_debug("Detected continuous buffer in vertex array creation from mesh. Optimization: no update calls will be needed.");
      va.vb = create_vertex_buffer(positions, total_size + size_of(f32), count);
   } else {
      va.vb = create_vertex_buffer(nullptr, total_size + size_of(f32), count);
      isz offset = 0;
      positions_offset = offset;
      offset         = update_buffer(&va.vb.buffer, positions, offset, positions_size);

      normals_offset = offset;
      offset         = update_buffer(&va.vb.buffer, normals,   offset, normals_size  );

      uvs_offset     = offset;
      offset         = update_buffer(&va.vb.buffer, uvs,       offset, uvs_size      );
   }


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
   va.ib = create_index_buffer(indices, indices_count);
   glVertexArrayElementBuffer(va.handle, va.ib.buffer.handle);
   return va;
}

void create_vertex_arrays_from_mesh(const Mesh *mesh, Vertex_Array *out_items) {
   assert(mesh && is_valid_mesh(*mesh));

   // Create vertex arrays for each surface
   for (size_t surface_idx = 0; surface_idx < mesh->surfaces.count; surface_idx++) {
      auto surface = mesh->surfaces.items[surface_idx];
      Vertex_Array *va = &out_items[surface_idx];
      Vector3 *positions = mesh->vertices.positions;
      Vector3 *normals   = mesh->vertices.normals;
      Vector2* uvs       = mesh->vertices.uvs;
      isz count          = mesh->vertices.count;
      u32* indices       = mesh->indices.items + surface.indices_offset;
      isz  indices_count = surface.indices_count;
      // We're retardedly creating a new vertex buffer for no reason other than its convenient right now, and we're gonna refactor into something comepletly different anyhow
      *va = create_vertex_array_from_arrays(positions, normals, uvs, count, indices, indices_count);
   }
}


Vertex_Array create_vertex_array_from_mesh(const Mesh *mesh) {
   assert(mesh && is_valid_mesh(*mesh));
   Vertex_Array va = {0};

   Vector3 *positions = mesh->vertices.positions;
   Vector3 *normals   = mesh->vertices.normals;
   Vector2* uvs       = mesh->vertices.uvs;
   isz      count     = mesh->vertices.count;

   u32* indices       = mesh->indices.items;
   isz  indices_count = mesh->indices.count;
   // We're retardedly creating a new vertex buffer for no reason other than its convenient right now, and we're gonna refactor into something comepletly different anyhow
   va = create_vertex_array_from_arrays(positions, normals, uvs, count, indices, indices_count);
   return va;
}


Vertex_Array create_cube_vertex_array(void) {
   constexpr float interleaved[] = {
      // positions          // normals           // texture coords
      -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
       0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
       0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,

       0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
      -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
      -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
       0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,

       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
      -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

      -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
      -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
      -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,

      -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
      -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
      -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

       0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
       0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
       0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,

       0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
       0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
       0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

      -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
       0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
       0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,

       0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
      -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
      -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

      -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
       0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,

       0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
      -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
      -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
   };

   static Mesh mesh = {0};
   if (!is_valid_mesh(mesh)) {
      mesh = create_mesh_from_interleaved(interleaved, count_of(interleaved));
   }
   return create_vertex_array_from_mesh(&mesh);
}


void bind_vertex_array(const Vertex_Array va) {
   glBindVertexArray(va.handle);
}


#include "framebuffer.c"
#include "bloom.c"
// #include "bloom_2.c"
// #include "bloom_3.c"
#include "postprocess.c"


void update_renderer(void) {
   // TODO: Check is window_height/width correspond to actual framebuffer
   int samples = default_framebuffer_samples();
   assert_msg(samples <= 0, "We're not ready to deal with multisampled default framebuffer (swapchain)");

   static bool first_time = false;
   bool inform_change = false;
   if (first_time || samples >= 1) {
      first_time = true;
      inform_change = true;
   }
   default_framebuffer = (Framebuffer){
      .handle = 0,
      .color = {
         .width  = get_window_width(),
         .height = get_window_height(),
      },
      .depth = {
         .width  = get_window_width(),
         .height = get_window_height(),
      },
      .is_default_framebuffer = true,
   };

   if (inform_change) {
      trace_okay("Here's some fucking news about the default framebuffer:");
      trace_struct(default_framebuffer);
   }
}


void print_default_framebuffer_info(void) {
   GLint viewport[4]; // Need array for 4 values: x, y, width, height

   // Bind the default framebuffer (0)
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
   trace_info("=== Default Framebuffer Info ===\n");

   // Resolution (viewport size, not FBO size)
   glGetIntegerv(GL_VIEWPORT, viewport);
   trace_info("Viewport: %d x %d (x = %d, y = %d)\n", viewport[2], viewport[3], viewport[0], viewport[1]); // width, height, x, y

   // Samples (MSAA)
   GLint samples;
   glGetIntegerv(GL_SAMPLES, &samples);
   trace_info("Samples: %d\n", samples);

   // Color attachment - check both front and back buffers
   GLint red, green, blue, alpha;

   trace_info("Back buffer format:\n");
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &green);
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blue);
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha);
   trace_info("  Color format: R%d G%d B%d A%d\n", red, green, blue, alpha);

   // Check if we have a front buffer (only in double-buffered contexts)
   GLint doublebuf = 0;
   glGetIntegerv(GL_DOUBLEBUFFER, &doublebuf);
   trace_info("Double-buffered: %s\n", doublebuf ? "yes" : "no");

   if (doublebuf) {
      trace_info("Front buffer format:\n");
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &green);
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blue);
      glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_FRONT_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha);
      trace_info("  Color format: R%d G%d B%d A%d\n", red, green, blue, alpha);
   }

   // Depth buffer
   GLint depth_size = 0;
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_size);
   trace_info("Depth bits: %d\n", depth_size);

   // Stencil buffer
   GLint stencil_size = 0;
   glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil_size);
   trace_info("Stencil bits: %d\n", stencil_size);

   // Check for errors
   GLenum error = glGetError();
   if (error != GL_NO_ERROR) {
      trace_info("OpenGL error occurred: 0x%X\n", error);
   }
}


void print_opengl_resource_limits(void) {
   GLint value;

   trace_info("=== OpenGL Resource Limits ===\n\n");

   // TEXTURES
   glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &value);
   trace_info("Max texture image units per fragment shader: %d\n", value);

   glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
   trace_info("Max combined texture image units (all shader stages): %d\n", value);

   glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &value);
   trace_info("Max texture units in vertex shader: %d\n", value);

   glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
   trace_info("Max 2D texture size: %dx%d\n", value, value);

   glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value);
   trace_info("Max 3D texture size: %dx%dx%d\n", value, value, value);

   glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &value);
   trace_info("Max cube map size: %dx%d\n", value, value);

   glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &value);
   trace_info("Max array texture layers: %d\n", value);

   // UNIFORMS
   glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &value);
   trace_info("Max vertex shader uniforms (floats): %d\n", value);

   glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &value);
   trace_info("Max fragment shader uniforms (floats): %d\n", value);

   glGetIntegerv(GL_MAX_COMBINED_UNIFORM_BLOCKS, &value);
   trace_info("Max combined uniform blocks across all stages: %d\n", value);

   glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
   trace_info("Max uniform buffer binding points: %d\n", value);

   glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &value);
   trace_info("Max size of a single UBO: %s\n", human_readable_size(value));

   // SHADER STORAGE BUFFERS (SSBOs)
   glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &value);
   trace_info("Max SSBO binding points: %d\n", value);

   glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
   trace_info("Max SSBO block size: %s\n", human_readable_size(value));

   // ATTRIBUTES & VARYINGS
   glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &value);
   trace_info("Max vertex attributes (vec3 pos, vec3 normal, etc): %d\n", value);

   glGetIntegerv(GL_MAX_VARYING_COMPONENTS, &value);
   trace_info("Max varying components between vertex & fragment shaders: %d\n", value);

   // FRAMEBUFFERS
   glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
   trace_info("Max framebuffer color attachments: %d\n", value);

   glGetIntegerv(GL_MAX_DRAW_BUFFERS, &value);
   trace_info("Max draw buffers (MRT): %d\n", value);

   // IMAGE UNITS
   glGetIntegerv(GL_MAX_IMAGE_UNITS, &value);
   trace_info("Max image units for shaders: %d\n", value);

   // COMPUTE SHADER
   glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value);
   trace_info("Max compute work group invocations: %d\n", value);

   GLint wg_size[3];
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &wg_size[0]);
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &wg_size[1]);
   glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &wg_size[2]);
   trace_info("Max compute work group sizes: [%d, %d, %d]\n", wg_size[0], wg_size[1], wg_size[2]);

   // TRANSFORM FEEDBACK
   glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS, &value);
   trace_info("Max transform feedback separate attribs: %d\n", value);

   glGetIntegerv(GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS, &value);
   trace_info("Max transform feedback components: %d\n", value);

   // RECOMMENDED DRAW COUNTS
   glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, &value);
   trace_info("Max recommended glDrawElements vertices: %d\n", value);

   glGetIntegerv(GL_MAX_ELEMENTS_INDICES, &value);
   trace_info("Max recommended glDrawElements indices: %d\n", value);

   // VIEWPORT
   GLint dims[2];
   glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
   trace_info("Max viewport dimensions: %d x %d\n", dims[0], dims[1]);

   trace_info("\n=================================\n\n");
}

// https://learnopengl.com/In-Practice/Debugging
void debug_opengl_output(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* userParam)
{
   if (
      // id == 131169 || // Framebuffer detailed info: The driver allocated storage for renderbuffer [X].
      id == 131185 || // Buffer detailed info: The driver is using video memory for buffer [X].
      // id == 131218 || // Program/shader state performance warning: Fragment shader in program [X] is being recompiled based on state.
      // id == 131204 || // Texture state usage warning: Texture [X] is base level inconsistent. Level [0] has inconsistent dimensions or formats.
      // id == 131154 ||    // Pixel-path performance warning: Pixel transfer is synchronized with 3D rendering.
      0
   ) {
      return;
   }

	// if (type == GL_DEBUG_TYPE_PERFORMANCE) return;


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

	trace_info(
      "OpenGL debug message (%d) (severity = %s) (type = %s) (source = %s) '%s'\n ",
      id, severity_str, type_str, source_str, message
   );
}


void enable_error_report() {
   trace_info("=== OpenGL error reporting enable ===\n");
   glEnable(GL_DEBUG_OUTPUT);
   glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
   glDebugMessageCallback(debug_opengl_output, nullptr);
   // glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
   // glDebugMessageCallback(0, nullptr);
}

void initialize_opengl_options(void) {

   glDisable(GL_FRAMEBUFFER_SRGB);
   // glEnable(GL_DITHER);
   glDisable(GL_DITHER);

   {
      // https://registry.khronos.org/OpenGL-Refpages/gl4/html/glMinSampleShading.xhtml
      // NOTE: Enabling GL_MULTISAMPLE might break raymarching because you can't bind a texture as image with multisample
      glEnable(GL_MULTISAMPLE);
      // IMPORTANT NOTE (May-06-2026):
      //    Disabling these are critical for MSAA + transparency.
      //    Somehow we see severe banding when using samples for framebuffer. Disabling these brings back the alpha smoothness.
      //    This is clear when developing vfx where lots of blending are required.
      //    At the same time, hair that is foliage-like ("sophia doll victory dance.fbx") depends on GL_SAMPLE_ALPHA_TO_COVERAGE enabled, otherwise it's rendererd completely wrong.
      //
      glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
      glDisable(GL_SAMPLE_ALPHA_TO_ONE);
      // Enable Supersampling with GL_SAMPLE_SHADING and glMinSampleShading set to 1
      // glEnable(GL_SAMPLE_SHADING);
      // glMinSampleShading(1.0):

      glEnable(GL_BLEND);
      glBlendEquation(GL_FUNC_ADD);
      // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }

   { // Some expected settings
      glEnable(GL_DEPTH_TEST);
      glDisable(GL_CULL_FACE);
      glCullFace(GL_BACK); // GL_BACK to Cull back faces
      glFrontFace(GL_CCW); // GL_CCW to define front faces as counter-clockwise
   }

   {
      glDisable(GL_SCISSOR_TEST);
      glEnable(GL_STENCIL_TEST);
   }
}

void init_renderer(void) {
   assert_msg(__state.renderer.initialized == false, "Renderer initialized twice?");

   int flags;
   glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
   bool has_debug_support = (flags & GL_CONTEXT_FLAG_DEBUG_BIT);
   if (!has_debug_support) {
      trace_warn("% OpenGL Context Debug is not set.");
   }
   enable_error_report();

   trace_info("GL_VENDOR   : %s\n", glGetString(GL_VENDOR));
   trace_info("GL_RENDERER : %s\n", glGetString(GL_RENDERER));
   trace_info("GL_VERSION  : %s\n", glGetString(GL_VERSION));

   print_opengl_resource_limits();
   print_default_framebuffer_info();
   initialize_opengl_options();
   __state.renderer.initialized = true;
   __state.renderer.mode = RENDERER_MODE_FILL;
}


void debug_depth_testing() {
   printf("=== Depth Testing Debug ===\n");

   // Check depth state
   GLboolean depth_test;
   glGetBooleanv(GL_DEPTH_TEST, &depth_test);
   printf("Depth test enabled: %s\n", depth_test ? "yes" : "no");

   GLint depth_func;
   glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
   printf("Depth function: 0x%04X\n", depth_func);

   GLboolean depth_mask;
   glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
   printf("Depth writes enabled: %s\n", depth_mask ? "yes" : "no");

   GLfloat depth_clear;
   glGetFloatv(GL_DEPTH_CLEAR_VALUE, &depth_clear);
   printf("Depth clear value: %f\n", depth_clear);

   GLdouble depth_range[2];
   glGetDoublev(GL_DEPTH_RANGE, depth_range);
   printf("Depth range: near=%f, far=%f\n", depth_range[0], depth_range[1]);

   printf("==========================\n");
}

void debug_culling_state() {
   printf("=== Culling State Debug ===\n");

   // Check if culling is enabled
   GLboolean cull_face;
   glGetBooleanv(GL_CULL_FACE, &cull_face);
   printf("Cull face enabled: %s\n", cull_face ? "yes" : "no");

   // Check which face is being culled
   GLint cull_face_mode;
   glGetIntegerv(GL_CULL_FACE_MODE, &cull_face_mode);
   printf("Cull face mode: %s\n", cull_face_mode == GL_BACK ? "GL_BACK" : cull_face_mode == GL_FRONT ? "GL_FRONT" : "GL_FRONT_AND_BACK");

   // Check front face winding order
   GLint front_face;
   glGetIntegerv(GL_FRONT_FACE, &front_face);
   printf("Front face winding: %s\n", front_face == GL_CCW ? "GL_CCW (Counter-clockwise)" : "GL_CW (Clockwise)");

   printf("===========================\n");
}

void shutdown_renderer(void) {
}


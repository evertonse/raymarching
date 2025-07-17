#include "raymath.h"
#include <stdio.h>

#define GLFW_INCLUDE_NONE
#include "GLFW/glfw3.h"

#define GLAD_GL_IMPLEMENTATION
#include "glad/gl.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "raymath.c"

#define CYE_IMPLEMENTATION
#undef assert
#undef unreachable
#include "cye.h"

#include "stb_c_lexer.c"

#include "shader.c"
#include "texture.c"
#include "buffer.c"
#include "renderer.c"

#include "assets/all_obj.h"

// Macro to define a mesh from OBJ data
#define DEFINE_MESH(prefix, ext)                          \
   static Mesh prefix##_mesh = {                          \
       .vertices       = (Vector3*)prefix##_objVerts,     \
       .normals        = (Vector3*)prefix##_objNormals,   \
       .uvs            = (Vector2*)prefix##_objTexCoords, \
       .indices        = (u32*)prefix##_objIndexes,       \
                                                          \
       .vertices_count = prefix##_objVertsCount,          \
       .uvs_count      = prefix##_objTexCoordsCount,      \
       .normals_count  = prefix##_objNormalsCount,        \
       .indices_count  = prefix##_objIndexesCount         \
   };                                                     \
   static char *prefix##_texture_path = "res/textures/" #prefix ext


DEFINE_MESH(bamboo, ".jpg");
DEFINE_MESH(enemy, ".png");
DEFINE_MESH(tiger, "_yellow.png");
DEFINE_MESH(horse, ".png");

static Mesh cube_mesh = {
   .vertices       = (Vector3*)cube_objVerts,
   .normals        = (Vector3*)cube_objNormals,
   .uvs            = (Vector2*)cube_objTexCoords,
   .indices        = (u32*)cube_objIndexes,

   .vertices_count = cube_objVertsCount,
   .uvs_count      = cube_objTexCoordsCount,
   .normals_count  = cube_objNormalsCount,
   .indices_count  = cube_objIndexesCount
};

// #define chosen_mesh bamboo_mesh
// #define chosen_texture_path bamboo_texture_path

// #define chosen_mesh horse_mesh
// #define chosen_texture_path horse_texture_path

#define chosen_mesh tiger_mesh
#define chosen_texture_path tiger_texture_path

// #define chosen_mesh enemy_mesh
// #define chosen_texture_path enemy_texture_path


typedef struct {
   Vector3 position;
   Vector3 rotation; // .x value is radians rotation around x-axis
   f32 zoom;
} Camera;

static Camera camera = {0};

bool camera_basis(const Camera *camera, Vector3 *out_right, Vector3 *out_up, Vector3 *out_forward);
void render_mesh_to_framebuffer(const Mesh *mesh) {
   static Vertex_Array va = {0};
   static Vertex_Array cube_va = {0};


#if 0
   static Vertex* gpu_vertices = nullptr;
   gpu_vertices = realloc(gpu_vertices, mesh->vertices_count * size_of(Vertex));
   for (u32 i = 0; i < mesh->vertices_count; ++i) {
      gpu_vertices[i].position_v3 = mesh->vertices[i];
      gpu_vertices[i].normal_v3 = mesh->normals[i];
      gpu_vertices[i].uv_v2 = mesh->uvs[i];
   }
   if (!is_valid_vertex_array(va)) {
      va = create_vertex_array(gpu_vertices, mesh->vertices_count, mesh->indices, mesh->indices_count);
   }

#else
   if (!is_valid_vertex_array(va)) {
      va = create_vertex_array_from_mesh(mesh);
   }
#endif

   if (!is_valid_vertex_array(cube_va)) {
      cube_va = create_vertex_array_from_mesh(&cube_mesh);
   }

   static Framebuffer fb = {0};
   if (!is_valid_framebuffer(fb)) {
      // fb = create_framebuffer(1600, 800);
      // fb = create_framebuffer_multisample(1600, 800, 16);
      fb = create_framebuffer_multisample_with_renderbuffers(1600, 800, 16);
   }

   static Uniform_Buffer ub = {0};
   if (!is_valid_buffer(ub.buffer)) {
      isz ub_binding = 2;
      ub = create_uniform_buffer(size_of(Matrix)*2, ub_binding);
   }
   ub.offset = 0; // reset for next frame


   static Shader shader = shader_invalid;
   static const char *shader_path = "res/shaders/default.glsl";
   bool want_reload = shader_needs_reload(shader);
   if (!is_valid_shader(shader) || want_reload) {
      if (is_valid_shader(shader) && want_reload) {
         shader = reload_shader(shader);
      } else {
         shader = create_shader(shader_path, 0);
      }
      if (!is_valid_shader(shader)) {
         // TODO: Load some default known to work shader program
         // shader = create_shader_from_vertex_and_fragment_memory(vs_src, fs_src);
      }

   }

   static Texture diffuse_texture = {0};
   if (!is_valid_texture(diffuse_texture)) {
      diffuse_texture = create_texture_from_filepath(chosen_texture_path);
   }

   static Texture cube_texture = {0};
   if (!is_valid_texture(cube_texture)) {
      cube_texture = create_texture_from_filepath("res/textures/ocean6.png");
   }

   // Step 4: Render setup
   glBindFramebuffer(GL_FRAMEBUFFER, fb.handle);
   glViewport(0, 0, fb.color.width, fb.color.height);


   {
      glEnable(GL_DEPTH_TEST);
      glClearColor(0.2f, 0.2f, 0.3f, 0.0f);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   }

   // Wireframe mode
   // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
   // back to its default using glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

   // Step 5: Set MVP (identity for simplicity)
   glUseProgram(shader.handle);

   {
      GLint view_location = glGetUniformLocation(shader.handle, "view");

      // Vector3 direction = spherical_to_cartesian((f32)glfwGetTime(), (f32)glfwGetTime() + PI/2.);
      Vector3 direction = spherical_to_cartesian(camera.rotation.x, camera.rotation.y);
      Matrix  view = MatrixLookAt((Vector3){0, 0, 0}, direction, (Vector3){0., 1., 0.});
      // printf("vec3(%f, %f, %f)\n", direction.x, direction.y, direction.z);
      // Matrix view = MatrixViewFromSpherical(camera.position, -camera.rotation.y, -camera.rotation.x);
      glUniformMatrix4fv(view_location, 1, GL_FALSE, MatrixToFloat(view));
   // Send to GPU
   }

   {  // Time uniform
      GLint loc = glGetUniformLocation(shader.handle, "u_time");
      glUniform1f(loc, (f32)glfwGetTime());
   }

   {
      GLint spherical_location = glGetUniformLocation(shader.handle, "spherical");
      glUniform2f(spherical_location, camera.rotation.y, camera.rotation.x);

      // GLint position_location = glGetUniformLocation(shader.handle, "camera_position");
      push_uniform(&ub, DATA_TYPE_VEC3, &camera.position, 1);
      update_buffer(ub.buffer, ub.cpu_mem, ub.offset, 0);
      // glUniform3f(position_location, camera.position.x, camera.position.y, camera.position.z);
   }

   {
      GLint loc = glGetUniformLocation(shader.handle, "perspective");
      Matrix perspective = MatrixPerspective(PI/3., (f64)fb.color.width/fb.color.height, 0.1, 100.0);
      // perspective.m11 *= -1; // Force to be "left-handed" just like the NDC
      // Matrix perspective = MatrixFrustum(-5., 5.,  -5., 5.,  -5., 5.);
      glUniformMatrix4fv(loc, 1, GL_FALSE, MatrixToFloat(perspective));
   }

   {
      Vector3 positions[] = {
         (Vector3){  0.0f,  0.0f,  0.0f  },
         (Vector3){  0.02f,  0.05f, -10.15f },
         (Vector3){ -1.5f, -2.2f, -2.5f  },
         (Vector3){ -3.8f, -2.0f, -12.3f },
         (Vector3){  2.4f, -0.4f, -3.5f  },
         (Vector3){ -1.7f,  3.0f, -7.5f  },
         (Vector3){  1.3f, -2.0f, -2.5f  },
         (Vector3){  1.5f,  2.0f, -2.5f  },
         (Vector3){  1.5f,  0.2f, -1.5f  },
         (Vector3){ -1.3f,  1.0f, -1.5f  }
      };



      static Storage_Buffer sb1 = {0};
      if (true) {
         isz binding = 3;
         isz offset = 0;
         isz size = mesh->vertices_count*size_of(*mesh->vertices);
         // bind_buffer_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding);
         // glBindBuffer(GL_SHADER_STORAGE_BUFFER, va.vb.buffer.handle);
         // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding, size, offset);
         // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, binding+1);

         if (sb1.buffer.size == 0) {
            trace_warn("initialzing storage_buffer\n");
            sb1 = create_storage_buffer(size, binding, mesh->vertices, false);
         }
      }

      static Storage_Buffer sb2 = {0};
      if (true) {
         isz binding = 5;
         isz offset = 0;
         isz size = mesh->indices_count*size_of(u32);
         // bind_buffer_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding);
         // glBindBuffer(GL_SHADER_STORAGE_BUFFER, va.vb.buffer.handle);
         // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, binding, size, offset);
         // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, binding+1);

         if (sb2.buffer.size == 0) {
            trace_warn("initialzing storage_buffer\n");
            assert(mesh->indices && mesh->indices_count > 0);
            sb2 = create_storage_buffer(size, binding, mesh->indices, false);
         }
      }

      glBindVertexArray(va.handle);
      glBindTextureUnit(4, diffuse_texture.handle); // matches binding = 4

      // bind_buffer_as_type(&va.ib.buffer, BUFFER_TYPE_STORAGE, 5);

      // void bind_buffer_slice_as_type(Buffer* buf, Buffer_Type type, isz binding, isz size, isz offset) {
      float pica = 69.f;

      static Buffer pica_buffer = {0};
      if (pica_buffer.handle == 0) {
         pica_buffer = create_buffer(&pica, size_of(pica));
      }

      // bind_buffer_slice_as_type(&pica_buffer, BUFFER_TYPE_STORAGE, 3, size_of(pica), 0);
      // bind_buffer_slice_as_type(&va.vb.buffer, BUFFER_TYPE_STORAGE, 3, mesh->vertices_count*size_of(*mesh->vertices), 0);
      bind_buffer_as_type(&sb1.buffer, BUFFER_TYPE_STORAGE, 3);
      bind_buffer_as_type(&sb2.buffer, BUFFER_TYPE_STORAGE, 5);

      GLint model_location = glGetUniformLocation(shader.handle, "model");
      for (isz i = 0; i < count_of(positions); i++) {
         if (9 == i ) {
            break;
         }

         Vector3 position = positions[i];
         // Matrix model = MatrixRotate((Vector3){0., (float)(i % 2 == 0)*1., 1.}, (f32)glfwGetTime() / 10.);
         Matrix model = MatrixRotate((Vector3){ 1., 1., 1.}, i);
         // model = MatrixMultiply(MatrixTranslate(position.x, position.y, position.z), model);
         model = MatrixMultiply(MatrixTranslate(i/2., 0., i/2.), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         assert(is_valid_vertex_array(va));
         glDrawElements(GL_TRIANGLES, va.ib.count, GL_UNSIGNED_INT, NULL);
      }


      if (true) {
         Matrix model = MatrixRotate((Vector3){ 1., 1., 1.}, PI/3.);
         // model = MatrixMultiply(MatrixTranslate(0, -0.50, 0), model);

         glUniformMatrix4fv(model_location, 1, GL_FALSE, MatrixToFloat(model));
         glBindTextureUnit(4, cube_texture.handle);
         bind_buffer_as_type(&cube_va.vb.buffer, BUFFER_TYPE_STORAGE, 3);

         glBindVertexArray(cube_va.handle);
         assert(is_valid_vertex_array(cube_va));
         glDrawElements(GL_TRIANGLES, cube_va.ib.count, GL_UNSIGNED_INT, NULL);
      }

   }

   // Step 7: Cleanup
   glBindVertexArray(0);
   glUseProgram(0);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);

   Rectanglei32 destination = {
      .x = 100,
      .y = 100,
      .width = 800, .height = 600
   };

   Framebuffer fb_resolved = fb;
   if (fb.color.samples > 1) {
      // compiler says possible undeifned is not use temp
      // Framebuffer fb_resolved = resolve_multisample_framebuffer_old(&fb);
      fb_resolved = resolve_multisample_framebuffer(fb);
      // blit_framebuffer_to_swapchain(fb_resolved);
   }
   blit_framebuffer_to_swapchain_rect(fb_resolved, destination);
}


Shader compute_shader = shader_invalid;
// static const char *compute_shader_path = "src/compute.glsl";
// static const char *compute_shader_path = "src/shaders/Tunnel-Cylinders.glsl";
static const char *compute_shader_path = "./src/shaders/shadertoy/base.glsl";

// TODO: Make the error be tracable throught aligning current line number with the shader file
// If is from glad
static void error_callback(int error, const char *description) {
   fprintf(stderr, "Error (%d): %s", error, description);
}

typedef struct {
   struct {
      const char *base;
      const char *sticky;
      const char *reload;
      const char *fps;
      const char *zero;
   };

   u8 mem[512];
} Window_Title;

static Window_Title title = {
   .base =  "ShaderToy",
   // These Should be empty string, not null
   .sticky = "",
   .reload = "",
   .fps = "",
   .zero = 0 // Mark the end
};


static f64 glfwGetScroll(GLFWwindow *window);


// CORRECTED: Camera basis calculation
bool camera_basis(const Camera *camera, Vector3 *out_right, Vector3 *out_up, Vector3 *out_forward) {
   if (!camera || !out_right || !out_up || !out_forward)
      return false;

   float pitch = camera->rotation.x; // Rotation around X-axis
   float yaw = camera->rotation.y;   // Rotation around Y-axis
   float roll = camera->rotation.z;  // Rotation around Z-axis

   // CORRECTED: Forward vector calculation (negative Z in OpenGL camera space)
   // This assumes yaw=0 points along negative Z, pitch=0 is level
   Vector3 forward = {
       -sinf(yaw) * cosf(pitch), // X component
       sinf(pitch),              // Y component
       -cosf(yaw) * cosf(pitch)  // Z component (negative Z forward)
   };
   forward = Vector3Normalize(forward);

   // World up vector
   Vector3 world_up = {0, 1, 0};

   // Check if forward is too aligned with world up
   float alignment = fabsf(Vector3DotProduct(forward, world_up));
   if (alignment >= 0.999f) {
      // Use alternative up vector when looking straight up/down
      Vector3 alt_up = {0, 0, 1}; // Use Z as alternative
      Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, alt_up));
      Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

      // Apply roll
      float cos_r = cosf(roll);
      float sin_r = sinf(roll);
      *out_right = Vector3Add(Vector3Scale(right, cos_r), Vector3Scale(up, sin_r));
      *out_up = Vector3CrossProduct(*out_right, forward);
      *out_up = Vector3Normalize(*out_up);
      *out_forward = forward;
      return true;
   }

   // Standard basis calculation
   Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, world_up));
   Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));

   // Apply roll rotation
   float cos_r = cosf(roll);
   float sin_r = sinf(roll);

   Vector3 right_rolled = Vector3Add(Vector3Scale(right, cos_r), Vector3Scale(up, sin_r));
   Vector3 up_rolled = Vector3Add(Vector3Scale(up, cos_r), Vector3Scale(right, -sin_r));

   *out_right = Vector3Normalize(right_rolled);
   *out_up = Vector3Normalize(up_rolled);
   *out_forward = forward;

   return true;
}

bool camera_basis2(const Camera *camera, Vector3 *out_right, Vector3 *out_up, Vector3 *out_forward) {
    if (!camera || !out_right || !out_up || !out_forward) return false;

    float pitch = camera->rotation.x;
    float yaw   = camera->rotation.y;
    float roll  = camera->rotation.z;
    // Step 1: Forward vector (Z axis)
    Vector3 forward = {
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw)
    };
    forward = Vector3Normalize(forward);

    // Base world up
    Vector3 world_up = {0, 1, 0};

    // Compute right and up from world_up (before roll)
    float alignment = fabsf(Vector3DotProduct(forward, world_up));
    if (alignment >= 0.999f) {
        return false; // Can't resolve stable basis if aligned
    }

    Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, world_up));
    Vector3 up    = Vector3Normalize(Vector3CrossProduct(right, forward));

    // Step 4: Apply roll to right and up
    float cos_r = cosf(roll);
    float sin_r = sinf(roll);

    Vector3 up_rolled = Vector3Add(Vector3Scale(up, cos_r), Vector3Scale(right, sin_r));
    Vector3 right_rolled = Vector3CrossProduct(forward, up_rolled); // Keep orthogonality

    *out_forward = forward;
    *out_right   = Vector3Normalize(right_rolled);
    *out_up      = Vector3Normalize(up_rolled);

    return true;
}

Camera move_camera(GLFWwindow *window, Camera cam) {

   ////////////////////////
   //// Rotation //////////
   ////////////////////////
   static bool mouse_button_left_down = false;
   static Vector2 mouse_last_position = { .x = -1.0f, .y = -1.0f };

   int mouse_left = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
   f64 mouse_x, mouse_y;
   glfwGetCursorPos(window, &mouse_x, &mouse_y);
   const f32 sensitivity = 60.0;
   f32 sensitivity_factor = Remap(sensitivity, 0.0f, 100.0f, 0.0059f, 0.0009f);

   if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_4) == GLFW_PRESS) {
      printf("Mouse Buttom 4 pressed\n");
   }

   if (mouse_left == GLFW_PRESS) {
      if (!mouse_button_left_down) {
         // First time pressing the button, store the last position
         mouse_button_left_down = true;
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      } else {
         // Calculate the mouse movement
         f32 delta_x = (f32)(mouse_x - mouse_last_position.x);
         f32 delta_y = (f32)(mouse_y - mouse_last_position.y);

         // Update camera rotation based on mouse movement
         cam.rotation.y -= delta_x * sensitivity_factor;
         cam.rotation.x -= delta_y * sensitivity_factor;

         // Clamp the vertical rotation to prevent flipping
         if (cam.rotation.x > DEG2RAD * (89.0f))
            cam.rotation.x = DEG2RAD * (89.0f);
         if (cam.rotation.x < DEG2RAD * (-89.0f))
            cam.rotation.x = DEG2RAD * (-89.0f);

         // Update the last mouse position
         mouse_last_position.x = (f32)mouse_x;
         mouse_last_position.y = (f32)mouse_y;
      }
   } else if (mouse_left == GLFW_RELEASE) {
      mouse_button_left_down = false;
   }

   ////////////////////////
   //// Position //////////
   ////////////////////////

   Vector3 v;
   v.x = 0;
   v.y = 0;
   v.z = 0;

   Vector3 forward = Vector3RotateByAxisAngle((Vector3){0., 0., 1.}, (Vector3){0., 1., 0.}, -cam.rotation.y);
   Vector3 right   = Vector3CrossProduct(forward, (Vector3){0., 1., 0.});
   forward = Vector3Normalize(forward);
   right   = Vector3Normalize(right);


   const f32 c = 0.14;
   if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
      v = Vector3Add(v, forward);
   }

   if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
      v = Vector3Subtract(v, forward);
   }

   if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
      v = Vector3Add(v, right);
   }

   if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
      v = Vector3Subtract(v, right);
   }

   v = Vector3Normalize(v);
   v = Vector3Scale(v, c);

   cam.position = Vector3Add(cam.position, v);

   f64 yoffset = glfwGetScroll(window);
   if (yoffset != 0.0) {
      cam.zoom = 1+yoffset;
   }

   if (cam.zoom < 1) {
      cam.zoom = 1;
      yoffset = 0;
   }


   return cam;
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
   if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
      // glfwSetWindowShouldClose(window, GLFW_TRUE);
   }

   if (key == GLFW_KEY_R && action == GLFW_PRESS) {
      Shader old = compute_shader;
      compute_shader = reload_shader(compute_shader);
      printf("reloaded and its broken ? %s\n", INVALID_SHADER_HANDLE == compute_shader.handle ? "yes" : "no");
      if (old.handle == compute_shader.handle) {
         title.reload = "(reload failed)";
      } else {
         title.reload = "";
      }
   }

   if (key == GLFW_KEY_C && action == GLFW_RELEASE) {
      bool sticky = glfwGetWindowAttrib(window, GLFW_FLOATING);
      glfwSetWindowAttrib(window, GLFW_FLOATING, !sticky);

   }
}

static bool mouse_right_pressed = false;
static bool mouse_left_pressed = false;
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
   if (button == GLFW_MOUSE_BUTTON_LEFT) {
      if (action == GLFW_PRESS) {
         mouse_left_pressed = true;
      } else if (action == GLFW_RELEASE) {
         mouse_left_pressed = false;
      }
   }

   if (button == GLFW_MOUSE_BUTTON_RIGHT) {
      if (action == GLFW_PRESS) {
         mouse_right_pressed = true;
      } else if (action == GLFW_RELEASE) {
         mouse_right_pressed = false;
      }
   }
}

static void conditionally_change_windows_title(GLFWwindow *window, f64 dt) {
   static char fps[512];
   if (dt > 0) {
      int written = snprintf(fps, (sizeof fps / sizeof fps[0]),"%.2f", 1./dt);
      title.fps = fps;
   }
   bool sticky = glfwGetWindowAttrib(window, GLFW_FLOATING);
   if (sticky) {
      title.sticky = "*sticky";
   } else {
      title.sticky = "";
   }

   const char **curr = &title.base;
   usz count = 0;
   isz max = (sizeof title.mem / sizeof title.mem[0]);
   const char* fmt = "%s ";
   while (*curr) {
      // If last don't add the space
      if (NULL == *(curr + 1)) {
         fmt = "%s";
      }
      count += snprintf((char*)title.mem + count, max - count, fmt, *curr);
      curr++;
   }
   const char *new_title = (const char*)title.mem;

   const char* current_title =  glfwGetWindowTitle(window);
   bool please_update =  0 == strcmp(new_title,  current_title);
   if (!please_update) {
      glfwSetWindowTitle(window, new_title);
   }
}

static f64 scroll_offset = 0.0;
static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
   scroll_offset += yoffset;
   // printf("xoffset=%f yoffset=%f\n", xoffset, yoffset);
}

static f64 glfwGetScroll(GLFWwindow *window) {
   return scroll_offset;
}

int main() {
   glfwSetErrorCallback(error_callback);

   if (!glfwInit())
      exit(EXIT_FAILURE);

   {  // open gl hints
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
      glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
   }

   glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

   GLFWmonitor *monitor = glfwGetPrimaryMonitor();
   const GLFWvidmode *mode = glfwGetVideoMode(monitor);

   // Get the maximum resolution
   const int max_width = mode->width;
   const int max_height = mode->height;

   int window_width  = max_width / 3.5;                // Half the width of the screen
   int window_height = max_height / 1.6;               // Half the height of the screen

   int right_padding_from_windows_bar = 67;
   int window_x = max_width - window_width - right_padding_from_windows_bar;  // 3/4 from the left
   int window_y = (max_height - window_height) / 2;                      // Centered vertically

   // int width = 1280;
   // int height = 720;

   GLFWwindow *window = glfwCreateWindow(window_width, window_height, title.base, NULL, NULL);
   if (!window) {
      glfwTerminate();
      exit(EXIT_FAILURE);
   }

   glfwSetWindowAttrib(window, GLFW_FLOATING, false); // sticky
   glfwSetWindowPos(window, window_x, window_y);
   // GLFW_CURSOR_HIDDEN GLFW_CURSOR_NORMAL GLFW_CURSOR_DISABLED(fps style) GLFW_CURSOR_CAPTURED(Won't be able to leave window) GLFW_CURSOR_DISABLED
   glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);



   glfwSetKeyCallback(window, key_callback);
   glfwSetScrollCallback(window, scroll_callback);
   glfwSetMouseButtonCallback(window, mouse_button_callback);


   glfwMakeContextCurrent(window);
   gladLoadGL(glfwGetProcAddress);
   glfwSwapInterval(1);

   int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
   if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
      enable_error_report();
   }

   compute_shader = create_shader(compute_shader_path, COMPUTE_SHADER);
   if (INVALID_SHADER_HANDLE == compute_shader.handle) {
      fprintf(stderr, "Compute shader failed. Fix it and press 'R' to reload.\n");
   }

   Texture compute_shader_texture = create_texture(window_width, window_height);
   Framebuffer fb = create_framebuffer_from_texture(compute_shader_texture);
   f64 start_time = glfwGetTime();

   f64 conditionally_change_windows_title_timer_default = 0.75;
   f64 conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;

   f64 shader_needs_reload_timer_default = 1.1; // Seconds
   f64 shader_needs_reload_timer = shader_needs_reload_timer_default;

   f64 previous_time = glfwGetTime();
   Camera camera_default = (Camera){
       .position = cliteral(Vector3){.x = 0, .y = 3.0, .z = -10.0},
       .rotation = cliteral(Vector3){.x = 0, .y = 0.0, .z =  0.0 },
       .zoom = 1.0f
   };

   camera = camera_default;

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
      glFrontFace(GL_CCW);           // GL_CCW to define front faces as counter-clockwise
   }

   while (!glfwWindowShouldClose(window)) {
      bool window_minized = glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
      f64 current_time = glfwGetTime();
      f64 delta_time = current_time - previous_time; // Time since last frame

      f64 elapsed_time = (f32)(current_time - start_time); // Total time since start

      // TODO: Timed operations struct instead
      conditionally_change_windows_title_timer -= delta_time;
      shader_needs_reload_timer -= delta_time;

      if (conditionally_change_windows_title_timer <= 0) {
         conditionally_change_windows_title(window, delta_time);
         conditionally_change_windows_title_timer = conditionally_change_windows_title_timer_default;
      }

      if (shader_needs_reload_timer <= 0) {
         if (shader_needs_reload(compute_shader)) {
            Shader old = compute_shader;
            compute_shader = reload_shader(compute_shader);
            if (old.handle == compute_shader.handle) {
               title.reload = "(reload failed)";
            } else {
               title.reload = "";
            }
         }
         shader_needs_reload_timer = shader_needs_reload_timer_default;
      }

      glfwGetFramebufferSize(window, &window_width, &window_height);

      // Resize texture only if need and is not minimized
      if ((window_width != compute_shader_texture.width
         || window_height != compute_shader_texture.height)
         && !window_minized)
      {
         glDeleteTextures(1, &compute_shader_texture.handle);
         compute_shader_texture = create_texture(window_width, window_height);
         attach_texture_to_framebuffer(&fb, compute_shader_texture);
      }

      // Camera GO!
      camera = move_camera(window, camera);

      if (INVALID_SHADER_HANDLE != compute_shader.handle) {
         glUseProgram(compute_shader.handle);

         {  // Time uniform
            GLint loc = glGetUniformLocation(compute_shader.handle, "iTime");
            glUniform1f(loc, (f32)elapsed_time);
         }


         {  // Resolution uniform
            GLint loc = glGetUniformLocation(compute_shader.handle, "iResolution");
            glUniform3f(
               loc,
               (f32)compute_shader_texture.width, (f32)compute_shader_texture.height,
               compute_shader_texture.width/(f32)compute_shader_texture.height
            );
         }

         {  // Position uniform
            GLint loc = glGetUniformLocation(compute_shader.handle, "iPosition");
            glUniform3f(
               loc, camera.position.x, camera.position.y, camera.position.z
            );
         }

         {  // Position uniform
            GLint loc = glGetUniformLocation(compute_shader.handle, "iRotation");
            glUniform3f(
               loc, camera.rotation.x, camera.rotation.y, camera.rotation.z
            );
         }

         {  // Position uniform
            GLint loc = glGetUniformLocation(compute_shader.handle, "iZoom");
            glUniform1f(
               loc, camera.zoom
            );
         }


         {  // Mouse uniform
            GLint mouse_loc = glGetUniformLocation(compute_shader.handle, "iMouse");
            f64 mouse_x, mouse_y;
            glfwGetCursorPos(window, &mouse_x, &mouse_y);
            glUniform4f(
               mouse_loc,
               (f32)mouse_x, (f32)mouse_y,
               mouse_left_pressed ? 1.f : 0.0f,
               mouse_right_pressed ? 1.f : 0.0f
            );
         }

         glBindImageTexture(0, fb.color.handle, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

         const GLuint work_group_size = 16;
         const GLuint work_group_size_x = work_group_size;
         const GLuint work_group_size_y = work_group_size;

         GLuint num_groups_x = (compute_shader_texture.width + work_group_size_x - 1) / work_group_size_x;
         GLuint num_groups_y = (compute_shader_texture.height + work_group_size_y - 1) / work_group_size_y;

         glDispatchCompute(num_groups_x, num_groups_y, 1);

         // Ensure all writes to the image are complete
         glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
      }

      // Only blit if windows is not minimized
      if (!window_minized) {
         blit_framebuffer_to_swapchain(fb);
         render_mesh_to_framebuffer(&chosen_mesh);
         // blend_framebuffers();
      }

      glfwSwapBuffers(window);
      glfwPollEvents();
      previous_time = current_time;
   }

   glfwDestroyWindow(window);

   glfwTerminate();
}

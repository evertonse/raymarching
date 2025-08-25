#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include "./stb_c_lexer.c"

#define INVALID_SHADER_HANDLE U32_MAX
#define INVALID_SHADER_TYPE  U32_MAX


#define MAX_SHADER_TYPES 3
#define COMPUTE_SHADER  GL_COMPUTE_SHADER
#define FRAGMENT_SHADER GL_FRAGMENT_SHADER
#define VERTEX_SHADER   GL_VERTEX_SHADER

typedef enum {
   SHADER_TYPE_UNDEFINED  = 0,
   SHADER_TYPE_COMPUTE    = GL_COMPUTE_SHADER,
   SHADER_TYPE_FRAGMENT   = GL_FRAGMENT_SHADER,
   SHADER_TYPE_VERTEX     = GL_VERTEX_SHADER
} Shader_Type;

typedef struct {
   GLuint handle;
   Shader_Type type;
   ZString path;
} Shader;

constexpr static Shader shader_invalid = {
   .handle = INVALID_SHADER_HANDLE,
   .type   = INVALID_SHADER_HANDLE,
   .path   = NULL
};

typedef DArray(isz) Isz_DArray;

#define MAX_SHADERS 256
static Isz_DArray shader_to_paths[MAX_SHADERS] = {0};

#undef read_file
#define read_file(x) (char*)cye_read_file(x)

static DString all_unique_paths = {0};

// Returns the start of added string or start of equal but already existing one.
static isz append_unique_path(ZString path) {
   isz count = 0;
   assert_msg(all_unique_paths.count < U32_MAX, "Comparing with count as signed");

   while (count < (isz)all_unique_paths.count) {
      char* curr_path = (char*)all_unique_paths.data + count;
      usz len = strlen(curr_path);
      assert_msg(len < I16_MAX, "Overflow might happens here and we're geting close in this case");
      if (strcmp(path, curr_path) == 0) {
         return count;
      }
      count += (isz)len + 1;
   }

   ds_write_buf(&all_unique_paths, path, strlen(path));
   ds_write_zero(&all_unique_paths);
   return count;
}

static void print_unique_paths(void) {
   usz count = 0;
   while (count < all_unique_paths.count) {
      char* curr_path = (char*)all_unique_paths.data + count;
      usz len = strlen(curr_path);
      count += len + 1;
      trace_debug("\n%s ", curr_path);
   }
}

// i64 *offset_* gets filled with the offset to the dynamic string data buffer for that type. It gets detected from reading #pragma type
static bool pre_process_shader(const char *path, DString *ds, Isz_DArray *path_offets, i64 *offset_compute, i64 *offset_fragment, i64 *offset_vertex) {
   static ZString shader_prefix_defines = R"(
      #ifndef lerp
         #define lerp mix
      #endif

      #ifndef PI
         #define PI 3.14159265358979323846
      #endif

      #ifndef TAU
         #define TAU PI * 2.
      #endif

      #ifndef EPSILON
         #define EPSILON 0.000001
      #endif

      #ifndef DEG2RAD
         #define DEG2RAD (PI/180.0)
      #endif

      #ifndef RAD2DEG
         #define RAD2DEG (180.0/PI)
      #endif
   )";

   char *source = read_file(path);
   if (!source) {
      return false;
   }
   isz string_offset_in_buffer = append_unique_path(path);
   da_append(path_offets, string_offset_in_buffer);

   stb_lexer lexer = {0};
   char store[8192] = {0}; // WARNING: @Big Max possible path string in #include that we can read
   assert((sizeof store / sizeof store[0]) == 8192);
   stb_c_lexer_init(&lexer, source, source + strlen(source), store, (sizeof store / sizeof store[0]));

   char *start = lexer.parse_point;
   char *end   = lexer.parse_point;
   while (stb_c_lexer_get_token(&lexer)) {
      if (lexer.token == '#' && stb_c_lexer_get_token(&lexer)) {
         if (lexer.token == CLEX_id && strcmp(lexer.string, "include") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_dqstring) {
               // On double quoted token the inside string (without quote) is stored at lexer.string
               const char *include_path = lexer.string;
               ds_write_buf(ds, start, end-start);
               start = lexer.parse_point; end = start;
               ds_write(ds, "\n"); // More readable in case of outputting to a file

               const char* resolved_path = nullptr;

               {
                  if (strlen(lexer.string) >= 2 && include_path[0] == '.' && include_path[1] == PATH_SEPARATOR_CHAR) {
                     resolved_path = tprintf("%s%s", path_dir_of(path), &include_path[1]);
                     trace_debug("Realtive path from #include = %s", resolved_path);
                     if (!file_exists(resolved_path)) { // @REMOVEME
                        debug_break();
                     }
                  } else {
                     resolved_path = include_path;
                  }
               }

               if (!pre_process_shader(resolved_path, ds, path_offets, offset_compute, offset_fragment, offset_vertex)) {
                  assert_msg(false, "TODO handle pre_process_shader failure");
                  return false;
               }
            }
         } else if (lexer.token == CLEX_id && strcmp(lexer.string, "version") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token != CLEX_intlit) {
               assert_msg(false, "After #version everything should be a integer");
               return false;
            }

            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_id && strcmp(lexer.string, "core") != 0) {
               assert_msg(false, "Only core version allowed, but we got %s instead", lexer.string);
               return false;
            }

            ds_write_buf(ds, start, lexer.parse_point-start);
            start = lexer.parse_point; end = start;
            ds_write(ds, shader_prefix_defines);

         } else if (lexer.token == CLEX_id && strcmp(lexer.string, "pragma") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_id) {
               if (strcmp(lexer.string, "fragment") == 0) {
                  if (offset_fragment == nullptr || -1 != *offset_fragment) {
                     return false;
                  } else {
                     ds_write_buf(ds, start, end-start);
                     start = lexer.parse_point; end = start;
                     if (ds->count > 0) {
                        ds_write_zero(ds);
                     }
                     trace_info("fragment count %d", ds->count);
                     *offset_fragment = ds->count;
                  }
               } else if (strcmp(lexer.string, "vertex") == 0) {
                  if (offset_vertex == nullptr || -1 != *offset_vertex) {
                     return false;
                  } else {
                     ds_write_buf(ds, start, end-start);
                     start = lexer.parse_point; end = start;
                     if (ds->count > 0) {
                        ds_write_zero(ds);
                     }
                     trace_info("vertex count %d", ds->count);
                     *offset_vertex = ds->count;
                  }
               }
            }
         }
      }
      end = lexer.parse_point;
   }
   ds_write_buf(ds, start, end-start);
   free(source);
   return true;
}

inline bool is_valid_shader(Shader shader) {
   return INVALID_SHADER_HANDLE != shader.handle;
   // TODO: Should we compare with 0 too? Also why does this break preprocess with seemingly unrelated error:?
   // Failed to open file: res/shaders/buffers/buffers/positions_xyz.glsl
   // src/renderer/./shader.c:146: Assertion Failure: `false` TODO handle pre_process_shader failure
   // return INVALID_SHADER_HANDLE != shader.handle && shader.handle != 0;
}

Shader create_shader_from_memory(const u8** sources, const Shader_Type* types, usize count) {
   Shader shader = shader_invalid;

   if (nullptr == sources || nullptr == types || count == 0) {
      fprintf(stderr, "Invalid shader input arrays.\n");
      return shader;
   }

   GLuint program = glCreateProgram();
   GLuint compiled_shaders[8] = {0}; // supports up to 8 stages; expand if needed

   for (usz i = 0; i < count; ++i) {
      const u8* src = sources[i];
      Shader_Type type = types[i];

      if (nullptr == src || type == INVALID_SHADER_TYPE) {
         fprintf(stderr, "Null shader source or invalid type at index %zu.\n", i);
         continue;
      }

      GLuint shader_handle = glCreateShader(type);
      glShaderSource(shader_handle, 1, (const GLchar**)&src, NULL);
      glCompileShader(shader_handle);

      GLint compiled = 0;
      glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &compiled);
      if (compiled == GL_FALSE) {
         GLint log_length = 0;
         glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &log_length);

         char* log = (char*)malloc(log_length);
         glGetShaderInfoLog(shader_handle, log_length, NULL, log);
         // TODO: Make this error line take into acount the include files
         trace_error("Shader compile error (type %u):\nOpenGL says: %s\n", type, log);
         free(log);

         glDeleteShader(shader_handle);
         continue;
      }

      glAttachShader(program, shader_handle);
      compiled_shaders[i] = shader_handle;
   }

   glLinkProgram(program);

   GLint linked = 0;
   glGetProgramiv(program, GL_LINK_STATUS, &linked);
   if (linked == GL_FALSE) {
      GLint log_length = 256;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

      if (0 != log_length) {
         char* log = (char*)malloc(log_length);
         glGetProgramInfoLog(program, log_length, NULL, log);
         fprintf(stderr, "Shader link error:\n%s(log_length=%d)\n", log, log_length);
         free(log);
      }

      glDeleteProgram(program);
      for (usz i = 0; i < count; ++i) {
         if (compiled_shaders[i])
            glDeleteShader(compiled_shaders[i]);
      }

      return shader;
   }

   // Cleanup attached shaders after linking
   for (usz i = 0; i < count; ++i) {
      if (compiled_shaders[i]) {
         glDetachShader(program, compiled_shaders[i]);
         glDeleteShader(compiled_shaders[i]);
      }
   }

   shader.handle = program;
   shader.type = 0; // could store bitfield of stages if needed
   shader.path = NULL; // optional: track which file(s) generated this

   return shader;
}

Shader create_shader_single_from_memory(u8* source, Shader_Type type) {
   const u8 *sources[] = {source};
   const Shader_Type types[] = {type};
   assert(count_of(sources) == count_of(types));

   Shader result = create_shader_from_memory(sources, types, count_of(sources));
   result.type = type;
   return result;
}

// Create and preprocess and compile the shader
Shader create_shader(const char* path, Shader_Type type) {
   DString ds = {0};
   Isz_DArray path_offsets = {0};
   Shader result = shader_invalid;
   i64 offset_compute = -1, offset_fragment = -1, offset_vertex = -1;
   // WARNING: We don't detect cyclic includes. #include "a" in b and #include "b" in a will not halt the programa and loop fo'ever

   auto checkpoint = tsave();
   if (pre_process_shader(path, &ds, &path_offsets, &offset_compute, &offset_fragment, &offset_vertex)) {
      ds_write_zero(&ds);

      Shader_Type types[MAX_SHADER_TYPES] = {0};
      const u8* sources[MAX_SHADER_TYPES] = {0};
      usz count = 0;

      if (offset_compute != -1) {
         sources[count] = &ds.data[offset_compute];
         types[count] = COMPUTE_SHADER;
         count += 1;
      }

      if (offset_fragment != -1) {
         sources[count] = &ds.data[offset_fragment];
         types[count] = FRAGMENT_SHADER;
         count += 1;
      }

      if (offset_vertex != -1) {
         sources[count] = &ds.data[offset_vertex];
         types[count] = VERTEX_SHADER;
         count += 1;
      }

      trace_info("offset_compute = %d, offset_fragment = %d, offset_vertex = %d\n", offset_compute, offset_fragment, offset_vertex);

      // No type detected from pre_process at all
      if (-1 == offset_compute && -1 == offset_fragment && -1 == offset_vertex) {
         result = create_shader_single_from_memory(ds.data, type);
      } else {
         for (size_t i = 0; i < count; i++) {
            make_dirs("src/assets/shaders/output/ignore/");
            write_file(tprintf("src/assets/shaders/output/ignore/(%d)type-%d.glsl", count, types[i]), (ZString)sources[i], strlen((ZString)sources[i]));
         }
         result = create_shader_from_memory(sources, types, count);
      }

      result.path = path;
   }
   trestore(checkpoint);


   if (INVALID_SHADER_HANDLE != result.handle) {
      usz checkpoint = tsave();
      {
         TString time_path = tprintf("%s.time", path_stem(path));
         String_Slice msg = ss_from_zstr("This file is just to mark time_t when the shader was compiled");
         write_file(time_path , msg.data, msg.size);
      }
      trestore(checkpoint);

      write_file("src/assets/shaders/output/success-dump.glsl", ds.data, ds.size);
      shader_to_paths[result.handle] =  path_offsets;

   } else {
       write_file("src/assets/shaders/output/failed-dump.glsl", ds.data, ds.size);
       // Only free on failure because we're gonna use the paths if all succeeds.
       da_free(path_offsets);
   }

   // Always free the dynamic array, under success or failure.
   ds_free(ds);
   return result;
}


Shader create_shader_single_from_memory_old(u8* source, Shader_Type type) {
   Shader shader = shader_invalid;
   shader.type = type;
   GLuint shader_handle = glCreateShader(type);
   glShaderSource(shader_handle, 1, (const GLchar**)&source, NULL);
   glCompileShader(shader_handle);

   // Check for compilation errors
   GLint is_compiled = 0;
   glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &is_compiled);
   if (GL_FALSE == is_compiled) {
      GLint max_length = 0;
      glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &max_length);

      // The info log is a string
      char* info_log = (char*)malloc(max_length);
      glGetShaderInfoLog(shader_handle, max_length, &max_length, info_log);

      fprintf(stderr, "%s\n", info_log);
      free(info_log);
      glDeleteShader(shader_handle);
      return shader;
   }

   // Create and link the program
   GLuint program = glCreateProgram();
   glAttachShader(program, shader_handle);
   glLinkProgram(program);

   // Check for linking errors
   GLint is_linked = 0;
   glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
   if (GL_FALSE == is_linked) {
      GLint max_length = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

      char* info_log = (char*)malloc(max_length);
      glGetProgramInfoLog(program, max_length, &max_length, info_log);

      fprintf(stderr, "%s\n", info_log);
      free(info_log);
      glDeleteProgram(program);
      glDeleteShader(shader_handle);
      return shader;
   }

   // Clean up
   glDetachShader(program, shader_handle);
   shader.handle = program;
   return shader;
}

// TODO: Move this faster implementation to cye.h
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_MINGW)

int needs_rebuild_from_paths(ZString output_path, ZString *input_paths, usz input_paths_count) {
  WIN32_FILE_ATTRIBUTE_DATA out_attr;
  if (!GetFileAttributesExA(output_path, GetFileExInfoStandard, &out_attr)) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND)
      return 1;
    return -1;
  }
  ULONGLONG out_time =
      ((ULONGLONG)out_attr.ftLastWriteTime.dwHighDateTime << 32) |
      out_attr.ftLastWriteTime.dwLowDateTime;

  for (usz i = 0; i < input_paths_count; ++i) {
    WIN32_FILE_ATTRIBUTE_DATA in_attr;
    if (!GetFileAttributesExA(input_paths[i], GetFileExInfoStandard,
                              &in_attr)) {
      return -1;
    }
    ULONGLONG in_time =
        ((ULONGLONG)in_attr.ftLastWriteTime.dwHighDateTime << 32) |
        in_attr.ftLastWriteTime.dwLowDateTime;
    if (in_time > out_time)
      return 1;
  }

  return 0;
}

#elif defined(PLATFORM_LINUX)
int needs_rebuild_from_paths(ZString output_path, ZString *input_paths, usz input_paths_count) {
   struct stat out_stat;
   if (stat(output_path, &out_stat) < 0) {
       if (errno == ENOENT) return 1;
       return -1;
   }

   for (usz i = 0; i < input_paths_count; ++i) {
       struct stat in_stat;
       if (stat(input_paths[i], &in_stat) < 0) return -1;
       if (in_stat.st_mtime > out_stat.st_mtime) return 1;
   }

   return 0;
}
#else
#   error "Platform not supported"
#endif


// TODO: Keep only one version either 1 or 2, and test it on linux when the time comes (long way from now 2025-07-22)
int needs_rebuild_from_paths2(ZString output_path, ZString *input_paths, usz input_paths_count) {
// Output timestamp
#if defined(_WIN32)
  WIN32_FILE_ATTRIBUTE_DATA od;
  if (!GetFileAttributesExA(output_path, GetFileExInfoStandard, &od)) {
    if (GetLastError() == ERROR_FILE_NOT_FOUND)
      return 1;
    return -1;
  }
  FILETIME *ot = &od.ftLastWriteTime;
#else
  struct stat sb;
  if (stat(output_path, &sb) < 0) {
    if (errno == ENOENT)
      return 1;
    return -1;
  }
#endif

  for (usz i = 0; i < input_paths_count; i++) {
    ZString ip = input_paths[i];
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA id;
    if (!GetFileAttributesExA(ip, GetFileExInfoStandard, &id)) {
      return -1;
    }
    FILETIME *it = &id.ftLastWriteTime;
    // Compare 64-bit values
    if (*((unsigned long long *)it) > *((unsigned long long *)ot))
      return 1;
#else
    if (stat(ip, &sb) < 0)
      return -1;
    if (sb.st_mtime > (time_t)sb.st_mtime)
      return 1;
#endif
  }
  return 0;
}

// TODO: Mark time of compilation in the shader struct itself on top of .time files
bool shader_needs_reload(Shader shader) {
   GLuint shader_handle = shader.handle;
   if (INVALID_SHADER_HANDLE == shader_handle) {
      return false;
   }

   Isz_DArray paths = shader_to_paths[shader_handle];
   if (0 == paths.count) {
      return false;
   }

   ZString first_path = (char*)all_unique_paths.data + paths.items[0];
   usz count = 0;
   bool result = false;


   usz checkpoint = tsave();
   // We always save and .time files based on first_path
   TString time_path = tprintf("%s.time", path_stem(first_path));

#if 1
   ZString* resolved_paths = (ZString*)talloc(paths.count * size_of(ZString));
   for (usz i = 0; i < paths.count; i++) {
       resolved_paths[i] = (char*)all_unique_paths.data + paths.items[i];
   }

   if (needs_rebuild_from_paths(time_path, resolved_paths, paths.count)) {
      trace_debug("Yes, we need reload based on paths for: %s", time_path);
      return_defer(result = true);
   } else {
       trace_debug("No reload needed for: %s", time_path);
   }

#else
   begin_profile();
   for (usz idx = 0; idx < paths.count; idx++) {
      char* curr_path = (char*)all_unique_paths.data + paths.items[idx];
      if (needs_rebuild(time_path, curr_path)) {
         trace_debug("Yes we need reload, time_path=%s curr_path=%s", time_path, curr_path);
         return_defer(result = true);
      } else {
         trace_debug("No we don't need reload, time_path=%s curr_path=%s", time_path, curr_path);
      }
   }
   end_profile("after loop");
#endif

defer:
   trestore(checkpoint);
   return result;
}

Shader create_shader_from_vertex_and_fragment_memory(const char* vs_src, const char* fs_src) {
   const u8 *sources[] = { (const u8*)vs_src, (const u8*)fs_src};
   const Shader_Type types[] = {
      GL_VERTEX_SHADER,
      GL_FRAGMENT_SHADER
   };
   assert(count_of(sources) == count_of(types));
   return create_shader_from_memory(sources, types, count_of(sources));
}

Shader reload_shader(Shader shader) {
   system("clear"); // HACK XXX: Trying to clear the whole terminal to not flood with erros
   Shader new_shader = create_shader(shader.path, shader.type);


   // Keep current shader while errors in new shader
   if (INVALID_SHADER_HANDLE == new_shader.handle) {
      return shader;
   }

   // Only delete if shader was valid to begin with
   if (INVALID_SHADER_HANDLE != shader.handle) {
      glDeleteProgram(shader.handle);
   }
   return new_shader;
}

void bind_shader(Shader shader) {
   if (!is_valid_shader(shader)) {
      trace_error("Trying to bind an invalid shader!");
      return;
   }
   glUseProgram(shader.handle);
}

void dispatch_compute_shader(const Shader shader, u32 groups_x, u32 groups_y, u32 groups_z) {
    assert(is_valid_shader(shader));
    assert(shader.type == SHADER_TYPE_COMPUTE);
    glUseProgram(shader.handle);
    glDispatchCompute(groups_x, groups_y, groups_z);
}

void shader_image_acess_barrier() {
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
}


static GLint get_cached_uniform_location(GLuint program, const char* name) {
    // You can bump this up or make it dynamic if needed
    #define MAX_UNIFORM_CACHE 512
    typedef struct {
        GLuint program;
        const char* name;
        GLint location;
    } UniformCache;

    static UniformCache cache[MAX_UNIFORM_CACHE];
    static int count = 0;

    for (int i = 0; i < count; ++i) {
        if (cache[i].program == program && strcmp(cache[i].name, name) == 0) {
            return cache[i].location;
        }
    }

    GLint location = glGetUniformLocation(program, name);
    if (count < MAX_UNIFORM_CACHE) {
        cache[count++] = (UniformCache){ program, name, location };
    }

    return location;
}

void upload_uniform_mat4(const Shader shader, const char* name, const Matrix value) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, MatrixToFloat(value));
}

void upload_uniform_vec3(const Shader shader, const char* name, const Vector3 value) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform3f(loc, value.x, value.y, value.z);
}

void upload_uniform_vec4(const Shader shader, const char* name, const Vector4 value) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform4f(loc, value.x, value.y, value.z, value.w);
}

void upload_uniform_float(const Shader shader, const char* name, float value) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform1f(loc, value);
}

void upload_uniform_sampler2D(const Shader shader, const char* name, int binding) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform1i(loc, binding);
}

void upload_uniform_bool(const Shader shader, const char* name, bool value) {
    // TODO: Make something like this work GLint loc = get_cached_uniform_location(shader.handle, name);
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform1i(loc, value ? 1 : 0);
}

void upload_uniform_int(const Shader shader, const char* name, int value) {
    GLint loc = glGetUniformLocation(shader.handle, name);
    if (loc >= 0) glUniform1i(loc, value);
}

#include <assert.h>
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
   SHADER_TYPE_COMPUTE  = GL_COMPUTE_SHADER,
   SHADER_TYPE_FRAGMENT = GL_FRAGMENT_SHADER,
   SHADER_TYPE_VERTEX   = GL_VERTEX_SHADER
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

   stb_lexer lexer;
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

               if (!pre_process_shader(include_path, ds, path_offets, offset_compute, offset_fragment, offset_vertex)) {
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
         fprintf(stderr, "Shader compile error (type %u):\n%s\n", type, log);
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
   return create_shader_from_memory(sources, types, count_of(sources));
}

// Create and preprocess and compile the shader
Shader create_shader(const char* path, Shader_Type type) {
   DString ds = {0};
   Isz_DArray path_offsets = {0};
   Shader result = shader_invalid;
   i64 offset_compute = -1, offset_fragment = -1, offset_vertex = -1;
   // WARNING: We don't detect cyclic includes. #include "a" in b and #include "b" in a will not halt the programa and loop fo'ever
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
           write_file(tprintf("(%d)type-%d.glsl", count, types[i]), (ZString)sources[i], strlen((ZString)sources[i]));
         }
         result = create_shader_from_memory(sources, types, count);
      }

      result.path = path;
   }


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

// TODO: Mark time of compilation in the shader itself on top of .time files
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

   for (usz idx = 0; idx < paths.count; idx++) {
      char* curr_path = (char*)all_unique_paths.data + paths.items[idx];
      if (needs_rebuild(time_path, curr_path)) {
         trace_debug("Yes we need reload, time_path=%s curr_path=%s", time_path, curr_path);
         return_defer(result = true);
      } else {
         trace_debug("No we don't need reload, time_path=%s curr_path=%s", time_path, curr_path);
      }
   }

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


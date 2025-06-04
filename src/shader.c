#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

uint32_t reload_compute_shader(uint32_t shader_handle, const char* path);
uint32_t create_compute_shader(const char* path);
uint32_t create_compute_shader_from_memory(u8* source);

uint8_t* read_entire_file_into_memory(const char* path);

typedef DArray(isz) Isz_DArray;




#define MAX_SHADERS 256
static Isz_DArray shader_to_paths[MAX_SHADERS] = {0};
static bool pre_process_shader(const char *path, DString *ds, Isz_DArray *path_offets);

#define INVALID_SHADER_HANDLE ((GLuint)-1)

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

static bool pre_process_shader(const char *path, DString *ds, Isz_DArray *path_offets) {
   char *source = read_file(path);
   if (!source) {
      return false;
   }
   isz string_offset_in_buffer = append_unique_path(path);
   da_append(path_offets, string_offset_in_buffer);

   stb_lexer lexer;
   char store[8192] = {0}; // Max possible path string in #include that we can read
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
               start = lexer.parse_point;
               end   = lexer.parse_point;
               ds_write(ds, "\n"); // More readable in case of outputting to a file

               if (!pre_process_shader(include_path, ds, path_offets)) {
                  assert_msg(false, "TODO handle pre_process_shader failure");
                  return false;
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


// Create and preprocess and compile the shader
u32 create_compute_shader(const char* path) {
    DString ds = {0};
    Isz_DArray path_offsets = {0};
    u32 result = INVALID_SHADER_HANDLE;

    // WARNING: We don't detect cyclic includes. #include "a" in b and #include "b" in a will halt the program
    if (pre_process_shader(path, &ds, &path_offsets)) {
        ds_write_zero(&ds);
        result = create_compute_shader_from_memory(ds.data);
    }


    if (INVALID_SHADER_HANDLE != result) {
       usz checkpoint = tsave();
       {
          TString time_path = tprintf("%s.time", path_stem(path));
          String_Slice msg = ss_from_zstr("This file is just to mark time_t when the shader was compiled");
          write_file(time_path , msg.data, msg.size);
       }
       trestore(checkpoint);

       write_file("src/shaders/output/success-dump.glsl", ds.data, ds.size);
       shader_to_paths[result] =  path_offsets;

    } else {
        write_file("src/shaders/output/failed-dump.glsl", ds.data, ds.size);
        // Only free on failure because we're gonna use the paths if all succeeds.
        da_free(path_offsets);
    }

    // Always free the dynamic array, under success or failure.
    ds_free(ds);
    return result;
}

// Returns INVALID_SHADER_HANDLE (-1) if error
// TODO: Allow passing size along with the string
uint32_t create_compute_shader_from_memory(u8* source) {
   GLuint shader_handle = glCreateShader(GL_COMPUTE_SHADER);
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
      return INVALID_SHADER_HANDLE;
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
      return INVALID_SHADER_HANDLE;
   }

   // Clean up
   glDetachShader(program, shader_handle);
   return program;
}

bool shader_needs_reload(uint32_t shader_handle) {
   if (INVALID_SHADER_HANDLE == shader_handle) {
      return false;
   }

   Isz_DArray paths = shader_to_paths[shader_handle];
   if (0 == paths.count) {
      return false;
   }

   ZString first_path = (char*)all_unique_paths.data + paths.items[0];
   usz count = 0;
   usz checkpoint = tsave();
   bool result = false;

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

uint32_t reload_compute_shader(uint32_t shader_handle, const char *path) {
   system("clear"); // HACK XXX

   Isz_DArray paths = shader_to_paths[shader_handle];
   trace_debug(da_fmt, da_fmt_arg(paths));

   for (usz idx = 0; idx < paths.count; ++idx) {
      trace_debug("%lld\n", paths.items[idx]);
   }
   print_unique_paths();

   uint32_t new_shader_handle = create_compute_shader(path);


   // Keep current shader while errors in new shader
   if (INVALID_SHADER_HANDLE == new_shader_handle) {
      return shader_handle;
   }

   // Only delete if shader was valid to begin with
   if (INVALID_SHADER_HANDLE != shader_handle) {
      glDeleteProgram(shader_handle);
   }
   return new_shader_handle;
}

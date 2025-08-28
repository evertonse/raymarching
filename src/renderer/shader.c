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
   .path   = nullptr
};

typedef DArray(isz) Isz_DArray;

#define MAX_SHADERS 256
static Isz_DArray shader_to_paths[MAX_SHADERS] = {0};

#undef read_file
#define read_file(x) (char*)cye_read_file(x)

static DString all_unique_paths = {0};

static struct {
   struct {
      ZString path; // Which Paths does this shader include considering the first path that we pass when we create
      isz line_number_into_full_block; // This is the first line of this path correspond to what file in the full block to be send to opengl, this considers even pre includes, this considers even pre includes.
      isz line_start;  // First line of this file in full block
      isz line_end;    // Last line of this file in full block
   } *items;
   isz count;
   isz capacity;
   isz fragment_line;  // Line where fragment shader starts (after #pragma fragment)
   isz vertex_line;    // Line where vertex shader starts (after #pragma vertex)
} shaders_metadata[MAX_SHADERS] = {0};

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

int index_shader_metadata(const char *path) {
   int free_slot = -1;
   for (int metadata_index = 0; metadata_index < count_of(shaders_metadata); metadata_index += 1) {
     auto metadata = shaders_metadata[metadata_index];
     // if (metadata.count > 0 && (0 == strcmp(path, metadata.items[0].path))) {
     if (metadata.count > 0 && path_equals(path, metadata.items[0].path)) {
       trace_debug("Found index %d metadata for %s", metadata_index, path);
       return metadata_index;
     }
     if (-1 == free_slot && 0 == metadata.count) {
       free_slot = metadata_index;
     }
   }
   assert_msg(-1 != free_slot, "We shouldn't really run out of this");
   return free_slot;
}


static void append_shader_metadata(int shader_index, const char *path, isz line_number) {
   if (shader_index >= MAX_SHADERS) {
      return;
   }

   // Ensure we have space
   if (shaders_metadata[shader_index].count >= shaders_metadata[shader_index].capacity) {
      shaders_metadata[shader_index].capacity = shaders_metadata[shader_index].capacity ? shaders_metadata[shader_index].capacity * 2 : 16;
      shaders_metadata[shader_index].items = (typeof(shaders_metadata[shader_index].items))realloc(shaders_metadata[shader_index].items, shaders_metadata[shader_index].capacity * size_of(*shaders_metadata[shader_index].items));
   }

   auto new_index = shaders_metadata[shader_index].count;
   shaders_metadata[shader_index].items[new_index].path = path;
   shaders_metadata[shader_index].items[new_index].line_number_into_full_block = line_number;
   shaders_metadata[shader_index].items[new_index].line_start = line_number;
   shaders_metadata[shader_index].count += 1;
}

void print_shader_metadata(int shader_index) {
   if (shader_index >= MAX_SHADERS) {
      return;
   }

   auto meta  = shaders_metadata[shader_index];
   DString ds = {0};


   isz fragment_line;  // Line where fragment shader starts (after #pragma fragment)
   isz vertex_line;    // Line where vertex shader starts (after #pragma vertex)
   ds_printf(&ds, "items = %p, count = %lld, capacity = %lld\nfragment_lines=%lld vertex_line=%lld", meta.items, (usz)meta.count, (usz)meta.capacity, (usz)meta.fragment_line, (usz)meta.vertex_line);
   for (int item_index = 0; item_index < meta.count; item_index += 1) {
      auto item = meta.items[item_index];
      ds_printf(&ds, "   path=%s line_number=%lld line_end=%lld\n",
         item.path,
         (usz)item.line_number_into_full_block,
         (usz)item.line_end
      );
   }
   ds_write_zero(&ds);

   assert_msg(CYE_MAX_TRACE_LOG_MSG_LENGTH > ds.count + 50,  "trace_info is not dyanmic alocated, it uses fixed buffer.");
   trace_info((char*)ds.items);
   // printf("\nprintf\n%s", ds.items);

   ds_free(ds);
}

static isz count_lines_in_string(const char* str, isz length) {
   isz lines = 0;
   for (isz i = 0; i < length; i++) {
      if (str[i] == '\n') lines++;
   }
   return lines;
}

static bool pre_process_shader_with_metadata(
   const char *path,        DString *ds,
   Isz_DArray *path_offets,
   i64 *offset_compute, i64 *offset_fragment,    i64 *offset_vertex,
   int shader_index,        isz *current_line_number
) {
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

   // Record this file's starting line
   isz file_start_line = *current_line_number;
   isz string_offset_in_buffer = append_unique_path(path);
   const char *stored_path = (const char *)(all_unique_paths.items + string_offset_in_buffer);

   // Add metadata entry with start line
   append_shader_metadata(shader_index, stored_path, file_start_line);
   da_append(path_offets, string_offset_in_buffer);
   isz metadata_index = shaders_metadata[shader_index].count - 1; // Last added entry

   stb_lexer lexer = {0};
   char store[8192] = {0};
   stb_c_lexer_init(&lexer, source, source + strlen(source), store, count_of(store));

   char *start = lexer.parse_point;
   char *end = lexer.parse_point;

   while (stb_c_lexer_get_token(&lexer)) {
      if (lexer.token == '#' && stb_c_lexer_get_token(&lexer)) {
         if (lexer.token == CLEX_id && strcmp(lexer.string, "include") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_dqstring) {
               const char *include_path = lexer.string;

               ds_write_buf(ds, start, end - start);
               *current_line_number += count_lines_in_string(start, end - start);

               start = lexer.parse_point;
               end = start;
               ds_write(ds, "\n");
               *current_line_number += 1;

               const char *resolved_path = nullptr;

               if (strlen(lexer.string) >= 2 && include_path[0] == '.' && include_path[1] == PATH_SEPARATOR_CHAR) {
                  resolved_path = tprintf("%s%s", path_dir_of(path), &include_path[1]);
                  trace_debug("Relative path from #include = %s", resolved_path);
                  if (!file_exists(resolved_path)) {
                     trace_error("Trying to #include \"%s\" that doesnt exist.", resolved_path);
                     return false;
                  }
               } else {
                  resolved_path = include_path;
               }

               if (!pre_process_shader_with_metadata(resolved_path, ds, path_offets, offset_compute, offset_fragment, offset_vertex, shader_index, current_line_number)) {
                  assert_msg(false, "TODO handle pre_process_shader failure");
                  return false;
               }

               ds_write(ds, "\n"); // Make sure no two paths has the same line_end number
               ds_write_buf(ds, start, lexer.parse_point - start);
               *current_line_number += 1;
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

            ds_write_buf(ds, start, lexer.parse_point - start);
            *current_line_number += count_lines_in_string(start, lexer.parse_point - start);

            start = lexer.parse_point; end = start;

            ds_write_buf(ds, shader_prefix_defines, strlen(shader_prefix_defines));
            *current_line_number += count_lines_in_string(shader_prefix_defines, strlen(shader_prefix_defines));

         } else if (lexer.token == CLEX_id && strcmp(lexer.string, "pragma") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_id) {
               if (strcmp(lexer.string, "fragment") == 0) {
                  if (offset_fragment == nullptr || -1 != *offset_fragment) {
                     return false;
                  } else {
                     ds_write_buf(ds, start, end - start);
                     *current_line_number += count_lines_in_string(start, end - start);

                     start = lexer.parse_point;
                     end = start;
                     if (ds->count > 0) {
                        ds_write_zero(ds);
                     }
                     shaders_metadata[shader_index].fragment_line = *current_line_number; // Record fragment start
                     trace_info("%s fragment count %d", __func__, ds->count);
                     *offset_fragment = ds->count;
                  }
               } else if (strcmp(lexer.string, "vertex") == 0) {
                  if (offset_vertex == nullptr || -1 != *offset_vertex) {
                     return false;
                  } else {
                     ds_write_buf(ds, start, end - start);
                     *current_line_number += count_lines_in_string(start, end - start);

                     start = lexer.parse_point;
                     end = start;
                     if (ds->count > 0) {
                        ds_write_zero(ds);
                     }
                     shaders_metadata[shader_index].vertex_line = *current_line_number; // Record vertex start
                     trace_info("%s vertex count %d offset_vertex = %p", __func__, ds->count, offset_vertex);
                     *offset_vertex = ds->count;
                  }
               }
            }
         }
      }
      end = lexer.parse_point;
   }

   ds_write_buf(ds, start, end - start);
   *current_line_number += count_lines_in_string(start, end - start);

   // Update this file's end line
   shaders_metadata[shader_index].items[metadata_index].line_end = *current_line_number - 1;

   free(source);
   return true;
}

static bool pre_process_shader(
   const char *path, DString *ds, Isz_DArray *path_offets,
   i64 *offset_compute, i64 *offset_fragment, i64 *offset_vertex
) {
   // Need to determine which shader index to use.
   // Prolly managed by the calling code. For now, assuming shader index 0
   int shader_index = index_shader_metadata(path);  // TODO: This should be passed as a parameter or determined somehow
   isz current_line = 1;

   // Clear existing metadata for this shader
   shaders_metadata[shader_index].count = 0;

   return pre_process_shader_with_metadata(path, ds, path_offets, offset_compute, offset_fragment, offset_vertex, shader_index, &current_line);
}

inline bool is_valid_shader(Shader shader) {
   // return INVALID_SHADER_HANDLE != shader.handle;
   // TODO: Should we compare with 0 too? Also why does this break preprocess with seemingly unrelated error:?
   // Failed to open file: res/shaders/buffers/buffers/positions_xyz.glsl
   // src/renderer/./shader.c:146: Assertion Failure: `false` TODO handle pre_process_shader failure
   return INVALID_SHADER_HANDLE != shader.handle && shader.handle != 0;
}

// static bool map_line_to_file(int shader_index, Shader_Type shader_type, isz global_line, const char **filepath, isz *local_line) {
//    if (shader_index >= MAX_SHADERS || shaders_metadata[shader_index].count <= 0) {
//       return false;
//    }
//    return false;
//
//    auto meta = shaders_metadata[shader_index].items[0];
//
//    isz meta_index_best = 0;
//
//    for (int meta_index = 1; meta_index < shaders_metadata[shader_index].count; meta_index += 1) {
//       auto item = shaders_metadata[shader_index].items[meta_index];
//       if (global_line >= item.line_start && global_line <= item.line_end) {
//          auto best = shaders_metadata[shader_index].items[meta_index_best];
//          // We can assume not tine line_starts nor line_ens are equals
//          if (item.line_start > best.line_start || item.line_end < best.line_end) {
//             meta_index_best = meta_index;
//          }
//       }
//    }
//
//    auto parent = shaders_metadata[shader_index].items[meta_index_best];
//    trace_okay("Best! for %lld", (usz)global_line);
//    trace_struct(parent);
//
//    auto list = shaders_metadata[shader_index];
//    if (meta_index_best <= (list.count -1)) {
//       *local_line = global_line - parent.line_start;
//       return true;
//    }
//
//    auto next = list.items[meta_index_best + 1];
//
//    // Between parent and first child or has no children
//    if (global_line <= next.line_start || parent.line_end < next.line_start) {
//       *local_line = global_line - parent.line_start;
//       return true;
//    }
//
//    isz lines = next.start_end - parent.line_start;
//    isz end   = next.line_end;
//    isz start = next.start_end;
//
//    lines += global_line - end;
//    return true;
//    for (int index = meta_index_best + 2; index < list.count; index += 1) {
//       if (start > parent.end_line) {
//          break;
//       }
//       auto item = list.items[index];
//    }
//
//    if (list.items[meta_index_best + 1].line_start >= list.items line_) {
//    }
//
//    return false;
// }

static bool map_line_to_file(int shader_index, Shader_Type shader_type, isz global_line, const char **filepath, isz *local_line) {
   if ((shader_index >= MAX_SHADERS) || (shaders_metadata[shader_index].count <= 0)) {
      return false;
   }

   auto *list = &shaders_metadata[shader_index];

   // Find the best matching file (the one that contains global_line with the tightest bounds)
   isz meta_index_best = 0;
   auto meta = list->items[0];
   bool found_match = false;

   // Check if first item contains the line
   if (global_line >= meta.line_start && global_line <= meta.line_end) {
      found_match = true;
   }

   for (int meta_index = 1; meta_index < list->count; meta_index += 1) {
      auto item = list->items[meta_index];

      if (global_line >= item.line_start && global_line <= item.line_end) {
         if (!found_match) {
            // First match found
            meta_index_best = meta_index;
            found_match = true;
         } else {
            auto best = list->items[meta_index_best];
            // Choose the file with tighter bounds (more specific range)
            if (item.line_start > best.line_start || (item.line_start == best.line_start && item.line_end < best.line_end)) {
               meta_index_best = meta_index;
            }
         }
      }
   }

   if (!found_match) {
      return false;
   }

   auto parent = list->items[meta_index_best];
   *filepath = parent.path;

   trace_okay("Best match for global line %lld: %s (range %lld-%lld)", (long long)global_line, parent.path, (long long)parent.line_start, (long long)parent.line_end);

   // If this is the last file or no children, simple calculation
   if (meta_index_best >= (list->count - 1)) {
      *local_line = global_line - parent.line_start + 1;
      return true;
   }

   // Calculate local line by subtracting included file lines
   isz calculated_local_line = 1; // Start at line 1 of the parent file
   isz current_global_line = parent.line_start;

   // Walk through the parent file, skipping over included files
   for (isz check_index = meta_index_best + 1; check_index < list->count; check_index++) {
      auto child = list->items[check_index];

      // Skip children that are not within this parent's range
      if (child.line_start < parent.line_start || child.line_end > parent.line_end) {
         continue;
      }

      // Skip nested children (children of children)
      bool is_nested_child = false;
      for (isz parent_check = meta_index_best + 1; parent_check < check_index; parent_check++) {
         auto potential_parent = list->items[parent_check];
         if (child.line_start >= potential_parent.line_start && child.line_end <= potential_parent.line_end && potential_parent.line_start >= parent.line_start && potential_parent.line_end <= parent.line_end) {
            is_nested_child = true;
            break;
         }
      }

      if (is_nested_child) {
         continue;
      }

      // If the target line is before this child starts
      if (global_line < child.line_start) {
         // Count lines from current position to target
         calculated_local_line += (global_line - current_global_line);
         *local_line = calculated_local_line;
         return true;
      }

      // If the target line is within this child, return immediately
      // (this shouldn't happen as we already found the best match)
      if (global_line >= child.line_start && global_line <= child.line_end) {
         *local_line = global_line - parent.line_start + 1;
         return true;
      }

      // Skip over the child's lines (they don't count in parent's local lines)
      // Add lines from current position to just before the child
      if (current_global_line < child.line_start) {
         calculated_local_line += (child.line_start - current_global_line);
      }

      // Move past the child
      current_global_line = child.line_end + 1;
   }

   // If we get here, the target line is after all children
   if (global_line >= current_global_line) {
      calculated_local_line += (global_line - current_global_line);
      *local_line = calculated_local_line;
      return true;
   }

   // Fallback: simple calculation
   *local_line = global_line - parent.line_start + 1;
   return true;
}

static void print_remapped_opengl_errors(const char *error_string, int shader_index, Shader_Type shader_type) {
   DString ds = {0};
   stb_lexer lex = {0};
   char store[512];
   stb_c_lexer_init(&lex, error_string, error_string + strlen(error_string), store, 512);

   const char *line_start = error_string;
   while (stb_c_lexer_get_token(&lex)) {
      if (lex.token == CLEX_intlit) {
         const char *before_num = line_start;

         if (stb_c_lexer_get_token(&lex) && lex.token == '(' && stb_c_lexer_get_token(&lex) && lex.token == CLEX_intlit) {

            long line_num = lex.int_number;

            if (stb_c_lexer_get_token(&lex) && lex.token == ')' && stb_c_lexer_get_token(&lex) && lex.token == ':') {

               ds_write_buf(&ds, before_num, lex.where_firstchar - before_num);

               const char *filepath = nullptr;
               isz local_line = -1;
               if (map_line_to_file(shader_index, shader_type, line_num, &filepath, &local_line)) {
                  ds_printf(&ds, "%s:%zu", filepath, local_line);
               } else {
                  ds_printf(&ds, "full_block:%ld", line_num);
               }

               const char *error_end = lex.parse_point;
               while (*error_end && *error_end != '\n' && *error_end != '\r')
                  error_end++;
               ds_printf(&ds, " :%.*s\n", (int)(error_end - lex.parse_point), lex.parse_point);

               while (*error_end && (*error_end == '\n' || *error_end == '\r')) {
                  error_end++;
               }
               line_start = error_end;
               if (*error_end) {
                  stb_c_lexer_init(&lex, error_end, error_string + strlen(error_string), store, 512);
               }
            }
         }
      }
   }

   if (line_start < error_string + strlen(error_string)) {
      ds_write_buf(&ds, line_start, (error_string + strlen(error_string)) - line_start);
   }

   if (ds.count) {
      ds_write_zero(&ds);
      trace_error("%s", ds.items);
   }
   ds_free(ds);
}


Shader create_shader_from_memory(const u8 **sources, const Shader_Type *types, usz count, const char *loaded_from_this_path) {
   Shader shader = shader_invalid;
   GLuint program = 0;
   GLuint compiled_shaders[8] = {0}; // supports up to 8 stages

   if (!sources || !types || count == 0 || count > count_of(compiled_shaders)) {
      trace_error("Invalid shader input arrays.\n");
      return shader;
   }

   program = glCreateProgram();
   if (!program) {
      trace_error("glCreateProgram failed.\n");
      return shader_invalid;
   }

   for (usz i = 0; i < count; ++i) {
      const u8 *src = sources[i];
      Shader_Type type = types[i];

      if (!src || type == INVALID_SHADER_TYPE) {
         trace_error("Null shader source or invalid type at index %zu.\n", i);
         goto fail;
      }

      GLuint shader_handle = glCreateShader(type);
      if (!shader_handle) {
         trace_error("glCreateShader failed at index %zu.\n", i);
         goto fail;
      }

      glShaderSource(shader_handle, 1, (const GLchar **)&src, nullptr);
      glCompileShader(shader_handle);

      GLint compiled = GL_FALSE;
      glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &compiled);
      if (compiled == GL_FALSE) {
         GLint log_length = 0;
         glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &log_length);

         if (log_length > 1) {
            char *error_msg = malloc((usz)log_length);
            glGetShaderInfoLog(shader_handle, log_length, nullptr, error_msg);

            if (loaded_from_this_path) {
               int shader_index = index_shader_metadata(loaded_from_this_path);
               trace_debug("Found index %d metadata for %s", shader_index, loaded_from_this_path);
               print_remapped_opengl_errors(error_msg, shader_index, type);
            }

            trace_error("Shader compile error (type %u):\nOpenGL says: %s\n", type, error_msg);
            free(error_msg);
         }

         glDeleteShader(shader_handle);
         goto fail;
      }

      glAttachShader(program, shader_handle);
      compiled_shaders[i] = shader_handle;
   }

   glLinkProgram(program);

   GLint linked = GL_FALSE;
   glGetProgramiv(program, GL_LINK_STATUS, &linked);
   if (linked == GL_FALSE) {
      GLint log_length = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

      if (log_length > 1) {
         char *log = malloc((usz)log_length);
         glGetProgramInfoLog(program, log_length, nullptr, log);
         trace_error("Shader link error:\n%s\n", log);
         free(log);
      }
      goto fail;
   }

   // Cleanup shaders after linking
   for (usz i = 0; i < count; ++i) {
      if (compiled_shaders[i]) {
         glDetachShader(program, compiled_shaders[i]);
         glDeleteShader(compiled_shaders[i]);
      }
   }

   shader.handle = program;
   shader.type = 0; // optional: store stage bitfield
   shader.path = loaded_from_this_path;
   return shader;

fail:
   // Cleanup shaders
   for (usz i = 0; i < count; ++i) {
      if (compiled_shaders[i]) {
         glDeleteShader(compiled_shaders[i]);
      }
   }
   if (program) {
      glDeleteProgram(program);
   }
   return shader_invalid;
}

Shader create_shader_single_from_memory(u8* source, Shader_Type type) {
   const u8 *sources[] = {source};
   const Shader_Type types[] = {type};
   assert(count_of(sources) == count_of(types));

   Shader result = create_shader_from_memory(sources, types, count_of(sources), nullptr);
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
         result = create_shader_from_memory(sources, types, count, path);
      }

      result.path = path;
   }


   trestore(checkpoint);


   if (is_valid_shader(result)) {
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

   print_shader_metadata(0);
   print_shader_metadata(1);

   // Always free the dynamic string, under success or failure.
   ds_free(ds);
   return result;
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
   ULONGLONG out_time = ((ULONGLONG)out_attr.ftLastWriteTime.dwHighDateTime << 32) | out_attr.ftLastWriteTime.dwLowDateTime;

   for (usz i = 0; i < input_paths_count; ++i) {
      WIN32_FILE_ATTRIBUTE_DATA in_attr;
      if (!GetFileAttributesExA(input_paths[i], GetFileExInfoStandard, &in_attr)) {
         return -1;
      }
      ULONGLONG in_time = ((ULONGLONG)in_attr.ftLastWriteTime.dwHighDateTime << 32) | in_attr.ftLastWriteTime.dwLowDateTime;
      if (in_time > out_time)
         return 1;
   }

   return 0;
}

#elif defined(PLATFORM_LINUX)

int needs_rebuild_from_paths(ZString output_path, ZString *input_paths, usz input_paths_count) {
   struct stat out_stat;
   if (stat(output_path, &out_stat) < 0) {
      if (errno == ENOENT)
         return 1;
      return -1;
   }

   for (usz i = 0; i < input_paths_count; ++i) {
      struct stat in_stat;
      if (stat(input_paths[i], &in_stat) < 0)
         return -1;
      if (in_stat.st_mtime > out_stat.st_mtime)
         return 1;
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
// TODO: For shader that didn't come from path, we could based content and compare to something? Just seems more trouble than its worth it
// Because if it didnt come from path, its usually hardcoded and constant during the program, no theres no reason to reload. it eighetr works or it doesnt
// And if it comes from path, than it's fine
bool shader_needs_reload(Shader shader) {
   GLuint shader_handle = shader.handle;
   if (INVALID_SHADER_HANDLE == shader_handle) {
      return false;
   }

   if (!shader.path) {
      return false;
   }

   auto index = index_shader_metadata(shader.path);
   auto meta  = shaders_metadata[index];
   auto paths_count = meta.count;

   if (paths_count <= 0) {
      return false;
   }


   usz  count = 0;
   bool result = false;
   usz checkpoint = tsave();
   // We always save and .time files based on first_path
   ZString* resolved_paths = (ZString*)talloc(paths_count * size_of(ZString));
   ZString first_path = meta.items[0].path;
   for (int i = 0; i < paths_count; i += 1) {
      resolved_paths[i] = meta.items[i].path;
       // resolved_paths[i] = (char*)all_unique_paths.data + paths.items[i];
   }
   TString time_path = tprintf("%s.time", path_stem(first_path));

   if (needs_rebuild_from_paths(time_path, resolved_paths, paths_count)) {
      trace_debug("Yes, we need reload based on paths for: %s", time_path);
      return_defer(result = true);
   } else {
      trace_debug("No reload needed for: %s", time_path);
   }

defer:
   trestore(checkpoint);
   return result;
}



Shader reload_shader(Shader shader) {
   system("clear"); // HACK XXX: Trying to clear the whole terminal to not flood with erros
   trace_info("Trying to reload %s", shader.path);
   Shader new_shader = create_shader(shader.path, shader.type);

   // return shader;


   // Keep current shader while errors in new shader
   if (!is_valid_shader(new_shader)) {
      return shader;
   }

   // Only delete if shader was valid to begin with
   if (is_valid_shader(shader)) {
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

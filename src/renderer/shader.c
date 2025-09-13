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

//
// Might not look like it from the struct declaration itself but this is a tree.
// We make so there's no two items with overlapping cases like this:
//                     line_start-------line_end
//                   line_start-----end_line
//                               ^^^
// It's either parent-child:
//    parrent > line_start--------------line_end
//    child   >      line_start-----end_line
//                             ^^^^^
// Or sibling-sibling (no overlap):
//    sibling > line_start----line_end
//    sibling >                 line_start-----end_line
//
static struct {
   struct {
      ZString path; // Which Paths does this shader include considering the first path that we pass when we create
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

static int index_shader_metadata(const char *path) {
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
   auto new_item  = &shaders_metadata[shader_index].items[new_index];
   *new_item = (typeof(*new_item)) {
      .path            = path,
      .line_start      = line_number,
      .line_end        = -1,
   };
   shaders_metadata[shader_index].count += 1;
}


static isz count_lines_in_string(const char* str, isz length) {
   isz lines = 0;
   for (isz i = 0; i < length; i++) {
      if (str[i] == '\n') lines++;
   }
   return lines;
}

// TODO: Make it work for offset_compute, and refactor that to allow easier access to these offsets.
static bool pre_process_shader_with_metadata(
   const char *path,        DString *ds,
   Isz_DArray *path_offets,
   i64 *offset_compute, i64 *offset_fragment,    i64 *offset_vertex,
   int shader_index,        isz *current_line_number
) {

   // Keep in mind that when reading the full file into memory, a newline is appended at the end
   // So even if you save a file with no new line at the end, this source will have a \n as the last char:
   // Remove it like this if you need: source[strlen(source)-1] = '\0';
   char *source = read_file(path);
   usz source_length = strlen(source);
   if (!source) {
      return false;
   }
   assert_msg(source[source_length-1] == '\n', "All offsets assumes the read_file behaviour is to append a new line always.");

   // Record this file's starting line
   isz file_start_line = *current_line_number;
   isz string_offset_in_buffer = append_unique_path(path);
   const char *stored_path = (const char *)(all_unique_paths.items + string_offset_in_buffer);

   // Add metadata entry with start line
   append_shader_metadata(shader_index, stored_path, file_start_line);
   da_append(path_offets, string_offset_in_buffer);
   isz metadata_index = shaders_metadata[shader_index].count - 1; // Last added entry
   auto *metadata = &shaders_metadata[shader_index].items[metadata_index];

   stb_lexer lexer = {0};
   char store[8192] = {0};
   stb_c_lexer_init(&lexer, source, source + source_length, store, count_of(store));

   char *start = lexer.parse_point;

   while (stb_c_lexer_get_token(&lexer)) {
      auto lexer_before = lexer;
      if (lexer.token == '#' && stb_c_lexer_get_token(&lexer)) {
         if (lexer.token == CLEX_id && strcmp(lexer.string, "include") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_dqstring) {
               const char *include_path = lexer.string;

               {  // Write until right before on the #
                  //        #include "path" // Some other code or comment
                  //       -^--------------------------------------------

                  auto end = lexer_before.where_firstchar;
                  ds_write_buf(ds, start, end - start);
                  *current_line_number += count_lines_in_string(start, end - start);
                  // The new start where we left off AFTER the include path, probably onto some whitespace
                  //        #include "path" // Some other code or comment
                  //        ---------------^-----------------------------
                  start = lexer.parse_point;
               }

               if (false) {
                  ds_write(ds, "\n");
                  *current_line_number += 1;
               }

               const char *resolved_path = nullptr;

               {  // Resolving if the path is relative to the file or relative to the working directory
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
               }

               if (!pre_process_shader_with_metadata(resolved_path, ds, path_offets, offset_compute, offset_fragment, offset_vertex, shader_index, current_line_number)) {
                  assert_msg(false, "TODO handle pre_process_shader failure");
                  return false;
               }

               // No need to do this since read_file add a new line, although I'm not sure if unix read_file version does this, that's why `if false`.
               if (false) {
                  // Make sure no two paths has the same line_end number
                  ds_write(ds, "\n");
                  *current_line_number += 1;
               }
            }
         } else if (lexer.token == CLEX_id && strcmp(lexer.string, "version") == 0) {
            {  // Checking commong errors and advancing the lexer
               if (stb_c_lexer_get_token(&lexer) && lexer.token != CLEX_intlit) {
                  assert_msg(false, "After #version everything should be a integer");
                  return false;
               }

               if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_id && strcmp(lexer.string, "core") != 0) {
                  assert_msg(false, "Only core version allowed, but we got %s instead", lexer.string);
                  return false;
               }
            }

            {
               // Here 'start' might have \n on it, so we'll catch it in count_lines
               // Differently from #pragma, we wanna include the #version core 460 in the final ds, so we'll not skip it.
               auto end = lexer.parse_point;
               ds_write_buf(ds, start, end - start);
               *current_line_number += count_lines_in_string(start, end - start);
               start = end;
            }

         } else if (lexer.token == CLEX_id && strcmp(lexer.string, "pragma") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_id) {
               bool is_fragment = (strcmp(lexer.string, "fragment") == 0);
               bool is_vertex   = (strcmp(lexer.string, "vertex"  ) == 0);

               if (is_vertex   && (nullptr == offset_vertex   || -1 != *offset_vertex  )) {
                  return false;
               }
               if (is_fragment && (nullptr == offset_fragment || -1 != *offset_fragment)) {
                  return false;
               }

               { // End is not where the lexer left off, is right until '#' because we wanna skip the pragma statement and not pass to the shader actual compilation buffer
                  auto end = lexer_before.where_firstchar;
                  ds_write_buf(ds, start, end - start);
                  *current_line_number += count_lines_in_string(start, end - start);
                  //
                  // NOTE: This error report might be wrong if these pragma is on one line and vertex is on another
                  //       Same this might happen with version. But I don't think we'll ever split into two lines, but something to keep in mind.
                  //
                  // The new start where we left off AFTER the complete pragma directive
                  //        #pragma  vertex  // Some other code or comment
                  //        ---------------^-----------------------------
                  start = lexer.parse_point;
               }

               // Zero terminate the previous shader code (vertex or another)
               if (ds->count > 0) {
                  ds_write_zero(ds);
               }

               {  // NOTE: We're considering that #pragma type occupies a full line k
                  //       we shall account for that with a -1
                  auto shader_type_offset = ds->count;
                  if (is_fragment) {
                     shaders_metadata[shader_index].fragment_line = *current_line_number;
                     *offset_fragment = shader_type_offset;
                  } else if (is_vertex) {
                     shaders_metadata[shader_index].vertex_line = *current_line_number;
                     *offset_vertex   = shader_type_offset;
                  } else {
                     assert_msg(0, "Implement other types");
                  }
               }
            }
         }
      }
   }

   {
      // In these case we're done with the file, but it might have '// Comments' or other tokens that is not recognized
      // By stb_lexer, so we just copy the rest verbatim.
      // NOTE: I'm unsure if it's eof-1 or just eof.
      auto end = lexer.eof;
      if (end > start) {
         ds_write_buf(ds, start, end - start);
         *current_line_number += count_lines_in_string(start, end - start);
         // Update this file's end line
         shaders_metadata[shader_index].items[metadata_index].line_end = *current_line_number;
      }
   }

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

static void print_shader_metadata(int shader_index) {
   if (shader_index >= MAX_SHADERS) {
      return;
   }

   const auto meta = shaders_metadata[shader_index];
   DString ds = {0};


   isz fragment_line = 0;  // Line where fragment shader starts (after #pragma fragment)
   isz vertex_line = 0;    // Line where vertex shader starts (after #pragma vertex)
   ds_printf(&ds, "items = %p, count = %lld, capacity = %lld\nfragment_lines=%lld vertex_line=%lld\n", meta.items, (usz)meta.count, (usz)meta.capacity, (usz)meta.fragment_line, (usz)meta.vertex_line);
   for (int item_index = 0; item_index < meta.count; item_index += 1) {
      auto item = meta.items[item_index];
      ds_printf(&ds, "   path=%s line_number=%lld line_end=%lld\n",
         item.path,
         (usz)item.line_start,
         (usz)item.line_end
      );
   }
   ds_write_zero(&ds);

   assert_msg(CYE_MAX_TRACE_LOG_MSG_LENGTH > ds.count + 50,  "trace_info is not dyanmic alocated, it uses fixed buffer.");
   trace_info((char*)ds.items);
   // printf("\nprintf\n%s", ds.items);

   ds_free(ds);
}

// TODO: Make this return the ith child instead 0-th would be the first child. return the ith child doesnt exist.
//       That would mean that if 0-th child return -1, the parent passed has no child (leaf node).
// TODO: Make it work when error is between #pragma and #version
static isz metadata_next_sibling(int shader_index, isz parent_index, isz from_this_child_index) {
   auto list = shaders_metadata[shader_index];
   assert(parent_index < (list.count - 1));

   auto parent = list.items[parent_index];
   auto first  = list.items[from_this_child_index];
   for (int index = from_this_child_index + 1; index < list.count; index += 1) {
      auto item = list.items[index];

      // Not a child of parent because it comes after parent
      if (item.line_start > parent.line_end) {
         return -2;
      }

      // We found the next sibling because this current item starts after the first child
      // Therefore it's not included inside the first child.
      if (item.line_start > first.line_end ) {
         assert_msg(item.line_end < parent.line_end, "this should be garanteed");
         return index;
      } else {
         // Otherwise is a child of first and not a sibling, keep looking
      }
   }
   return -1;
}

static bool metadata_map_line_to_file_ground_truth(int shader_index, Shader_Type shader_type, isz global_line, const char **filepath, isz *local_line) {
   if (shader_index >= MAX_SHADERS || shaders_metadata[shader_index].count <= 0) {
      return false;
   }

   auto meta = shaders_metadata[shader_index].items[0];
   print_shader_metadata(shader_index);

   isz meta_index_best = 0;

   for (int meta_index = 1; meta_index < shaders_metadata[shader_index].count; meta_index += 1) {
      auto item = shaders_metadata[shader_index].items[meta_index];
      // We don't wanna take into account the actual line_end of current item
      // Because it is considering the extra \n appended by read_file
      // This allow us to catch errors like this #include "./src/coordinates.glsl" vec5 fas;
      // Instead of saying the error is inside ./src/coordinates.glsl we can be sure its on the file that included.
      // NOTE: Maybe explain better, trust me you want exclusive check `global_line < item.line_end`, not inclusive.
      if (global_line >= item.line_start && global_line < item.line_end) {
         auto best = shaders_metadata[shader_index].items[meta_index_best];
         // We can assume no line_start nor line_end are equals
         if (item.line_start > best.line_start || item.line_end < best.line_end) {
            meta_index_best = meta_index;
         }
      }
   }

   auto parent = shaders_metadata[shader_index].items[meta_index_best];
   *filepath = parent.path;

   trace_info("Best! for %lld", (usz)global_line);
   trace_struct(parent);

   auto list = shaders_metadata[shader_index];

   assert(meta_index_best < list.count);
   isz line_number = 1;
   // If parent is the very last item we do the calculation right here
   if ( (list.count -1) == meta_index_best) {
      line_number += global_line - parent.line_start;
      *local_line = line_number;
      return true;
   }

   auto child = list.items[meta_index_best + 1];

   // Not children or between parent and first child.
   if (global_line <= child.line_start || parent.line_end < child.line_start) {
      line_number += global_line - parent.line_start;
      *local_line = line_number;
      return true;
   }

   line_number += child.line_start - parent.line_start;

   isz parent_index      = meta_index_best;
   isz curr_child_index  = parent_index + 1;
   isz next_child_index  = metadata_next_sibling(shader_index, parent_index, curr_child_index);

   while (!(next_child_index < 0))  {
      assert(next_child_index < list.count);
      auto curr_child = list.items[curr_child_index];
      auto next_child = list.items[next_child_index];
      // In between these two children, we're done.
      if (global_line >= curr_child.line_end  && global_line <= next_child.line_start) {
         line_number += global_line - curr_child.line_end;
         *local_line = line_number;
         return true;
      }

      //  Since it's not in between those, we need to add how much lines are between them.
      // Plus 1 (line_end + 1) to compensate automatic new line when read_file is called
      line_number += next_child.line_start - curr_child.line_end;

      curr_child_index = next_child_index;
      next_child_index = metadata_next_sibling(shader_index, parent_index, curr_child_index);
   }

   auto last_child = list.items[curr_child_index];
   assert_msg(global_line >= last_child.line_end  && global_line <= parent.line_end,
      "If we got here then is has to be between the last child and the last line of the parent"
   );
   line_number += global_line - last_child.line_end;
   // TODO: Test this case
   *local_line = line_number;
   return true;
}

// Returns index of the ith child of `parent_index`
// -1 if no such child exists
static isz metadata_ith_child(int shader_index, isz parent_index, isz ith) {
   auto list = shaders_metadata[shader_index];
   auto parent = list.items[parent_index];

   // TODO: make ith unsigned
   assert(ith >= 0);

   isz child_count = 0;
   isz start_has_to_be_bigger_than_this = parent.line_start;
   for (isz index = parent_index + 1; index < list.count; index += 1) {
      auto child = list.items[index];
      // We've left the parent’s span -> no more children
      if (child.line_start > parent.line_end) {
         return -1;
      }

      // Candidate inside parent => it's the next *direct* child
      if (child.line_start > start_has_to_be_bigger_than_this && child.line_end < parent.line_end) {
         child_count += 1;
         start_has_to_be_bigger_than_this = child.line_end;
      }

      if (child_count == (ith + 1)) {
         return index;
      }
   }

   return -1;
}


static bool metadata_map_line_to_file_new(int shader_index, Shader_Type shader_type, isz global_line, const char **filepath, isz *local_line) {
   if (shader_index >= MAX_SHADERS || shaders_metadata[shader_index].count <= 0){
      return false;
   }

   auto list = shaders_metadata[shader_index];

   isz  best_index = 0;
   auto best = list.items[best_index];

   // Find the deepest (most specific) file that contains this line
   for (int meta_index = 1; meta_index < list.count; meta_index += 1) {
      auto curr = list.items[meta_index];
      // We don't wanna take into account the actual line_end of current item
      // Because it is considering the extra \n appended by read_file
      // This allow us to catch errors like this #include "./src/coordinates.glsl" vec5 fas;
      // Instead of saying the error is inside ./src/coordinates.glsl we can be sure its on the file that included.
      // NOTE: Maybe explain better, trust me you want exclusive check `global_line < item.line_end`, not inclusive.
      if (global_line >= curr.line_start && global_line < curr.line_end) {
         // We can assume no line_start nor line_end are equals
         if (curr.line_start > best.line_start || curr.line_end < best.line_end) {
            best_index = meta_index;
            best = list.items[best_index];
         }
      }
   }
   *filepath = best.path;


   // Now let's find the line number (the hard part)
   auto parent = best;
   auto parent_index = best_index;
   assert(parent_index < list.count);

   isz line_number = 1;
   // TODO: Test with no children

   isz ith = 0;
   isz curr_index = metadata_ith_child(shader_index, parent_index, ith++); // 1th child
   isz next_index = metadata_ith_child(shader_index, parent_index, ith++); // 1th child
   // One or more children, at leat one
   bool has_children = !(curr_index < 0);


   if (!has_children) {
      // Parent doesn't have children then it's between global_line and parent start
      line_number += global_line - parent.line_start;
   } else {
      // Parent only has at least 1 child.
      // We need start couting by checking if is between the first child and the parent
      auto child = list.items[curr_index];
      bool between_starts = global_line >= parent.line_start && global_line <= child.line_start;

      if (between_starts) {
         // Add only the number of lines between global_line and parent start
         line_number += global_line - parent.line_start;
         goto end;
      } else {
         // Add the full number of lines between child and parent
         line_number += child.line_start - parent.line_start;
      }
   }

   while (next_index >= 0) {
      auto next = list.items[next_index];
      auto curr = list.items[curr_index];

      bool between_us = global_line >= curr.line_end && global_line <= next.line_start;

      if (between_us) {
         line_number += global_line - curr.line_end;
         break;
      }

      // Global line it's not between anything, so add how many lines to come from curr sibling to the next
      line_number += next.line_start - curr.line_end;

      curr_index = next_index;
      next_index = metadata_ith_child(shader_index, parent_index, ith++); // 1th child

      bool last_loop = next_index < 0;
      if (last_loop) {
         // NOTE: variable 'next' here it the current 'next', not the upcoming iteration 'next' that will be defines by next_index (which this on is new).
         assert_msg(global_line >= next.line_end && global_line <= parent.line_end, "Ain't no way we're the last loop, and it's not between next and parent");
         line_number += global_line - next.line_end;
         break;
      }

   }

end:
   // This is correct even if theres no child, or is last item or if it's between the first and
   *local_line = line_number;
   return true;
}



static bool metadata_map_line_to_file(int shader_index, Shader_Type shader_type, isz global_line, const char **filepath, isz *local_line) {
   isz local_line1 = -1;
   const char *filepath1 = "";
   bool ok1 = metadata_map_line_to_file_ground_truth(shader_index, shader_type, global_line, &filepath1, &local_line1);

   isz local_line2 = -1;
   const char *filepath2 = "";
   bool ok2 = metadata_map_line_to_file_new(shader_index, shader_type, global_line, &filepath2, &local_line2);

   if (local_line1 == local_line2 && ok1 == ok2 && path_equals(filepath2, filepath1)) {
      if (ok1) {
         trace_okay("%s ground_truth and new match", __func__);
      } else {
         trace_error("%s ground_truth and new match, but its not okay.", __func__);
      }
      *local_line = local_line1;
      *filepath   = filepath1;
      return ok1;
   } else {
      trace_warn("%s ground_truth (old) and new mismatch:\n"
         "old={ok=%s,line_number=%lld,path=%s}\nnew={ok=%s,line_number=%lld,path=%s}\n. Returning old.",
         __func__,
         (ok1 ? "true" : "false"), local_line1, filepath1,
         (ok2 ? "true" : "false"), local_line2, filepath2
      );
      *local_line = local_line1;
      *filepath   = filepath1;
      return ok1;
   }
}
static void print_remapped_opengl_errors(const char *error_string, int shader_index, Shader_Type shader_type) {
   DString ds = {0};
   const char *p = error_string;
   const char *end = error_string + strlen(error_string);

   while (p < end) {
      // Try to find "(<number>)" pattern
      const char *paren = strchr(p, '(');
      if (!paren)
         break;

      // Check if previous chars look like "... <int>("
      const char *num_start = paren;
      while (num_start > p && isdigit((unsigned char)num_start[-1])) {
         num_start--;
      }
      if (num_start == paren) {
         p = paren + 1;
         continue; // no number before '('
      }

      // Parse integer before '('
      long first_num = strtol(num_start, nullptr, 10);

      // Parse inside "(<int>)"
      const char *inside = paren + 1;
      char *inside_end = nullptr;
      long line_num = strtol(inside, &inside_end, 10);

      if (!inside_end || *inside_end != ')') {
         p = paren + 1;
         continue; // not a valid "(int)"
      }

      const char *after = inside_end + 1;
      if (*after != ':') {
         p = after;
         continue; // not "(int):"
      }

      // At this point we have a match
      const char *line_start = p;
      const char *line_end = after;
      while (*line_end && *line_end != '\n' && *line_end != '\r')
         line_end++;

      // Write original prefix
      ds_write_buf(&ds, line_start, num_start - line_start);

      // Remap line number
      isz global_line = line_num;
      isz local_line = -1;
      const char *filepath = nullptr;
      ZString inform_type_msg = "";

      if (shader_type == SHADER_TYPE_VERTEX) {
         global_line += shaders_metadata[shader_index].vertex_line - 1;
         inform_type_msg = "Vertex shader";
      } else if (shader_type == SHADER_TYPE_FRAGMENT) {
         global_line += shaders_metadata[shader_index].fragment_line - 1;
         inform_type_msg = "Fragment shader";
      }

      ds_printf(&ds, "%s\n", inform_type_msg);
      if (metadata_map_line_to_file(shader_index, shader_type, global_line, &filepath, &local_line)) {
         ds_printf(&ds, "%s:%zu", filepath, local_line);
      } else {
         ds_printf(&ds, "full_block:%ld", line_num);
      }

      // Append rest of error message
      ds_printf(&ds, " %.*s\n", (int)(line_end - after), after);

      // Advance
      p = line_end;
   }

   // Write any trailing text if we didn’t consume all
   if (p < end) {
      ds_write_buf(&ds, p, end - p);
   }

   if (ds.count) {
      ds_write_zero(&ds);
      trace_error("%s", ds.items);
   }
   ds_free(ds);
}

static void print_remapped_opengl_errors2(const char *error_string, int shader_index, Shader_Type shader_type) {
   DString ds = {0};
   stb_lexer lex = {0};
   char store[8*1024];
   isz error_string_length = strlen(error_string);
   stb_c_lexer_init(&lex, error_string, error_string + error_string_length, store, count_of(store));

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
               isz global_line = line_num;
               ZString inform_type_msg = "";


               if (SHADER_TYPE_VERTEX == shader_type) {
                  global_line += shaders_metadata[shader_index].vertex_line - 1;
                  inform_type_msg = "Vertex shader";
               } else if (SHADER_TYPE_FRAGMENT == shader_type) {
                  global_line += shaders_metadata[shader_index].fragment_line - 1;
                  inform_type_msg = "Fragment shader";
               }
               ds_printf(&ds, "%s\n", inform_type_msg);
               if (metadata_map_line_to_file(shader_index, shader_type, global_line, &filepath, &local_line)) {
                  ds_printf(&ds, "%s:%zu", filepath, local_line);
               } else {
                  ds_printf(&ds, "full_block:%ld", line_num);
               }

               const char *error_end = lex.parse_point;
               while (*error_end && *error_end != '\n' && *error_end != '\r') {
                  error_end++;
               }
               ds_printf(&ds, " %.*s\n", (int)(error_end - lex.parse_point), lex.parse_point);

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
            {
               glGetShaderInfoLog(shader_handle, log_length, nullptr, error_msg);
               trace_error("Shader compile error (type %u):\nOpenGL says: %s\n", type, error_msg);

               if (loaded_from_this_path) {
                  int shader_index = index_shader_metadata(loaded_from_this_path);
                  trace_debug("Found index %d metadata for %s", shader_index, loaded_from_this_path);
                  print_remapped_opengl_errors(error_msg, shader_index, type);
               }
            }
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

void create_or_overwrite_time_marker(ZString path) {
   assert(       path_ext("src/dkajslda") == nullptr            ); // Assert that this aint got no exntesion
   assert(strcmp(path_stem("src/dkajslda"), "src/dkajslda") == 0); // Assert that path_stem return the same path when the file has no extension
   assert_msg(strcmp(path_ext("src/dkajslda.txt"), ".txt")   == 0, "ext = %s", path_ext("src/dkajslda.txt")); // Assert it's just the txt without the dot



   ZString extension = path_ext(path);
   assert_msg(path, "Dont you dare pass null to this, fayta.");
   assert_msg(!is_dir(path), "What do you want me to do with a directory?.");
   assert_msg(strcmp(extension, ".time") != 0 , "Extension include .time, which we use to mark the time, so no cigar.");

   usz checkpoint = tsave();
   {
      TString time_path = tprintf("%s.time", path_stem(path));
      String_Slice msg  = ss_from_zstr("This file is just to mark time_t when the shader was compiled");
      write_file(time_path , msg.data, msg.size);
   }
   trestore(checkpoint);
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
            write_file(tprintf("src/assets/shaders/output/ignore/dump-(%d)type-%d.glsl", count, types[i]), (ZString)sources[i], strlen((ZString)sources[i]));
         }

         write_file("src/assets/shaders/output/will-see-dump.glsl", ds.data, ds.size);
         result = create_shader_from_memory(sources, types, count, path);
      }

      result.path = path;
   }

   trestore(checkpoint);



   if (is_valid_shader(result)) {
      create_or_overwrite_time_marker(result.path);
      write_file("src/assets/shaders/output/success-dump.glsl", ds.data, ds.size);
      shader_to_paths[result.handle] =  path_offsets;

   } else {
      write_file("src/assets/shaders/output/failed-dump.glsl", ds.data, ds.size);
      // Only free on failure because we're gonna use the paths if all succeeds.
      da_free(path_offsets);
   }

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
   // system("clear"); // HACK XXX: Trying to clear the whole terminal to not flood with erros
   trace_info("Trying to reload %s", shader.path);
   Shader new_shader = create_shader(shader.path, shader.type);

   // return shader;


   // Keep current shader while errors in new shader
   if (!is_valid_shader(new_shader)) {
      // Create a new timer to avoid flooding the console with errors
      create_or_overwrite_time_marker(shader.path);
      return shader;
   }

   // Only delete if shader was valid to begin with
   if (is_valid_shader(shader)) {
      glDeleteProgram(shader.handle);
   }

   trace_okay("Successfully reloaded `%s` shader", shader.path);
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

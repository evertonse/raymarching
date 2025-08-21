typedef struct {
   u32  vertex_offset;  // Base vertex in global buffers
   u32  index_offset;   // Start index in global index buffer
   u32  index_count;    // Number of indices to draw
   u32  instance_count; // For instanced rendering (usually 1)
   u32  material_index; // Material index
   bool has_joints;     // Whether this renderable uses joint data
} Renderable;

// Global buffer system with separate attribute buffers
typedef struct {
   // Separate attribute buffers (non-interleaved)
   Buffer positions_buffer;
   Buffer normals_buffer;
   Buffer uvs_buffer;
   Buffer joints_buffer; // Optional, but likely
   Buffer indices_buffer;
   Buffer indirect_buffer;

   // CPU staging arrays
   Vector3 *positions;
   Vector3 *normals;
   Vector2 *uvs;
   Joint_Data *joints;
   u32 *indices;
   Draw_Command *draw_commands;

   // Current counts and capacities
   u32 vertex_count;
   u32 vertex_capacity;
   u32 index_count;
   u32 index_capacity;
   u32 draw_command_count;
   u32 draw_command_capacity;

   GLuint vao;

   // Dirty flags for smart updates
   bool positions_dirty;
   bool normals_dirty;
   bool uvs_dirty;
   bool joints_dirty;
   bool indices_dirty;
   bool indirect_dirty;
} Manager_Buffers;

static Manager_Buffers manager_buffers = {0};

// Generic buffer growth function
void buffer_grow_if_needed(Buffer *buffer, u32 current_count, u32 additional_count, u32 element_size, void **cpu_array, u32 *capacity, bool *dirty_flag) {
   if (current_count + additional_count > *capacity) {
      u32 new_capacity = (*capacity * 2) + additional_count;
      isz new_size = new_capacity * element_size;

      // Grow CPU array
      *cpu_array = realloc(*cpu_array, new_size);
      *capacity = new_capacity;

      // Recreate GPU buffer with larger size
      if (buffer->mapped_ptr) {
         glUnmapNamedBuffer(buffer->handle);
         buffer->mapped_ptr = NULL;
      }
      // TODO: Use ornaphane of the buffer strategy instead of deleting the buffer

      glDeleteBuffers(1, &buffer->handle);
      *buffer = create_buffer_extended(buffer->type, buffer->usage, NULL, new_size, buffer->binding);
      *dirty_flag = true; // Need to re-upload all data
   }
}

// Initialize the global buffer system
void init_manager(u32 initial_vertex_capacity, u32 initial_index_capacity) {
   managed_buffers.vertex_capacity = initial_vertex_capacity;
   managed_buffers.index_capacity = initial_index_capacity;
   managed_buffers.draw_command_capacity = 1024;

   // Allocate CPU arrays
   manager_buffers.positions     = malloc(initial_vertex_capacity * size_of(Vector3));
   manager_buffers.normals       = malloc(initial_vertex_capacity * size_of(Vector3));
   manager_buffers.uvs           = malloc(initial_vertex_capacity * size_of(Vector2));
   manager_buffers.joints        = malloc(initial_vertex_capacity * size_of(Joint_Data));
   manager_buffers.indices       = malloc(initial_index_capacity  * size_of(u32));
   manager_buffers.draw_commands = malloc(manager_buffers.draw_command_capacity * size_of(Draw_Command));

   // Create GPU buffers
   manager_buffers.positions_buffer = create_buffer_extended(BUFFER_TYPE_VERTEX, BUFFER_USAGE_DYNAMIC, NULL, initial_vertex_capacity * size_of(Vector3), -1);

   manager_buffers.normals_buffer = create_buffer_extended(BUFFER_TYPE_VERTEX, BUFFER_USAGE_DYNAMIC, NULL, initial_vertex_capacity * size_of(Vector3), -1);

   manager_buffers.uvs_buffer = create_buffer_extended(BUFFER_TYPE_VERTEX, BUFFER_USAGE_DYNAMIC, NULL, initial_vertex_capacity * size_of(Vector2), -1);

   manager_buffers.joints_buffer = create_buffer_extended(BUFFER_TYPE_STORAGE, BUFFER_USAGE_DYNAMIC, NULL, initial_vertex_capacity * size_of(Joint_Data), 3);

   manager_buffers.indices_buffer = create_buffer_extended(BUFFER_TYPE_INDEX, BUFFER_USAGE_DYNAMIC, NULL, initial_index_capacity * size_of(u32), -1);

   manager_buffers.indirect_buffer = create_buffer_extended(BUFFER_TYPE_STORAGE, BUFFER_USAGE_DYNAMIC, NULL, manager_buffers.draw_command_capacity * size_of(Draw_Command), 0);

   // Create VAO and setup attribute bindings
   glCreateVertexArrays(1, &managed_buffers.vao);

   // Positions (binding 0)
   glVertexArrayVertexBuffer(manager_buffers.vao, 0, manager_buffers.positions_buffer.handle, 0, size_of(Vector3));
   glEnableVertexArrayAttrib(managed_buffers.vao, 0);
   glVertexArrayAttribFormat(manager_buffers.vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(managed_buffers.vao, 0, 0);

   // Normals (binding 1)
   glVertexArrayVertexBuffer(manager_buffers.vao, 1, manager_buffers.normals_buffer.handle, 0, size_of(Vector3));
   glEnableVertexArrayAttrib(managed_buffers.vao, 1);
   glVertexArrayAttribFormat(manager_buffers.vao, 1, 3, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(managed_buffers.vao, 1, 1);

   // UVs (binding 2)
   glVertexArrayVertexBuffer(manager_buffers.vao, 2, manager_buffers.uvs_buffer.handle, 0, size_of(Vector2));
   glEnableVertexArrayAttrib(managed_buffers.vao, 2);
   glVertexArrayAttribFormat(manager_buffers.vao, 2, 2, GL_FLOAT, GL_FALSE, 0);
   glVertexArrayAttribBinding(managed_buffers.vao, 2, 2);

   // Index buffer
   glVertexArrayElementBuffer(managed_buffers.vao, managed_buffers.indices_buffer.handle);

   // Bind joint data as SSBO (binding point 3)
   glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, manager_buffers.joints_buffer.handle);
}

// Add mesh data to global buffers
Renderable manager_push(Vector3 *positions, Vector3 *normals, Vector2 *uvs, Joint_Data *joints, u32 vertex_count, u32 *indices, u32 index_count, u32 material_index) {
   // Grow buffers if needed
   buffer_grow_if_needed(&manager_buffers.positions_buffer, manager_buffers.vertex_count, vertex_count, size_of(Vector3), (void **)&manager_buffers.positions, &manager_buffers.vertex_capacity, &manager_buffers.positions_dirty);

   buffer_grow_if_needed(&manager_buffers.normals_buffer, manager_buffers.vertex_count, vertex_count, size_of(Vector3), (void **)&manager_buffers.normals, &manager_buffers.vertex_capacity, &manager_buffers.normals_dirty);

   buffer_grow_if_needed(&manager_buffers.uvs_buffer, manager_buffers.vertex_count, vertex_count, size_of(Vector2), (void **)&manager_buffers.uvs, &manager_buffers.vertex_capacity, &manager_buffers.uvs_dirty);

   buffer_grow_if_needed(&manager_buffers.joints_buffer, manager_buffers.vertex_count, vertex_count, size_of(Joint_Data), (void **)&manager_buffers.joints, &manager_buffers.vertex_capacity, &manager_buffers.joints_dirty);

   buffer_grow_if_needed(&manager_buffers.indices_buffer, manager_buffers.index_count, index_count, size_of(u32), (void **)&manager_buffers.indices, &manager_buffers.index_capacity, &manager_buffers.indices_dirty);

   Renderable renderable = {.vertex_offset = manager_buffers.vertex_count, .index_offset = manager_buffers.index_count, .index_count = index_count, .instance_count = 1, .material_index = material_index, .has_joints = (joints != NULL)};

   // Copy vertex attribute data
   memcpy(&manager_buffers.positions[manager_buffers.vertex_count], positions, vertex_count * size_of(Vector3));
   memcpy(&manager_buffers.normals[manager_buffers.vertex_count], normals, vertex_count * size_of(Vector3));
   memcpy(&manager_buffers.uvs[manager_buffers.vertex_count], uvs, vertex_count * size_of(Vector2));

   // Copy joint data if present
   if (joints) {
      memcpy(&manager_buffers.joints[manager_buffers.vertex_count], joints, vertex_count * size_of(Joint_Data));
      manager_buffers.joints_dirty = true;
   }

   // Copy index data (offset by current vertex count)
   for (u32 i = 0; i < index_count; i++) {
      manager_buffers.indices[manager_buffers.index_count + i] = indices[i] + manager_buffers.vertex_count;
   }

   // Update counts and mark dirty
   managed_buffers.vertex_count += vertex_count;
   managed_buffers.index_count += index_count;
   manager_buffers.positions_dirty = true;
   manager_buffers.normals_dirty = true;
   manager_buffers.uvs_dirty = true;
   manager_buffers.indices_dirty = true;

   return renderable;
}

// Smart GPU buffer updates
void manager_update_gpu_buffers() {
   if (managed_buffers.positions_dirty) {
      glNamedBufferSubData(manager_buffers.positions_buffer.handle, 0, manager_buffers.vertex_count * size_of(Vector3), manager_buffers.positions);
      manager_buffers.positions_dirty = false;
   }

   if (managed_buffers.normals_dirty) {
      glNamedBufferSubData(manager_buffers.normals_buffer.handle, 0, manager_buffers.vertex_count * size_of(Vector3), manager_buffers.normals);
      manager_buffers.normals_dirty = false;
   }

   if (managed_buffers.uvs_dirty) {
      glNamedBufferSubData(manager_buffers.uvs_buffer.handle, 0, manager_buffers.vertex_count * size_of(Vector2), manager_buffers.uvs);
      manager_buffers.uvs_dirty = false;
   }

   if (managed_buffers.joints_dirty) {
      glNamedBufferSubData(manager_buffers.joints_buffer.handle, 0, manager_buffers.vertex_count * size_of(Joint_Data), manager_buffers.joints);
      manager_buffers.joints_dirty = false;
   }

   if (managed_buffers.indices_dirty) {
      glNamedBufferSubData(manager_buffers.indices_buffer.handle, 0, manager_buffers.index_count * size_of(u32), manager_buffers.indices);
      manager_buffers.indices_dirty = false;
   }

   if (managed_buffers.indirect_dirty) {
      glNamedBufferSubData(manager_buffers.indirect_buffer.handle, 0, manager_buffers.draw_command_count * size_of(Draw_Command), manager_buffers.draw_commands);
      manager_buffers.indirect_dirty = false;
   }
}

// Render single renderable
void render_renderable(const Renderable *renderable) {
   glBindVertexArray(managed_buffers.vao);
   glDrawElementsBaseVertex(GL_TRIANGLES, renderable->index_count, GL_UNSIGNED_INT, (void *)(renderable->index_offset * size_of(u32)), renderable->vertex_offset);
}

// Add to indirect batch
void add_to_indirect_batch(const Renderable *renderable) {
   buffer_grow_if_needed(&manager_buffers.indirect_buffer, manager_buffers.draw_command_count, 1, size_of(Draw_Command), (void **)&manager_buffers.draw_commands, &manager_buffers.draw_command_capacity, &manager_buffers.indirect_dirty);

   Draw_Command cmd = {.index_count = renderable->index_count, .instance_count = renderable->instance_count, .first_index = renderable->index_offset, .base_vertex = renderable->vertex_offset, .base_instance = 0};

   manager_buffers.draw_commands[manager_buffers.draw_command_count++] = cmd;
   manager_buffers.indirect_dirty = true;
}

// Execute indirect batch
void render_indirect_batch() {
   if (managed_buffers.draw_command_count == 0)
      return;

   manager_update_gpu_buffers();

   glBindVertexArray(managed_buffers.vao);
   glBindBuffer(GL_DRAW_INDIRECT_BUFFER, manager_buffers.indirect_buffer.handle);

   glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, manager_buffers.draw_command_count, 0);

   managed_buffers.draw_command_count = 0;
}

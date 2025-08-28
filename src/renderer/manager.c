


 // Match glsl types
typedef u64        uvec2;
typedef u32        uint;
typedef Vector4Int ivec4;
typedef Vector4    vec4;
#include "./shared/types.glsl"

typedef struct Draw_Command Draw_Command;
typedef struct Material     Material;


typedef struct {
   u32 index; // Index into draw_commands.items
   u32 count; // Allocated in sequence, Index+0 ... Index + count-1
} Draw_Index;

static_assert(size_of(Draw_Command) % 16 == 0);


// Global buffer system with separate attribute buffers
// Unalignment goes crazy with all these dirty flags @Flag
typedef struct {
   // CPU staging arrays

   struct {
      Vector3 *positions;
      Vector3 *normals;
      Vector2 *uvs;


      // Sequence of Vertex data. First draw_command will fill with all its Positions then Normals, then Uvs. 
      // Then we are back to Positions again for the next draw_command and so on
      u32    count;
      u32    capacity;
      Buffer buffer;
      bool   dirty;
   } vertices;


   struct {
      struct {
         // Order is important
         Vector4Int indices;
         Vector4    weights;
         // should have one of each per position or none
      } *items;

      u32    count;
      u32    capacity;
      Buffer buffer; // Optional, but likely
      bool   dirty;
   } joints;

   struct {
      u32   *items;
      u32    count;
      u32    capacity;
      Buffer buffer;
      bool   dirty;
   } indices;

   // Material management
   struct {
      struct {
         Texture diffuse;
         Texture specular;
         Texture emissive;
         bool    loaded; // Instead of that just is_valid or check the path
      } *items;
      u32    count;
      u32    capacity;
      Buffer buffer; // These are just the bindless handles
      bool   dirty;
   } materials;

   struct {
      Draw_Command *items;
      u32    count;
      u32    capacity;
      Buffer buffer;
      bool   dirty;
   } draw_commands;

   GLuint vao; // TODO: Remove, the renderer should simply have one vertex array for everything and bind just the index buffer when needed
} Manager;

static Manager manager = {0};


void grow_manager_if_needed(u32 required_vertices, u32 required_indices) {
   // Check if we need to grow vertex arrays
   if (manager.vertices.count + required_vertices > manager.vertices.capacity) {
      u32 new_capacity = manager.vertices.capacity + (manager.vertices.capacity / 2);
      if (new_capacity < manager.vertices.count + required_vertices) {
         new_capacity = manager.vertices.count + required_vertices;
      }

      // Reallocate CPU arrays
      manager.vertices.positions = realloc(manager.vertices.positions, new_capacity * size_of(manager.vertices.positions[0]));
      manager.vertices.normals   = realloc(manager.vertices.normals,   new_capacity * size_of(manager.vertices.normals  [0]));
      manager.vertices.uvs       = realloc(manager.vertices.uvs,       new_capacity * size_of(manager.vertices.uvs      [0]));


      manager.vertices.capacity = new_capacity;
      manager.vertices.dirty    = true; // Mark for full upload
   }

   // Check if we need to grow index buffer
   if (manager.indices.count + required_indices > manager.indices.capacity) {
      u32 new_capacity = manager.indices.capacity + (manager.indices.capacity / 2);
      if (new_capacity < manager.indices.count + required_indices) {
         new_capacity = manager.indices.count + required_indices;
      }

      isz buffer_size = new_capacity * size_of(manager.indices.items[0]);
      manager.indices.items = realloc(manager.indices.items, buffer_size);
      manager.indices.capacity = new_capacity;
      manager.indices.dirty = true;
   }
}

Draw_Index push_draw_command_to_manager(const Draw_Command command) {
   const int required_draw_commands = 1;
   if (manager.draw_commands.count + required_draw_commands > manager.draw_commands.capacity) {
      u32 new_capacity = manager.draw_commands.capacity + (manager.draw_commands.capacity / 2);
      if (new_capacity < manager.draw_commands.count + required_draw_commands) {
         new_capacity = manager.draw_commands.count + required_draw_commands;
      }

      isz buffer_size = new_capacity * size_of(manager.draw_commands.items[0]);
      manager.draw_commands.items = realloc(manager.draw_commands.items, buffer_size);

      manager.draw_commands.capacity = new_capacity;
   }
   Draw_Index index = { .index = manager.draw_commands.count, .count = 1 };

   manager.draw_commands.count += 1;
   manager.draw_commands.items[index.index] = command;
   manager.draw_commands.dirty = true;

   return index;
}


// Add surface data to global buffers
Draw_Index push_arrays_to_manager(
      Vector3 *positions, Vector3 *normals, Vector2 *uvs, void *joints, u32 vertices_count,
      u32 *indices, u32 indices_count,
      u32 material_index,
      isz base_vertices_offset_override // Can pass -1 to not override anything
) {
   grow_manager_if_needed(vertices_count, indices_count);

   u32 base_vertices_offset = -1 == base_vertices_offset_override ? (u32)manager.vertices.count : (u32)base_vertices_offset_override;

   // Create description for the command
   Draw_Command draw_command = {
      .indices_count   = indices_count,
      .instance_count  = 3,
      .indices_offset  = manager.indices.count,
      .vertices_offset = base_vertices_offset,
      .instance_offset = 0,

      .material_index  = material_index,

      .has_joints      = (joints != nullptr),
      .vertices_count  = vertices_count
   };

   Draw_Index draw_index = push_draw_command_to_manager(draw_command);

   // Copy vertex data to CPU staging arrays
   // The key insight: we store data in separate sections per draw_command
   // Layout: [pos0,pos1,pos2...][norm0,norm1,norm2...][uv0,uv1,uv2...]
   if (vertices_count > 0) {
      memcpy(&manager.vertices.positions[manager.vertices.count], positions, vertices_count * size_of(Vector3));
      memcpy(&manager.vertices.normals  [manager.vertices.count], normals,   vertices_count * size_of(Vector3));
      memcpy(&manager.vertices.uvs      [manager.vertices.count], uvs,       vertices_count * size_of(Vector2));

      // Copy joint data if present
      if (false && joints) {
         memcpy(&manager.joints.items[manager.vertices.count], joints, vertices_count * size_of(manager.joints.items[0]));
         manager.joints.dirty = true;
      }
   }

   assert_msg(indices_count, "I don't see any reason why the number of indices should be zero");
   // Always copy indices (they should already be relative to this draw_command's vertices)
   memcpy(&manager.indices.items[manager.indices.count], indices, indices_count * size_of(u32));

   // Update counters
   manager.vertices.count += vertices_count;
   manager.indices.count += indices_count;

   // Mark buffers as dirty for GPU upload
   manager.vertices.dirty = true;
   manager.indices.dirty = true;

   return draw_index;
}


// Only uploads what changed, using your update_buffer function
void update_manager_gpu_resources() {
   if (manager.vertices.dirty) {
      trace_info("[Manager] Vertices were dirty");

      isz vertex_size = size_of(manager.vertices.positions[0]) + size_of(manager.vertices.normals[0]) +  size_of(manager.vertices.uvs[0]);

      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.vertices.capacity * (size_of(manager.vertices.positions[0]) + size_of(manager.vertices.normals[0]) + size_of(manager.vertices.uvs[0]));
      resize_buffer_if_needed(&manager.vertices.buffer, required_buffer_capacity_in_bytes);

      // We need to pack data as: [all_positions][all_normals][all_uvs]
      // TODO: I dont like this stile of size_of
      isz positions_size = manager.vertices.count * size_of(Vector3);
      isz normals_size   = manager.vertices.count * size_of(Vector3);
      isz uvs_size       = manager.vertices.count * size_of(Vector2);

      isz positions_offset = 0;
      isz normals_offset   = positions_size;
      isz uvs_offset       = positions_size + normals_size;

      // Upload positions first
      update_buffer(&manager.vertices.buffer, manager.vertices.positions, positions_offset, positions_size);

      // Upload normals after positions
      update_buffer(&manager.vertices.buffer, manager.vertices.normals,   normals_offset,   normals_size);

      // Upload uvs after normals
      update_buffer(&manager.vertices.buffer, manager.vertices.uvs,       uvs_offset,       uvs_size);

      manager.vertices.dirty = false;
   }


   if (manager.joints.dirty) {
      goto out;

      trace_info("[Manager] Joints were dirty");
      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.joints.capacity * size_of(manager.joints.items[0]);
      resize_buffer_if_needed(&manager.joints.buffer, required_buffer_capacity_in_bytes);

      isz joints_size = manager.joints.count * size_of(manager.joints.items[0]);
      update_buffer(&manager.joints.buffer, manager.joints.items, 0, joints_size);
      manager.joints.dirty = false;
   }
   out:

   if (manager.indices.dirty) {

      trace_info("[Manager] Indices were dirty");
      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.indices.capacity * size_of(manager.indices.items[0]);
      resize_buffer_if_needed(&manager.indices.buffer, required_buffer_capacity_in_bytes);

      isz indices_size = manager.indices.count * size_of(manager.indices.items[0]);
      update_buffer(&manager.indices.buffer, manager.indices.items, 0, indices_size);
      manager.indices.dirty = false;
   }

   if (manager.draw_commands.dirty) {
      trace_info("[Manager] Draw Commands were dirty");

      isz required_buffer_capacity_in_bytes = manager.draw_commands.capacity * size_of(manager.draw_commands.items[0]);
      resize_buffer_if_needed(&manager.draw_commands.buffer, required_buffer_capacity_in_bytes);

      isz draw_commands_size = manager.draw_commands.count * size_of(manager.draw_commands.items[0]);
      update_buffer(&manager.draw_commands.buffer, manager.draw_commands.items, 0, draw_commands_size);
      manager.draw_commands.dirty = false;
   }

   if (manager.materials.dirty) {
      static Material materials_handles[2048];
      assert_msg(manager.materials.count <= count_of(materials_handles),
         "If we have more than %lld materials per MDI, maybe it's time to do a proper material unloading ok buddy?", (usz)count_of(materials_handles)
      );
      assert_msg(manager.materials.capacity <= count_of(materials_handles),
         "If we have more than %lld materials (in capacity) per MDI, maybe it's time to do a proper material unloading ok buddy?", (usz)count_of(materials_handles)
      );

      // Load all textures from paths should that should be set when pushing all materials.
      isz this_many_needed_loaded = 0, this_many_textures = 0;
      for (u32 material_index = 0; material_index < manager.materials.count; material_index += 1) {
         auto material = &manager.materials.items[material_index];

         if (material->loaded) {
            continue;
         }
         this_many_needed_loaded += 1;

         // TODO: Set a default texture for each of these
         if (material->diffuse.path) {
            material->diffuse = create_texture_from_filepath(material->diffuse.path);
            this_many_textures += 1;
         }
         if (material->specular.path) {
            material->specular = create_texture_from_filepath(material->specular.path);
            this_many_textures += 1;
         }
         if (material->emissive.path) {
            material->emissive = create_texture_from_filepath(material->emissive.path);
            this_many_textures += 1;
         }
         material->loaded = true;

         materials_handles[material_index] = (Material) {
            .diffuse_handle  = material->diffuse.bindless_handle,
            .specular_handle = material->specular.bindless_handle,
            .emissive_handle = material->emissive.bindless_handle,
            .normal_handle   = 69
         };
      }

      isz required_buffer_capacity_in_bytes = manager.materials.capacity * size_of(Material);
      if (!is_valid_buffer(manager.materials.buffer)) {
         manager.materials.buffer = create_buffer(BUFFER_USAGE_SUBDATA, materials_handles, required_buffer_capacity_in_bytes);
      } else {
         resize_buffer_if_needed(&manager.materials.buffer, required_buffer_capacity_in_bytes);

         isz buffer_size = manager.materials.count * size_of(Material);
         update_buffer(&manager.materials.buffer, materials_handles, 0, buffer_size);
      }

      trace_info("[Manager] Materials were dirty and needed to load %lld textures for %lld/%lld materials.", this_many_textures, this_many_needed_loaded, (isz)manager.materials.count);
      manager.materials.dirty = false;
   }
}

// Bind textures for a specific material index from the manager
void bind_material_textures(u32 material_index, Shader shader) {
   if (material_index >= manager.materials.count) {
      // Invalid material index, set defaults
      upload_uniform_bool(shader, "has_specular", false);
      upload_uniform_bool(shader, "has_emissive", false);
      return;
   }

   auto material = &manager.materials.items[material_index];

   // Ensure textures are loaded
   if (!material->loaded) {
      trace_warn("Materials should have been loaded already, but it's not. Loading it for now, but if you keep fucking this us ur done boy.");
      if (material->diffuse.path) {
         material->diffuse = create_texture_from_filepath(material->diffuse.path);
      }
      if (material->specular.path) {
         material->specular = create_texture_from_filepath(material->specular.path);
      }
      if (material->emissive.path) {
         material->emissive = create_texture_from_filepath(material->emissive.path);
      }
      material->loaded = true;
   }

   // TODO: Move this to Draw_Command data
   upload_uniform_bool(shader, "has_specular", false);
   upload_uniform_bool(shader, "has_emissive", false);

   // Bind textures
   if (is_valid_texture(material->diffuse)) {
      bind_texture(material->diffuse, 3);
   }

   if (is_valid_texture(material->specular)) {
      bind_texture(material->specular, 4);
      upload_uniform_bool(shader, "has_specular", true);
   }

   if (is_valid_texture(material->emissive)) {
      bind_texture(material->emissive, 5);
      upload_uniform_bool(shader, "has_emissive", true);
   }
}

// TODO: Should we just draw everything instead of calling this, or is there a point into choosing certain draw items to be draw and others not?
//       We could do a begin_frame end_frame and collect the commands into a buffer and renders those. Or comabine draw_index into draw_index with bigger counts to make it faster. But idk tho
// TODO: change to this instead https://docs.gl/gl4/glDrawElementsIndirect
void draw_from_index(const Draw_Index draw_index, Shader shader) {
   if (draw_index.index + draw_index.count > manager.draw_commands.count) {
      trace_error("%s: You are tripping dawg", __func__);
      return;
   }

   update_manager_gpu_resources();
   glBindVertexArray(manager.vao);
   glVertexArrayElementBuffer(manager.vao, manager.indices.buffer.handle);

   for (u32 sequential_index = 0; sequential_index < draw_index.count;  sequential_index += 1) {
      auto draw_command = &manager.draw_commands.items[draw_index.index + sequential_index];

      bind_material_textures(draw_command->material_index, shader); // NOTE: This load lazily, which might cause spikes

      auto vertex_size = 2*size_of(Vector3) + size_of(Vector2);
      bind_buffer_view(&manager.vertices.buffer, BUFFER_TYPE_STORAGE, 3, 0, manager.vertices.count * vertex_size);
      // Applies vertices_offset to all indices (so draw_command can use local indices 0,1,2...)
      Draw_Command cmd = *draw_command;
      glDrawElementsBaseVertex(GL_TRIANGLES,
         cmd.indices_count,                          // How many indices
         GL_UNSIGNED_INT,                           // Index type
         (void *)(cmd.indices_offset * size_of(u32)), // Where indices start
         cmd.vertices_offset                          // Base vertex offset
      );
   }
}

// Draw all indices ever created
void draw_indirect(Shader shader) {
   update_manager_gpu_resources();

   {
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] %s update_manager_gpu_resources (0x%X).", __func__, err);
      }
   }

   assert_msg(is_valid_buffer(manager.draw_commands.buffer), "Draw Comands Buffer is should always be valid in this function");

   {
      glBindVertexArray(manager.vao);
      glVertexArrayElementBuffer(manager.vao, manager.indices.buffer.handle);
   }

   {
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] %s glBindVertexArray (0x%X).", __func__, err);
      }
   }

   {
      bind_buffer_draw_indirect(&manager.draw_commands.buffer);
      auto draw_commands_size = manager.draw_commands.count * size_of(manager.draw_commands.items[0]);
      bind_buffer_view(&manager.draw_commands.buffer, BUFFER_TYPE_STORAGE, 18, 0, draw_commands_size);
   }

   {
      auto vertices_size = manager.vertices.count * (2*size_of(Vector3) + size_of(Vector2));
      bind_buffer_view(&manager.vertices.buffer,  BUFFER_TYPE_STORAGE, 3, 0, vertices_size);
   }

   {
      auto material_handles_size = manager.materials.count * size_of(Material);
      bind_buffer_view(&manager.materials.buffer, BUFFER_TYPE_STORAGE, 10, 0, material_handles_size);
   }
   // bind_material_textures(manager.draw_commands.items[0].material_index, shader); // NOTE: This mostly wrong

   {
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] %s buffer bindings failed (0x%X).", __func__, err);
      }
   }

   {
      GLint vaoBound = 0, eboBound = 0, dibBound = 0;
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vaoBound);
      glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &eboBound);
      glGetIntegerv(GL_DRAW_INDIRECT_BUFFER_BINDING, &dibBound);
      if (!vaoBound || !eboBound || !dibBound
      ) {
          trace_error("Indirect setup missing: VAO=%d EBO=%d DIB=%d", vaoBound, eboBound, dibBound);
      }

      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] Before glMultiDrawElementsIndirect (0x%X).", err);
      }
   }

   glMultiDrawElementsIndirect(
       GL_TRIANGLES, GL_UNSIGNED_INT,
       (const void *)0,           // No offset into draw command buffer
       manager.draw_commands.count,
       size_of(manager.draw_commands.items[0])      // Stride, 0 if the data is tightly packed
   );

   {
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] glMultiDrawElementsIndirect failed (0x%X).", err);
      }
   }

   // glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
   // glFinish();
}


void grow_materials_if_needed(u32 required_materials) {
   if (manager.materials.count + required_materials <= manager.materials.capacity) {
      return; // No growth needed
   }

   u32 new_capacity = manager.materials.capacity + (manager.materials.capacity / 2);
   if (new_capacity < manager.materials.count + required_materials) {
      new_capacity = manager.materials.count + required_materials;
   }

   manager.materials.items    = realloc(manager.materials.items, new_capacity * size_of(manager.materials.items[0]));
   manager.materials.capacity = new_capacity;
}

// Add a material to the manager and return its index
u32 push_material_to_manager(const char* diffuse_path, const char* specular_path, const char* emissive_path) {
   grow_materials_if_needed(1);

   u32 material_index = manager.materials.count;
   auto material = &manager.materials.items[material_index];

   // Zero Initialize
   material->diffuse  = (Texture){0};
   material->specular = (Texture){0};
   material->emissive = (Texture){0};

   // Store paths (we'll load textures later in load_manager_textures())
   material->diffuse.path  = diffuse_path  ? strdup(diffuse_path)  : nullptr;
   material->specular.path = specular_path ? strdup(specular_path) : nullptr;
   material->emissive.path = emissive_path ? strdup(emissive_path) : nullptr;
   material->loaded = false;


   manager.materials.count += 1;
   manager.materials.dirty = true;

   return material_index;
}


// Push a single mesh to the buffer manager
Draw_Index push_mesh_to_manager(const Mesh *mesh, u32 material_index_base) {
   assert(mesh && is_valid_mesh(*mesh));


   Draw_Index result_draw_index = {0};

   // It's expected and correct that 1 mesh has continuous buffer that surfaces view into. Thats all.
   // If it changes and somehow surfaces has never before seen owned positions data then this will be wrong.
   // Thats why every surface had the same base_vertices_offset
   auto base_vertices_offset = manager.vertices.count;

   // Process each surface as a separate draw_command
   for (u32 surface_index = 0; surface_index < mesh->surfaces.count; surface_index += 1) {
      auto surface = mesh->surfaces.items[surface_index];

      // Extract indices for this surface
      u32 *surface_indices = mesh->indices.items + surface.indices_offset;
      u32 surface_indices_count = surface.indices_count;

      // Calculate the material index
      u32 final_material_index = material_index_base;
      if (surface.material_index >= 0) {
         final_material_index += surface.material_index;
      }

      // Push this surface's data to the manager
      // DONE: This was wasteful, now we avoid repeated postiions.
      u32 vertices_count = (0 == surface_index) ?  mesh->vertices.count : 0;
      Draw_Index draw_index = push_arrays_to_manager(
         mesh->vertices.positions,   // All positions
         mesh->vertices.normals,     // All normals
         mesh->vertices.uvs,         // All UVs
         mesh->vertices.joints,      // Joint data (can be nullptr)
         vertices_count,             // Total vertex count
         surface_indices,            // Surface-specific indices
         surface_indices_count,      // Surface index count
         final_material_index,       // Material index
         base_vertices_offset        // Forcing an offset
      );

      // Detect first time assignment
      if (0 == result_draw_index.count) {
         result_draw_index = draw_index;
      } else {
         if (result_draw_index.index + (result_draw_index.count - 1) == draw_index.index) {
            trace_error("For some reason the draw_indexes from mesh is not sequencial, find out why.");
            exit(1);
         }
         result_draw_index.count += draw_index.count;
      }
   }

   return result_draw_index;
}

// Push entire model to buffer manager, handling all meshes and materials
Draw_Index push_model_to_manager(const Model *model) {
   assert(model != nullptr);
   assert(model->meshes.items != nullptr);
   assert(model->meshes.count > 0);


   // First, push all materials from the model to the manager
   u32 material_index_base = manager.materials.count;
   for (isz material_index = 0; material_index < model->materials.count; material_index += 1) {
      auto material = model->materials.items[material_index];

      push_material_to_manager(material.diffuse, material.specular, material.emissive);
   }

   Draw_Index result_draw_index = {0};

   for (isz mesh_index = 0; mesh_index < model->meshes.count; mesh_index += 1) {
      Mesh *mesh = &model->meshes.items[mesh_index];
      Draw_Index mesh_draw_item = push_mesh_to_manager(mesh, material_index_base);

      // Detect first time assignment
      if (0 == result_draw_index.count) {
         result_draw_index = mesh_draw_item;
      } else {
         if (result_draw_index.index + (result_draw_index.count - 1) == mesh_draw_item.index) {
            trace_error("For some reason the draw_indexes from mesh is not sequencial, find out why.");
            exit(1);
         }
         result_draw_index.count += mesh_draw_item.count;
      }
   }

   return result_draw_index;
}


void destroy_manager_materials() {
   for (u32 i = 0; i < manager.materials.count; i += 1) {
      auto material = &manager.materials.items[i];

      // Yes we allocated the path
      free((void *)material->diffuse.path);
      free((void *)material->specular.path);
      free((void *)material->emissive.path);

      destroy_texture(&material->diffuse);
      destroy_texture(&material->specular);
      destroy_texture(&material->emissive);
   }

   free(manager.materials.items);
   manager.materials.items = nullptr;
   manager.materials.count = 0;
   manager.materials.capacity = 0;
}



// Initialize the global buffer system
void init_manager() {
   manager = (Manager){0};
   u32 intial_capacity = 64;
   u32 initial_vertex_capacity       = intial_capacity;
   u32 initial_index_capacity        = intial_capacity;
   u32 initial_material_capacity     = intial_capacity;
   u32 initial_draw_command_capacity = intial_capacity;
   // Zero-initialize the manager

   // Initialize capacities
   manager.vertices.capacity      = initial_vertex_capacity;
   manager.indices.capacity       = initial_index_capacity;
   manager.joints.capacity        = initial_vertex_capacity;
   manager.materials.capacity     = initial_material_capacity;
   manager.draw_commands.capacity = initial_draw_command_capacity;

   // Allocate CPU staging arrays
   manager.vertices.positions  = malloc(initial_vertex_capacity * size_of(manager.vertices.positions[0]));
   manager.vertices.normals    = malloc(initial_vertex_capacity * size_of(manager.vertices.normals[0]));
   manager.vertices.uvs        = malloc(initial_vertex_capacity * size_of(manager.vertices.uvs[0]));
   manager.indices.items       = malloc(initial_index_capacity  * size_of(manager.indices.items[0]));
   manager.joints.items        = malloc(initial_vertex_capacity * size_of(manager.joints.items[0]));
   // Allocate materials and draw_commands arrays
   manager.draw_commands.items = malloc(initial_draw_command_capacity * size_of(manager.draw_commands.items[0]));

   manager.materials.items     = malloc(initial_material_capacity * size_of(manager.materials.items[0]));

   auto buffer_flag = BUFFER_USAGE_SUBDATA;
   // auto buffer_flag = BUFFER_USAGE_ORPHANABLE;

   // Create GPU buffers
   isz vertex_item_size     = size_of(manager.vertices.positions[0]) + size_of(manager.vertices.normals[0]) + size_of(manager.vertices.uvs[0]);
   isz vertex_buffer_size   = initial_vertex_capacity * vertex_item_size;
   manager.vertices.buffer  = create_buffer(buffer_flag, nullptr, vertex_buffer_size);

   isz joints_buffer_size   = initial_vertex_capacity * size_of(manager.joints.items[0]);
   manager.joints.buffer    = create_buffer(buffer_flag, nullptr, joints_buffer_size);


   isz indices_buffer_size = initial_index_capacity * size_of(manager.indices.items[0]);
   manager.indices.buffer   = create_buffer(buffer_flag, nullptr, indices_buffer_size);

   isz draw_commands_buffer_size = initial_index_capacity * size_of(manager.draw_commands.items[0]);
   manager.draw_commands.buffer  = create_buffer(buffer_flag, nullptr, draw_commands_buffer_size);

   // Create VAO
   glCreateVertexArrays(1, &manager.vao);
   glVertexArrayElementBuffer(manager.vao, manager.indices.buffer.handle);

   // Initialize counters
   manager.vertices.count      = 0;
   manager.indices.count       = 0;
   manager.joints.count        = 0;
   manager.materials.count     = 0;
   manager.draw_commands.count = 0;

   // Mark all as clean initially
   manager.vertices.dirty      = false;
   manager.joints.dirty        = false;
   manager.indices.dirty       = false;
   manager.materials.dirty     = false;
   manager.draw_commands.dirty = false;
}


typedef struct {
    u32 index_count;
    u32 instance_count;  // For instanced rendering (usually 1)
    u32 index_offset;    // Start index in index *not byte*
    i32 vertex_offset;   // Base Vertex in index *not byte*
    u32 instance_offset; // Base Instance in index *not byte*
    // Optional user-defined data goes here - if nothing, stride is 0


   /*
      unsigned int * indices = (unsigned int *)ELEMENT_ARRAY_BUFFER;
      for (DrawElementsIndirectCommand cmd : GL_DRAW_INDIRECT_BUFFER) {
          for (uint i = 0; i < cmd.count; ++i) {
              int gl_VertexID = indices[cmd.firstIndex + i] + cmd.baseVertex;
          }
      }
   */

   /* source: https://ktstephano.github.io/rendering/opengl/mdi
gl_VertexID
   Vertex index with first index and base vertex offset
gl_InstanceID
   Current instance whenever instanceCount > 1, else 0
gl_DrawID
   The current draw command index we are on inside of the GL_DRAW_INDIRECT_BUFFER.
   So if you submitted 30 draw commands in the buffer, this value will range from 0 to 29.
   Useful for a situation such as needing to access a different transform matrix depending on the current draw command number.
gl_BaseVertex
   Base vertex of current draw command
gl_BaseInstance
   Base instance of current draw command (can use this to pass in any integer data you want if not using instanced vertex attributes)
   */
} Draw_Command;

typedef struct {
   Draw_Command draw_command;
   u32  material_index;    // Material index
   u32  vertex_count;      // How many vertices there are (1 vertex means that we have exaclty 1 position 1 normal 1 uv + 1 joints if skeleton is present)
   bool has_joints;        // Whether this renderable uses joint data
} Renderable;


// Global buffer system with separate attribute buffers
typedef struct {
   // Separate attribute buffers (non-interleaved)
   Buffer vertex_buffer; // Sequence of Vertex data. First renderable will fill with all its Positions then Normals, then Uvs. Then we are back to Positions again for the next Renderable and so on
   Buffer joints_buffer; // Optional, but likely
   Buffer index_buffer;
   Buffer indirect_buffer; // Draw Commands Buffer unused for now
   Buffer renderable_buffer;  // Array os renderable that will be used in the glsl to do vertex pulling of both
   bool vertex_dirty;
   bool joints_dirty;
   bool indices_dirty;
   bool indirect_dirty;

   // CPU staging arrays
   Vector3 *positions;
   Vector3 *normals;
   Vector2 *uvs;
   struct {
      // Order is important
      Vector4Int joint_indices;
      Vector4    joint_weights;
      // should have one of each per position or none
   } *joints;
   u32 vertex_count;
   u32 vertex_capacity;

   u32 *indices;
   u32 index_count;
   u32 index_capacity;

   // Material management
   struct {
      const char *diffuse_path;
      const char *specular_path;
      const char *emissive_path;
      Texture diffuse;
      Texture specular;
      Texture emissive;
      bool    textures_loaded; // Track if paths have been converted to textures
   } *materials;
   u32  material_count;
   u32  material_capacity;
   bool materials_dirty;

   // Renderable management
   Renderable *renderables;
   u32 renderable_count;
   u32 renderable_capacity;

   Draw_Command *draw_commands;
   u32 draw_command_count;
   u32 draw_command_capacity;

   GLuint vao;

} Manager;

static Manager manager = {0};

typedef struct {
    Renderable *items;
    u32 count;
} Model_Renderables;

void grow_manager_if_needed(u32 required_vertices, u32 required_indices) {
   // Check if we need to grow vertex arrays
   if (manager.vertex_count + required_vertices > manager.vertex_capacity) {
      u32 new_capacity = manager.vertex_capacity + (manager.vertex_capacity / 2);
      if (new_capacity < manager.vertex_count + required_vertices) {
         new_capacity = manager.vertex_count + required_vertices;
      }

      // Reallocate CPU arrays
      manager.positions = realloc(manager.positions, new_capacity * size_of(manager.positions[0]));
      manager.normals   = realloc(manager.normals,   new_capacity * size_of(manager.normals  [0]));
      manager.uvs       = realloc(manager.uvs,       new_capacity * size_of(manager.uvs      [0]));

      // Use the existing resize_buffer_if_needed function for GPU buffer
      // Buffer layout: [all positions][all normals][all uvs] per renderable
      isz buffer_size = new_capacity * (size_of(manager.positions[0]) + size_of(manager.normals[0]) + size_of(manager.uvs[0]));
      resize_buffer_if_needed(&manager.vertex_buffer, buffer_size);

      manager.vertex_capacity = new_capacity;
      manager.vertex_dirty    = true; // Mark for full upload
   }

      // manager.joints    = realloc(manager.joints,    new_capacity * size_of(*manager.joints));

   // Check if we need to grow index buffer
   if (manager.index_count + required_indices > manager.index_capacity) {
      u32 new_capacity = manager.index_capacity + (manager.index_capacity / 2);
      if (new_capacity < manager.index_count + required_indices) {
         new_capacity = manager.index_count + required_indices;
      }

      manager.indices = realloc(manager.indices, new_capacity * size_of(u32));

      isz buffer_size = new_capacity * size_of(u32);
      resize_buffer_if_needed(&manager.index_buffer, buffer_size);

      manager.index_capacity = new_capacity;
      manager.indices_dirty = true;
   }
}

// Add mesh data to global buffers
Renderable push_arrays_to_manager(Vector3 *positions, Vector3 *normals, Vector2 *uvs,
                                 void *joints, u32 vertex_count, u32 *indices, // Changed Joint_Data to void*
                                 u32 index_count, u32 material_index) {
   // Grow buffers if needed
   grow_manager_if_needed(vertex_count, index_count);

   // Create renderable descriptor
   Renderable renderable = {
      .draw_command = {
         .vertex_offset  = manager.vertex_count,
         .index_offset   = manager.index_count,
         .index_count    = index_count,
         .instance_count = 1
      },
      .has_joints   = (joints != nullptr),
      .vertex_count = vertex_count
   };

   // Copy vertex data to CPU staging arrays
   // The key insight: we store data in separate sections per renderable
   // Layout: [pos0,pos1,pos2...][norm0,norm1,norm2...][uv0,uv1,uv2...]
   memcpy(&manager.positions[manager.vertex_count], positions, vertex_count * size_of(Vector3));
   memcpy(&manager.normals[manager.vertex_count], normals, vertex_count * size_of(Vector3));
   memcpy(&manager.uvs[manager.vertex_count], uvs, vertex_count * size_of(Vector2));

   // Copy joint data if present
   if (joints) {
      memcpy(&manager.joints[manager.vertex_count], joints, vertex_count * size_of(*manager.joints));
      manager.joints_dirty = true;
   }

   // Copy indices (they should already be relative to this renderable's vertices)
   memcpy(&manager.indices[manager.index_count], indices, index_count * size_of(u32));

   // Update counters
   manager.vertex_count += vertex_count;
   manager.index_count += index_count;

   // Mark buffers as dirty for GPU upload
   manager.vertex_dirty = true;
   manager.indices_dirty = true;

   return renderable;
}

// Only uploads what changed, using your update_buffer function
void update_manager_gpu_buffers() {
   if (manager.vertex_dirty) {
      // We need to pack data as: [all_positions][all_normals][all_uvs]
      // TODO: I dont like this stile of size_of
      isz positions_size = manager.vertex_count * size_of(Vector3);
      isz normals_size   = manager.vertex_count * size_of(Vector3);
      isz uvs_size       = manager.vertex_count * size_of(Vector2);

      isz positions_offset = 0;
      isz normals_offset   = positions_size;
      isz uvs_offset       = positions_size + normals_size;

      // Upload positions first
      update_buffer(&manager.vertex_buffer, manager.positions, positions_offset, positions_size);

      // Upload normals after positions
      update_buffer(&manager.vertex_buffer, manager.normals,   normals_offset,   normals_size);

      // Upload uvs after normals
      update_buffer(&manager.vertex_buffer, manager.uvs,       uvs_offset,       uvs_size);

      manager.vertex_dirty = false;
   }

   if (manager.joints_dirty) {
      isz joints_size = manager.vertex_count * size_of(*manager.joints);
      update_buffer(&manager.joints_buffer, manager.joints, 0, joints_size);
      manager.joints_dirty = false;
   }

   if (manager.indices_dirty) {
      isz indices_size = manager.index_count * size_of(u32);
      update_buffer(&manager.index_buffer, manager.indices, 0, indices_size);
      manager.indices_dirty = false;
   }

   // NOTE: indirect_buffer update would go here
}

void draw_renderable(const Renderable *renderable) {
   update_manager_gpu_buffers();
   glBindVertexArray(manager.vao);
   glVertexArrayElementBuffer(manager.vao, manager.index_buffer.handle);
   glVertexArrayVertexBuffer (manager.vao, 0, manager.vertex_buffer.handle, 0, 8*size_of(float));


   auto vertex_size = 2*size_of(Vector3) + size_of(Vector2);
   bind_buffer_view(&manager.vertex_buffer, BUFFER_TYPE_STORAGE, 3, 0, manager.vertex_count * vertex_size);
   // Applies vertex_offset to all indices (so renderable can use local indices 0,1,2...)
   Draw_Command cmd = renderable->draw_command;
   glDrawElementsBaseVertex(GL_TRIANGLES,
      cmd.index_count,                           // How many indices
      GL_UNSIGNED_INT,                           // Index type
      (void *)(cmd.index_offset * size_of(u32)), // Where indices start
      cmd.vertex_offset                          // Base vertex offset
   );
}

// Push a single mesh to the buffer manager, returns array of renderables (one per surface)
Renderable* push_mesh_to_manager(const Mesh *mesh, u32 material_index_base, u32 *out_renderable_count) {
   assert(mesh && is_valid_mesh(*mesh));


   *out_renderable_count = mesh->surfaces.count;
   Renderable *renderables = malloc(mesh->surfaces.count * size_of(Renderable));

   // Process each surface as a separate renderable
   for (u32 surface_idx = 0; surface_idx < mesh->surfaces.count; surface_idx++) {
      auto surface = mesh->surfaces.items[surface_idx];

      // Extract indices for this surface
      u32 *surface_indices = mesh->indices.items + surface.indices_offset;
      u32 surface_indices_count = surface.indices_count;

      // Calculate the material index
      u32 final_material_index = material_index_base;
      if (surface.material_index >= 0) {
         final_material_index += surface.material_index;
      }

      // Push this surface's data to the manager
      renderables[surface_idx] = push_arrays_to_manager(
         mesh->vertices.positions,       // All positions
         mesh->vertices.normals,         // All normals
         mesh->vertices.uvs,             // All UVs
         mesh->vertices.joints,      // Joint data (can be nullptr)
         mesh->vertices.count,           // Total vertex count
         surface_indices,                     // Surface-specific indices
         surface_indices_count,               // Surface index count
         final_material_index                 // Material index
      );
   }

   return renderables;
}

void grow_materials_if_needed(u32 required_materials) {
   if (manager.material_count + required_materials <= manager.material_capacity) {
      return; // No growth needed
   }

   u32 new_capacity = manager.material_capacity + (manager.material_capacity / 2);
   if (new_capacity < manager.material_count + required_materials) {
      new_capacity = manager.material_count + required_materials;
   }

   manager.materials = realloc(manager.materials, new_capacity * size_of(*manager.materials));
   manager.material_capacity = new_capacity;
}

// Add a material to the manager and return its index
u32 push_material_to_manager(const char* diffuse_path, const char* specular_path, const char* emissive_path) {
   grow_materials_if_needed(1);

   u32 material_index = manager.material_count;
   auto material = &manager.materials[material_index];

   // Store paths (we'll load textures later in load_manager_textures())
   material->diffuse_path = diffuse_path ? strdup(diffuse_path) : nullptr;
   material->specular_path = specular_path ? strdup(specular_path) : nullptr;
   material->emissive_path = emissive_path ? strdup(emissive_path) : nullptr;
   material->textures_loaded = false;

   // Initialize texture handles to 0
   material->diffuse = (Texture){0};
   material->specular = (Texture){0};
   material->emissive = (Texture){0};

   manager.material_count++;
   manager.materials_dirty = true;

   return material_index;
}

// Push entire model to buffer manager, handling all meshes and materials
Model_Renderables push_model_to_manager(const Model *model) {
   assert(model != nullptr);
   assert(model->meshes.items != nullptr);
   assert(model->meshes.count > 0);

   Model_Renderables result = {0};

   // First, push all materials from the model to the manager
   u32 material_index_base = manager.material_count;
   for (isz material_idx = 0; material_idx < model->materials.count; material_idx++) {
      auto material = model->materials.items[material_idx];

      push_material_to_manager(material.diffuse, material.specular, material.emissive);
   }

   // Count total renderables needed (sum of all surfaces across all meshes)
   u32 total_renderables = 0;
   for (isz mesh_idx = 0; mesh_idx < model->meshes.count; mesh_idx++) {
      total_renderables += model->meshes.items[mesh_idx].surfaces.count;
   }

   // Allocate space for all renderables
   result.items = malloc(total_renderables * size_of(Renderable));
   result.count = total_renderables;

   // Process each mesh and collect renderables
   u32 renderable_index = 0;
   for (isz mesh_idx = 0; mesh_idx < model->meshes.count; mesh_idx++) {
      Mesh *mesh = &model->meshes.items[mesh_idx];

      u32 mesh_renderable_count;
      Renderable *mesh_renderables = push_mesh_to_manager(mesh, material_index_base, &mesh_renderable_count);

      // Copy mesh renderables to result array
      memcpy(&result.items[renderable_index], mesh_renderables, mesh_renderable_count * size_of(Renderable));

      renderable_index += mesh_renderable_count;

      // Free temporary mesh renderables array
      free(mesh_renderables);
   }

   return result;
}

// Bind textures for a specific material index from the manager
void bind_material_textures(u32 material_index, Shader shader) {
   if (material_index >= manager.material_count) {
      // Invalid material index, set defaults
      upload_uniform_bool(shader, "has_specular", false);
      upload_uniform_bool(shader, "has_emissive", false);
      return;
   }

   auto material = &manager.materials[material_index];

   // Ensure textures are loaded
   if (!material->textures_loaded) {
      if (material->diffuse_path) {
         material->diffuse = create_texture_from_filepath(material->diffuse_path);
      }
      if (material->specular_path) {
         material->specular = create_texture_from_filepath(material->specular_path);
      }
      if (material->emissive_path) {
         material->emissive = create_texture_from_filepath(material->emissive_path);
      }
      material->textures_loaded = true;
   }

   // Set default states
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

// Draw a specific renderable with its material
void draw_model_renderable(const Renderable *renderable, Shader shader) {
   assert(renderable != nullptr);

   // Bind material textures
   bind_material_textures(renderable->material_index, shader);

   // Draw the renderable
   draw_renderable(renderable);
}

// Draw all renderables from a model
void draw_model_renderables(const Model_Renderables *model_data, Shader shader) {
   assert(model_data != nullptr);

   for (u32 i = 0; i < model_data->count; i++) {
      draw_model_renderable(&model_data->items[i], shader);
   }
}

// Cleanup function to free model renderables data (materials stay in manager)
void destroy_model_renderables(Model_Renderables *model_data) {
   if (!model_data)
      return;

   // Free renderables array
   if (model_data->items) {
      free(model_data->items);
   }

   *model_data = (Model_Renderables){0};
}

// Cleanup materials in manager (call when shutting down)
void destroy_manager_materials() {
   for (u32 i = 0; i < manager.material_count; i++) {
      auto material = &manager.materials[i];

      // Free path strings
      free((void *)material->diffuse_path);
      free((void *)material->specular_path);
      free((void *)material->emissive_path);

      // Destroy textures
      destroy_texture(&material->diffuse);
      destroy_texture(&material->specular);
      destroy_texture(&material->emissive);
   }

   free(manager.materials);
   manager.materials = nullptr;
   manager.material_count = 0;
   manager.material_capacity = 0;
}


// Load all textures from paths (call this after pushing all materials)
void load_manager_textures() {
   for (u32 i = 0; i < manager.material_count; i++) {
      auto material = &manager.materials[i];

      if (material->textures_loaded)
         continue;

      if (material->diffuse_path) {
         material->diffuse = create_texture_from_filepath(material->diffuse_path);
      }
      if (material->specular_path) {
         material->specular = create_texture_from_filepath(material->specular_path);
      }
      if (material->emissive_path) {
         material->emissive = create_texture_from_filepath(material->emissive_path);
      }
      material->textures_loaded = true;
   }
   manager.materials_dirty = false;
}

// Initialize the global buffer system
void init_manager() {
   manager = (Manager){0};
   u32 initial_memory = 64;
   u32 initial_vertex_capacity     = initial_memory;
   u32 initial_index_capacity      = initial_memory;
   u32 initial_material_capacity   = initial_memory;
   u32 initial_renderable_capacity = initial_memory;
   // Zero-initialize the manager

   // Initialize capacities
   manager.vertex_capacity     = initial_vertex_capacity;
   manager.index_capacity      = initial_index_capacity;
   manager.material_capacity   = initial_material_capacity;
   manager.renderable_capacity = initial_renderable_capacity;

   // Allocate CPU staging arrays
   manager.positions = malloc(initial_vertex_capacity * size_of(Vector3));
   manager.normals   = malloc(initial_vertex_capacity * size_of(Vector3));
   manager.uvs       = malloc(initial_vertex_capacity * size_of(Vector2));
   manager.joints    = malloc(initial_vertex_capacity * size_of(*manager.joints));
   manager.indices   = malloc(initial_index_capacity  * size_of(u32));

   // Allocate materials and renderables arrays
   manager.materials   = malloc(initial_material_capacity * size_of(*manager.materials));
   manager.renderables = malloc(initial_renderable_capacity * size_of(Renderable));

   // Create GPU buffers
   isz vertex_buffer_size = initial_vertex_capacity * (size_of(Vector3) + size_of(Vector3) + size_of(Vector2));
   manager.vertex_buffer  = create_buffer(BUFFER_USAGE_SUBDATA_RESIZABLE, nullptr, vertex_buffer_size);

   isz joints_buffer_size = initial_vertex_capacity * size_of(*manager.joints);
   manager.joints_buffer  = create_buffer(BUFFER_USAGE_SUBDATA_RESIZABLE, nullptr, joints_buffer_size);


   isz inidices_buffer_size = initial_index_capacity * size_of(u32);
   manager.index_buffer   = create_buffer(BUFFER_USAGE_SUBDATA_RESIZABLE, nullptr, inidices_buffer_size);

   // Create VAO
   glCreateVertexArrays(1, &manager.vao);
   glVertexArrayElementBuffer(manager.vao, manager.index_buffer.handle);

   // Initialize counters
   manager.vertex_count       = 0;
   manager.index_count        = 0;
   manager.material_count     = 0;
   manager.renderable_count   = 0;
   manager.draw_command_count = 0;

   // Mark all as clean initially
   manager.vertex_dirty    = false;
   manager.joints_dirty    = false;
   manager.indices_dirty   = false;
   manager.indirect_dirty  = false;
   manager.materials_dirty = false;
}


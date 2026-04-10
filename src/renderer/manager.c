 // Match glsl types
typedef u64        uvec2;
typedef u32        uint;
typedef Vector4Int ivec4;
typedef Vector4    vec4;
typedef float16    mat4;
#include "./shared/types.glsl"

typedef struct Joint_Vertex Joint_Vertex;
typedef struct Draw_Command Draw_Command;
typedef struct Material     Material;


typedef struct {
   u32 base;  // Index into draw_commands.items
   u32 count; // Allocated in sequence, Index + 0, Index + 1, ..., Index + count-1
} Draw_Index;

typedef struct {
   u32 base;  // Index into draw_commands.items
   u32 count; // Allocated in sequence, Index + 0, Index + 1, ..., Index + count-1
} Animation_Index;

typedef struct {
   isz index;
} Scene_Node;

constexpr int MAX_TEXTURE_PER_MATERIAL = 4;

static_assert(size_of(Draw_Command) % 16 == 0);

// #define WE_ARE_DOING_SYNC_OURSELVES

#ifdef WE_ARE_DOING_SYNC_OURSELVES
   constexpr auto instances_buffer_usage                  = BUFFER_USAGE_MAP_PERSISTENT_WRITE;
   constexpr auto geometry_to_world_matrices_buffer_usage = BUFFER_USAGE_MAP_PERSISTENT_WRITE;
#else
   constexpr auto instances_buffer_usage                  = BUFFER_USAGE_SUBDATA;
   constexpr auto geometry_to_world_matrices_buffer_usage = BUFFER_USAGE_SUBDATA;
#endif

// Global buffer system with separate attribute buffers
// Unalignment goes crazy with all these dirty flags @Flag
typedef struct {
   // CPU staging arrays

   // TODO: Unify these type of buffers for ease of development.

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
      Vector4 *items;
      u32    count;
      u32    capacity;
      Buffer buffer;
      bool   dirty;
   } tangents;

   struct {
      Joint_Vertex *items;
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

   struct {
      Animation *items;
      u32 count;
      u32 capacity;
   } animations;

   // Material management
   struct {
      struct {
         union{
            Texture textures[MAX_TEXTURE_PER_MATERIAL];
            struct {
               Texture diffuse;
               Texture specular;
               Texture emissive;
               Texture normal;
            };
         };
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


   // TODO: I really not satisfied this Renderable Abstraction, can't be Entity either. Think of something better for a render unit thing that has instances and draw_commands associated with it
   //       We can't really associate draw_commands with instances because various times many draw_commands as associated with only 1 instance, it would be wasteful to allocate gpu space for replicate instance data for each draw_command.
   struct {
      struct {
         // Instances and Draw Comamnds are related as we have always less or equal ren than we draw_commands
         struct {
            Draw_Index draw_index;
            Animation_Index animation_index;
            Joint_List joint_list; // NOTE: It's not that lean of a structure, maybe we sould make use an index instead?
            // TODO: Make it possible to share 1 Animation between instances
            struct {
               struct {
                  f64  animation_current_time;
                  f64  animation_speed;
                  uint animation_number;
                  // isz animation_last_keyframe_index; // Read animation.c comment to get some insight of what we might need to do to speed up finding keypair
                  Transform transform;
                  Geometry_To_World_List geometry_to_world_matrices;
                  struct { // TODO: make it possible to have instances with differentes materials considering surfaces in a mesh
                     u32 base;
                     u32 count;
                  } material_index;
               } *items;
               u32 count;
               u32 capacity;
            } instances;
         } *items; // Instances for a renderable unit
         u32 count;
         u32 capacity;
      } renderables;

      DArray(struct {
         isz node_parent;
         isz instance_index;
         isz renderable_index;
      }) nodes;

      Buffer instances_buffer;
      Buffer geometry_to_world_matrices_buffer;
      bool  instances_dirty;
   } scene;

   GLuint vao; // TODO: Remove, the renderer should simply have one vertex array for everything and bind just the index buffer when needed
} Manager;

static Manager manager = {0};


bool overload has_animation(Scene_Node node) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   const auto *renderable = &manager.scene.renderables.items[renderable_index];
   if (renderable->animation_index.count < 1 || renderable->joint_list.count <= 0) {
      return false;
   }
   return true;
}


// Why is this taking u32?
void grow_manager_if_needed(u32 required_vertices, u32 required_indices, bool has_joints, bool has_tangents) {
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

   u32 required_joints = required_vertices;
   if (has_joints && (manager.joints.count + required_joints > manager.joints.capacity)) {
      u32 new_capacity = manager.joints.capacity + (manager.joints.capacity / 2);
      if (new_capacity < manager.joints.count + required_joints) {
         new_capacity = manager.joints.count + required_joints;
      }

      // Reallocate CPU arrays
      manager.joints.items = realloc(manager.joints.items, new_capacity * size_of(manager.joints.items[0]));

      manager.joints.capacity = new_capacity;
      manager.joints.dirty    = true; // Mark for full upload
   }

   u32 required_tangents = required_vertices;
   if (has_tangents && (manager.tangents.count + required_tangents > manager.tangents.capacity)) {
      u32 new_capacity = manager.tangents.capacity + (manager.tangents.capacity / 2);
      if (new_capacity < manager.tangents.count + required_tangents) {
         new_capacity = manager.tangents.count + required_tangents;
      }

      // Reallocate CPU arrays
      manager.tangents.items = realloc(manager.tangents.items, new_capacity * size_of(manager.tangents.items[0]));

      manager.tangents.capacity = new_capacity;
      manager.tangents.dirty    = true; // Mark for full upload
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
   const isz required_draw_commands = 1;
   if (manager.draw_commands.count + required_draw_commands > manager.draw_commands.capacity) {
      u32 new_capacity = manager.draw_commands.capacity + (manager.draw_commands.capacity / 2);
      if (new_capacity < manager.draw_commands.count + required_draw_commands) {
         new_capacity = manager.draw_commands.count + required_draw_commands;
      }

      isz buffer_size = new_capacity * size_of(manager.draw_commands.items[0]);
      manager.draw_commands.items = realloc(manager.draw_commands.items, buffer_size);

      manager.draw_commands.capacity = new_capacity;
   }
   Draw_Index index = { .base = manager.draw_commands.count, .count = 1 };

   manager.draw_commands.count += 1;
   manager.draw_commands.items[index.base] = command;
   manager.draw_commands.dirty = true;

   return index;
}



// Add surface data to global buffers
Draw_Index push_arrays_to_manager(
      Vector3 *positions, Vector3 *normals, Vector2 *uvs, Vector4 *tangents, void *joints, u32 vertices_count,
      u32 *indices, u32 indices_count,
      u32 material_index,
      isz base_vertices_offset_override, // Can pass -1 to not override anything
      isz base_tangents_offset_override,  // Can pass -1 to not override anything
      isz base_joints_offset_override   // Can pass -1 to not override anything
) {
   bool has_joints = joints != nullptr;
   bool has_tangents = tangents != nullptr;
   grow_manager_if_needed(vertices_count, indices_count, has_joints, has_tangents);

   u32 base_vertices_offset = -1 == base_vertices_offset_override   ? (u32)manager.vertices.count : (u32)base_vertices_offset_override;
   u32 base_joints_offset   = -1 == base_joints_offset_override     ? (u32)manager.joints.count   : (u32)base_joints_offset_override;
   u32 base_tangents_offset = -1 == base_tangents_offset_override   ? (u32)manager.tangents.count : (u32)base_tangents_offset_override;

   // Create description for the command
   Draw_Command draw_command = {
      .indices_count   = indices_count,
      .instance_count  = 0,
      .indices_offset  = manager.indices.count,
      .vertices_offset = base_vertices_offset,
      .instance_offset = 0,

      .tangents_offset = base_tangents_offset_override,
      .has_tangents    = has_tangents,
      .joints_offset   = base_joints_offset,
      .has_joints      = has_joints,

      .material_index  = material_index,
      .vertices_count  = vertices_count
   };

   Draw_Index draw_index = push_draw_command_to_manager(draw_command);

   // Copy vertex data to CPU staging arrays
   // We store data in separate sections per draw_command
   // Layout: [pos0,pos1,pos2...][norm0,norm1,norm2...][uv0,uv1,uv2...]
   if (vertices_count > 0) {
      memcpy(&manager.vertices.positions[manager.vertices.count], positions, vertices_count * size_of(Vector3));
      memcpy(&manager.vertices.normals  [manager.vertices.count], normals,   vertices_count * size_of(Vector3));
      memcpy(&manager.vertices.uvs      [manager.vertices.count], uvs,       vertices_count * size_of(Vector2));

      // Copy joint data if present, same count as vertices but different cpu buffer
      if (has_joints) {
         memcpy(&manager.joints.items[manager.joints.count], joints, vertices_count * size_of(manager.joints.items[0]));
      }

      if (has_tangents) {
         memcpy(&manager.tangents.items[manager.tangents.count], tangents, vertices_count * size_of(manager.tangents.items[0]));
      }
   }

   assert_msg(indices_count, "I don't see any reason why the number of indices should be zero");
   // Always copy indices (they should already be relative to this draw_command's vertices)
   memcpy(&manager.indices.items[manager.indices.count], indices, indices_count * size_of(u32));

   // Update counters
   manager.vertices.count += vertices_count;
   if (has_tangents) {
      manager.tangents.count += vertices_count;
   }
   if (has_joints) {
      manager.joints.count += vertices_count;
   }
   manager.indices.count += indices_count;

   // Mark buffers as dirty for GPU upload
   manager.vertices.dirty = true;
   if (has_tangents) {
      manager.tangents.dirty = true;
   }
   if (has_joints) {
      manager.joints.dirty = true;
   }
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


   if (manager.tangents.dirty) {
      trace_info("[Manager] tangents were dirty");
      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.tangents.capacity * size_of(manager.tangents.items[0]);
      resize_buffer_if_needed(&manager.tangents.buffer, required_buffer_capacity_in_bytes);

      isz tangents_size = manager.tangents.count * size_of(manager.tangents.items[0]);
      update_buffer(&manager.tangents.buffer, manager.tangents.items, 0, tangents_size);
      manager.tangents.dirty = false;
   }

   if (manager.joints.dirty) {
      trace_info("[Manager] Joints were dirty");
      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.joints.capacity * size_of(manager.joints.items[0]);
      resize_buffer_if_needed(&manager.joints.buffer, required_buffer_capacity_in_bytes);

      isz joints_size = manager.joints.count * size_of(manager.joints.items[0]);
      update_buffer(&manager.joints.buffer, manager.joints.items, 0, joints_size);
      manager.joints.dirty = false;
   }

   if (manager.indices.dirty) {
      trace_info("[Manager] Indices were dirty");

      // Always sync the gpu buffer size to the cpu capacity, not the cpu size just so we do less resizes as resize won't occurs if we already have enough
      isz required_buffer_capacity_in_bytes = manager.indices.capacity * size_of(manager.indices.items[0]);
      resize_buffer_if_needed(&manager.indices.buffer, required_buffer_capacity_in_bytes);

      isz indices_size = manager.indices.count * size_of(manager.indices.items[0]);
      update_buffer(&manager.indices.buffer, manager.indices.items, 0, indices_size);
      manager.indices.dirty = false;
   }

   if (manager.scene.instances_dirty) {
      isz instances_count = 0;
      // Count instances
      for (u32 renderable_index = 0; renderable_index < manager.scene.renderables.count; renderable_index += 1) {
         auto renderable = manager.scene.renderables.items[renderable_index];
         instances_count += renderable.instances.count;
      }

      isz total_geometry_matrices_count = 0;
      {  // Instances buffer management
         // TODO: Change this to permanently mapped pointer instead and create independent dirty flag for instances
         isz instance_size = size_of(struct Instance);
         isz required_instances_buffer_capacity_in_bytes = instances_count * instance_size;

         auto checkpoint = tsave();
         struct Instance* gpu_instances = talloc(required_instances_buffer_capacity_in_bytes);
         isz linear_instance_index  = 0;
         isz linear_matrices_offset = 0;
         for (u32 renderable_index = 0; renderable_index < manager.scene.renderables.count; renderable_index += 1) {
            auto renderable = manager.scene.renderables.items[renderable_index];
            for (u32 instance_index = 0; instance_index < renderable.instances.count; instance_index += 1) {
               auto instance = renderable.instances.items[instance_index];
               auto geometry_to_world_matrices = instance.geometry_to_world_matrices;

               assert(linear_instance_index < instances_count);
               gpu_instances[linear_instance_index].model_matrix = MatrixToFloatV(MatrixCompose(instance.transform));
               gpu_instances[linear_instance_index].geometry_to_model_offset = linear_matrices_offset;
               linear_instance_index  += 1;
               // NOTE: Either make geometry_to_world_matrices always instance available (rn is lazy from play_animation) or use joint_list from renderable
               // linear_matrices_offset += instance.geometry_to_world_matrices.count;
               trace_debug("matrices_offset %lld, instance_index=%lld, renderable_index=%lld",
                     (isz)linear_matrices_offset, (isz)instance_index, (isz)renderable_index
               );
               linear_matrices_offset += renderable.joint_list.count;
            }
         }
         total_geometry_matrices_count = linear_matrices_offset;

         // TODO: Change to capacity
         if (!is_valid_buffer(manager.scene.instances_buffer)) {
            manager.scene.instances_buffer = create_buffer(instances_buffer_usage, gpu_instances, required_instances_buffer_capacity_in_bytes);
         } else {
            resize_buffer_if_needed(&manager.scene.instances_buffer, required_instances_buffer_capacity_in_bytes);
            update_buffer(&manager.scene.instances_buffer, gpu_instances, 0, required_instances_buffer_capacity_in_bytes);
         }
         trestore(checkpoint);
         bind_buffer_view(&manager.scene.instances_buffer, BUFFER_TYPE_STORAGE, BINDING_INSTANCE_BUFFER, 0, required_instances_buffer_capacity_in_bytes);
      }

      if (total_geometry_matrices_count > 0)
      {  // Animation buffer management
         isz require_size = total_geometry_matrices_count * size_of(float16);
         auto checkpoint = tsave();
         float16* gpu_matrices = talloc(require_size);
         byte* gpu_matrices_ptr = (byte*)gpu_matrices;
         for (u32 renderable_index = 0; renderable_index < manager.scene.renderables.count; renderable_index += 1) {
            auto renderable = manager.scene.renderables.items[renderable_index];
            for (u32 instance_index = 0; instance_index < renderable.instances.count; instance_index += 1) {
               auto instance = renderable.instances.items[instance_index];
               auto matrices = instance.geometry_to_world_matrices;
               isz matrices_size_in_bytes = matrices.count * size_of(matrices.items[0]);
               memcpy(gpu_matrices_ptr, matrices.items, matrices_size_in_bytes);
               gpu_matrices_ptr += matrices_size_in_bytes;
            }
         }


         if (!is_valid_buffer(manager.scene.geometry_to_world_matrices_buffer)) {
            manager.scene.geometry_to_world_matrices_buffer
               = create_buffer(geometry_to_world_matrices_buffer_usage, gpu_matrices, require_size);
         } else {
            resize_buffer_if_needed(&manager.scene.geometry_to_world_matrices_buffer, require_size);
            update_buffer(&manager.scene.geometry_to_world_matrices_buffer, gpu_matrices, 0, require_size);
         }
         trestore(checkpoint);
         bind_buffer_view(&manager.scene.geometry_to_world_matrices_buffer, BUFFER_TYPE_STORAGE, BINDING_ANIMATION_MATRICES, 0, require_size);
      }
   }

   if (manager.draw_commands.dirty) {
      trace_info("[Manager] Draw Commands were dirty.");

      // TODO: Maybe move this. This might trigger a lot.
      // TODO: Maybe We should have 3 cmd buffers and persistent map
      isz instances_count = 0;
      for (u32 renderable_index = 0; renderable_index < manager.scene.renderables.count; renderable_index += 1) {
         auto renderable = manager.scene.renderables.items[renderable_index];
         auto draw_index = renderable.draw_index;
         for (u32 sequential_index = 0; sequential_index < draw_index.count;  sequential_index += 1) {
            auto draw_command = &manager.draw_commands.items[draw_index.base + sequential_index];
            draw_command->instance_count = renderable.instances.count;
            draw_command->instance_offset = instances_count;
         }
         instances_count += renderable.instances.count;
      }

      trace_info(
         "[Manager] Draw Commands: capacity = %lld count = %lld, renderables_count = %lld, instances_count = %lld, size_of_each_command = %lld",
         (isz)manager.draw_commands.capacity, (isz)manager.draw_commands.count, (isz)manager.scene.renderables.count, (isz)instances_count, (isz)size_of(manager.draw_commands.items[0])
      );


      isz required_buffer_capacity_in_bytes = manager.draw_commands.capacity * size_of(manager.draw_commands.items[0]);
      resize_buffer_if_needed(&manager.draw_commands.buffer, required_buffer_capacity_in_bytes);

      { // Deduplicate draw_commands with no instances. Maybe do this? Tbh this will only be useful if we like are deleting a lot of scene_nodes every frame?
      }

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
         for (isz material_texture_index = 0; material_texture_index < MAX_TEXTURE_PER_MATERIAL; material_texture_index += 1) {
            auto texture = &material->textures[material_texture_index];
            if (texture->path) {
               *texture = create_texture_from_filepath(texture->path);
               this_many_textures += 1;
            }
         }

         material->loaded = true;
         materials_handles[material_index] = (Material) {
            .diffuse_handle  = material->diffuse.bindless_handle,
            .specular_handle = material->specular.bindless_handle,
            .emissive_handle = material->emissive.bindless_handle,
            .normal_handle   = material->normal.bindless_handle,
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
      trace_warn("Materials should have been loaded already, but it's not. Loading it for now, but if you keep fucking this up u're done boy.");
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
   if (draw_index.base + draw_index.count > manager.draw_commands.count) {
      trace_error("%s: You are tripping dawg, bogus draw_index", __func__);
      return;
   }

   update_manager_gpu_resources();
   glBindVertexArray(manager.vao);
   glVertexArrayElementBuffer(manager.vao, manager.indices.buffer.handle);

   for (u32 sequential_index = 0; sequential_index < draw_index.count;  sequential_index += 1) {
      auto draw_command = &manager.draw_commands.items[draw_index.base + sequential_index];

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
// Accept diffuse texture for debugging shader
void draw_indirect(Texture diffuse, Shader shader) {
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
      bind_buffer_view(&manager.draw_commands.buffer, BUFFER_TYPE_STORAGE, BINDING_DRAW_COMMAND, 0, draw_commands_size);
   }

   {
      auto vertices_size = manager.vertices.count * (2*size_of(Vector3) + size_of(Vector2));
      bind_buffer_view(&manager.vertices.buffer,  BUFFER_TYPE_STORAGE, BINDING_VERTEX_BUFFER, 0, vertices_size);
   }

   {
      auto tangents_size = manager.tangents.count * (size_of(manager.tangents.items[0]));
      bind_buffer_view(&manager.tangents.buffer,  BUFFER_TYPE_STORAGE, BINDING_TANGENTS_BUFFER, 0, tangents_size);
   }

   {
      auto joints_size = manager.joints.count * (size_of(manager.joints.items[0]));
      bind_buffer_view(&manager.joints.buffer,  BUFFER_TYPE_STORAGE, BINDING_JOINT_BUFFER, 0, joints_size);
   }

   {
      auto material_handles_size = manager.materials.count * size_of(Material);
      bind_buffer_view(&manager.materials.buffer, BUFFER_TYPE_STORAGE, BINDING_MATERIAL, 0, material_handles_size);
   }

#if RENDERER_DEBUG
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
      if (!vaoBound || !eboBound || !dibBound) {
          trace_error("Indirect setup missing: VAO=%d EBO=%d DIB=%d", vaoBound, eboBound, dibBound);
      }

      GLenum err = glGetError();
      if (err != GL_NO_ERROR) {
         trace_error("[OpenGL Error] Before glMultiDrawElementsIndirect (0x%X).", err);
      }
   }
#endif

   // assert(is_valid_texture(diffuse));
   // bind_texture(diffuse, 3);

   glMultiDrawElementsIndirect(
       GL_TRIANGLES, GL_UNSIGNED_INT,
       (const void *)0,                          // No offset into draw command buffer
       manager.draw_commands.count,              // How many commands. In count, not size.
       size_of(manager.draw_commands.items[0])   // Stride, 0 if the data is tightly packed
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
u32 push_material_to_manager(const char* diffuse_path, const char* specular_path, const char* emissive_path, const char* normal_path) {
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
   material->normal.path   = normal_path   ? strdup(normal_path)   : nullptr;
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

   //
   // TODO: This base_thingy is so predominant that we should design push_arrays around that, instead of these overrides.
   //       Also, not every surface in the mesh need to have tangents, it might make sense to only upload the
   //       tangents that are actually used by a surface we're aready controlling base_tangents_offset per draw_command anyways.
   //       Maybe the mesh itself should trim the tangents before getting here, but the problem with that is that tangents are related to the other vertex data (positions, uvs...) through indices buffer.
   //       It would add too much complication to have a separate indices for tangets or something of the kind. Making the manager upload less tangent data per draw_command seems more viable and simple.
   //       We just need to think if indexing the tangent buffer in the shader would bring back the same complication regarding indices that I've mentioned if it were to be done in mesh creation time.
   //
   auto base_vertices_offset = manager.vertices.count;
   auto base_tangents_offset = manager.tangents.count;
   auto base_joints_offset   = manager.joints.count;

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
      u32 vertices_count = (0 == surface_index) ? mesh->vertices.count : 0;
      Draw_Index draw_index = push_arrays_to_manager(
         mesh->vertices.positions,   // All positions
         mesh->vertices.normals,     // All normals
         mesh->vertices.uvs,         // All UVs
         mesh->vertices.tangents,    // Tangents   (can be nullptr)
         mesh->vertices.joints,      // Joint data (can be nullptr)
         vertices_count,             // Total vertex count
         surface_indices,            // Surface-specific indices
         surface_indices_count,      // Surface index count
         final_material_index,       // Material index
         base_vertices_offset,       // Forcing an offset for vertices
         base_tangents_offset,       // Forcing an offset for tangents
         base_joints_offset          // Forcing an offset for joints
      );

      // Detect first time assignment
      if (0 == result_draw_index.count) {
         result_draw_index = draw_index;
      } else {
         if ((result_draw_index.base + result_draw_index.count) != draw_index.base) {
            trace_error("For some reason the draw_indexes from mesh is not sequencial. Find out why.");
            trace_struct(result_draw_index);
            trace_struct(draw_index);
            exit(1);
         }
         result_draw_index.count += draw_index.count;
      }
   }

   return result_draw_index;
}

// Push entire model to buffer manager, handling all meshes and materials
Draw_Index push_model_to_manager(const Model *model, Animation_Index *animation_index) {
   assert(model != nullptr);
   assert(model->meshes.items != nullptr);
   assert(model->meshes.count > 0);

   // IMPORTANT: Lets outside how many anymations it has
   if (model->animations.count > 0 ) {
      animation_index->base  = manager.animations.count;
      animation_index->count = model->animations.count;
      // NOTE: Only one animation for now
      // Animation  deep_copied_animation = animation_deep_copy(&model->animations.items[0]);
      for (isize i = 0; i < model->animations.count; i++) {
         Animation  deep_copied_animation = model->animations.items[i];
         da_append(&manager.animations, deep_copied_animation);
      }
   }

   // First, push all materials from the model to the manager
   u32 material_index_base = manager.materials.count;
   for (isz material_index = 0; material_index < model->materials.count; material_index += 1) {
      auto material = model->materials.items[material_index];
      // (void)(material.normal && (debug_break(), 1));
      push_material_to_manager(material.diffuse, material.specular, material.emissive, material.normal);
   }

   Draw_Index result_draw_index = {0};

   for (isz mesh_index = 0; mesh_index < model->meshes.count; mesh_index += 1) {
      Mesh *mesh = &model->meshes.items[mesh_index];
      Draw_Index mesh_draw_item = push_mesh_to_manager(mesh, material_index_base);

      // Detect first time assignment
      if (0 == result_draw_index.count) {
         result_draw_index = mesh_draw_item;
      } else {
         if ((result_draw_index.base + result_draw_index.count) != mesh_draw_item.base) {
            trace_error("%s For some reason the draw_indexes from mesh is not sequencial. Find out why.", __func__);
            trace_struct(result_draw_index);
            trace_struct(mesh_draw_item);
            exit(1);
         }
         result_draw_index.count += mesh_draw_item.count;
      }
   }

   return result_draw_index;
}


// T Pose
void play_animation_identity(Scene_Node node) {
   // Needs to update geometry_to_world_matrices_buffer after this
   manager.scene.instances_dirty = true;

   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable = &manager.scene.renderables.items[renderable_index];

   if (renderable->animation_index.count < 1) {
      trace_warn("Trying to play animation on a node that doesn't have one. (renderable_index = %lld, instance_index = %lld)", renderable_index, instance_index);
      return;
   }

   auto instance  = &renderable->instances.items[instance_index];
   auto animation = &manager.animations.items[renderable->animation_index.base + instance->animation_number];

   if (instance->geometry_to_world_matrices.count <= 0 && nullptr == instance->geometry_to_world_matrices.items) {
      instance->geometry_to_world_matrices.count = renderable->joint_list.count;
      isz size = size_of(float16) * instance->geometry_to_world_matrices.count;
      instance->geometry_to_world_matrices.items = malloc(size);
   }
   float16 identity = MatrixToFloatV(MatrixIdentity());
   for (u32 index = 0; index < instance->geometry_to_world_matrices.count; index += 1) {
      instance->geometry_to_world_matrices.items[index] = identity;
   }
   // TODO: Mark animation as dirty when we get around to setting up a dirty flag for it
   return;
}


void play_animation(Scene_Node node, uint animation_number) {
   // TODO: Mark animation as dirty when we get around to setting up a dirty flag for it.
   // For now we use instances_dirty to update geometry_to_world_matrices_buffer after this
   manager.scene.instances_dirty = true;

   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable = &manager.scene.renderables.items[renderable_index];

   if (renderable->animation_index.count < 1) {
      trace_warn("Trying to play animation on a node that doesn't have one. (renderable_index = %lld, instance_index = %lld)", renderable_index, instance_index);
      return;
   }

   if (animation_number >= renderable->animation_index.count) {
      trace_warn("Trying to play animation number (%d) that does not correspod to any animation on this node as it only goes to %d", animation_number, renderable->animation_index.count);
      return;
   }

   auto instance  = &renderable->instances.items[instance_index];
   instance->animation_number = animation_number;
   auto animation = &manager.animations.items[renderable->animation_index.base + instance->animation_number];

   f64 *curr_time = &instance->animation_current_time;
   const f64 animation_speed = instance->animation_speed;

   //
   // TODO: We should actually skip frames instead because of unstable delta times.
   //       The problem is that 2 of the same animation that start at different
   //       times start syncing as if they both had the same start.
   //
   // That was a hacky solution before, now we're clamping dt which isn't a clever ideal solution
   // As user's might expected an animation to take exactly a certain amount of time and sunddenly it couldnt
   // finish in time because we advanced the animation by a clamped dt
   //
   #if 0
      static double skip_calls = 0;
      if (skip_calls < 200) {
         skip_calls += 1;
         return;
      }
   #endif

   {  // Timing Operations
      static enum {smooth_delta, clamp_delta, bad_raw_delta, enum_count} strategy = smooth_delta;
      if (is_button_pressed(BUTTON_R)) {
         strategy += 1;
         strategy = (strategy % enum_count);
         trace_info("Changed the animation stategy to %s",
            strategy  == smooth_delta  ? "smooth_delta"
            :strategy == clamp_delta   ? "clamp_delta"
            :strategy == bad_raw_delta ? "bad_raw_delta"
            :"unknown"
         );
      }


      // NOTE: In case the animation delta is too high that is passes the time_end, should we loop around the overshot amount?
      //       That would make sure that is case of a high delta we don't suddenly sync all the animation to time_begin; effectively making all
      //       animations start suspiciously synchronized all of sudden (after a high delta caused by lag).
      //       It's likely we wanna either loop around or clamp, looping around might look strange in the normal case (low delta), because the animation has the seamless loop in mind
      //       that means that overshooting a bit would likely break that seemless looping feel, unless change animation to lerp from end to begin frames.
      //       Clamping might have the same syncronizing problem if the animation is short enough.
      static double dt = 0.016;
      if (strategy == clamp_delta) {
         dt = time_delta();
         // clamp to 100ms max
         dt = clamp(dt, 0, 0.1);
      } else if (strategy == smooth_delta) {
         // Smoothing delta time
         double raw_dt = time_delta();
         dt = 0.9 * dt + 0.1 * raw_dt;
      } else if (strategy == bad_raw_delta) {
         dt = time_delta();
      }

      if (is_debugging()) {
         // dt = 0.1; // Delta timing while debugging is always huge because of pauses.
      }


      *curr_time += dt * animation_speed;
      const bool loop_animation = true; // TODO: Get this from instance

      if (loop_animation) {
         if (*curr_time >= animation->time_end) {
            *curr_time = animation->time_begin;
         }

         if (*curr_time < animation->time_begin) {
            *curr_time = animation->time_end;
         }
      }

      // We make sure before getting the matrices we're within the expected times by keyframes
      *curr_time = clamp(*curr_time, animation->time_begin, animation->time_end);
   }
   auto list = joint_matrices_from_animation(&renderable->joint_list, animation, *curr_time);

   if (is_button_pressed(BUTTON_F2)) {
      auto old_count = list.count;
      list = joint_matrices_using_scene(&renderable->joint_list, animation, *curr_time);
      assert_msg(old_count == list.count, "count does not match for %s", animation->scene->metadata.original_file_path);
      trace_debug("Using roubadinha animation");
   }


   isz list_data_size = size_of(list.matrices[0]) * list.count;
   if (instance->geometry_to_world_matrices.count <= 0 && nullptr == instance->geometry_to_world_matrices.items) {
      assert_msg(renderable->joint_list.count == list.count, "Joint list and the Joint matrices should have the same count, because it's a bijection to the bones count");
      instance->geometry_to_world_matrices.items = malloc(list_data_size);
      instance->geometry_to_world_matrices.count = list.count;
   }

   memcpy(instance->geometry_to_world_matrices.items, list.items, list_data_size);

   return;
}


void overload play_animation(Scene_Node node) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable = manager.scene.renderables.items[renderable_index];
   auto instance   = renderable.instances.items[instance_index];
   play_animation(node, instance.animation_number);
}




// NOTE: 2025-09-04 Every instance is created here renderable is created elsewhere
Scene_Node internal create_scene_node_from_renderable(isz renderable_index, const Transform transform) {
   auto renderable = &manager.scene.renderables.items[renderable_index];

   isz instance_index = renderable->instances.count;
   da_append(&renderable->instances,
      { .transform = transform, .animation_speed = 1. }
   );

   isz scene_node_index = manager.scene.nodes.count;
   da_append(&manager.scene.nodes,
      {.node_parent = -1, .instance_index = instance_index, .renderable_index = renderable_index}
   );

   Scene_Node scene_node = {.index = scene_node_index};

   // T Pose by default
   if (has_animation(scene_node)) {
      play_animation_identity(scene_node);
   }

   // Because the instance transform needs to be updated
   manager.scene.instances_dirty = true;

   // Because instance_count has to be updated for each draw command.
   manager.draw_commands.dirty = true;

   return scene_node;
}

// Create a new draw commands without creating new vertex, indices buffers, it should only affect the draw commands
Scene_Node create_scene_node_new_cmd(Scene_Node node, const Transform transform) {
   isz src_renderable_index  = manager.scene.nodes.items[node.index].renderable_index;
   Draw_Index src_draw_index = manager.scene.renderables.items[src_renderable_index].draw_index;
   Joint_List src_joint_list = manager.scene.renderables.items[src_renderable_index].joint_list;
   Animation_Index src_animation_index   = manager.scene.renderables.items[src_renderable_index].animation_index;


   Draw_Index draw_index = {0};
   for (u32 sequential_index = 0; sequential_index < src_draw_index.count;  sequential_index += 1) {
      Draw_Command draw_command = manager.draw_commands.items[src_draw_index.base + sequential_index];
      //
      // These shall be set when updating gpu buffers
      // draw_command.instance_count = 1;
      // draw_command.instance_offset = instances_count;
      //
      Draw_Index curr_draw_index = push_draw_command_to_manager(draw_command);
      if (0 == draw_index.count) {
         draw_index.base = curr_draw_index.base;
      }
      draw_index.count += 1;
      assert_msg(draw_index.base + draw_index.count == curr_draw_index.base +  curr_draw_index.count, "The ends should meet because they should be sequential");
   }


   isz renderable_index = manager.scene.renderables.count;
   da_append(&manager.scene.renderables, {.draw_index = draw_index, .animation_index = src_animation_index, .joint_list = src_joint_list});
   auto new_node = create_scene_node_from_renderable(renderable_index, transform);
   return new_node;
}

// TODO: Change C treesitter query to make overload be a keyword right before the the type instead of after
Scene_Node create_scene_node(const Model *model, const Transform transform) {
   Animation_Index animation_index = {.base = 0, .count = 0};
   Draw_Index draw_index = push_model_to_manager(model, &animation_index);
   isz renderable_index = manager.scene.renderables.count;
   da_append(&manager.scene.renderables, {.draw_index = draw_index, .animation_index = animation_index, .joint_list = model->joints});
   return create_scene_node_from_renderable(renderable_index, transform);
}

Scene_Node overload create_scene_node(Scene_Node node, const Transform transform) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   return create_scene_node_from_renderable(renderable_index, transform);
}


//
// TODO: Updating the whole instances transform every time seems to be a bit slow
//       Maybe just update in place with persistent mapped instead of going through the cpu buffer staging system
//
void update_transform(Scene_Node node, Transform transform) {
   manager.scene.instances_dirty = true;

   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable = &manager.scene.renderables.items[renderable_index];
   auto instance  = &renderable->instances.items[instance_index];
   instance->transform = transform;
   return;
}

void overload update_transform(Scene_Node node, Quaternion rotation) {
   manager.scene.instances_dirty = true;

   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable = &manager.scene.renderables.items[renderable_index];
   auto instance  = &renderable->instances.items[instance_index];
   instance->transform.rotation = rotation;
   return;
}


Transform get_transform(Scene_Node node) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;
   auto renderable      = &manager.scene.renderables.items[renderable_index];
   auto instance        = &renderable->instances.items[instance_index];
   return instance->transform;
}

void set_animation_time(Scene_Node node, f64 time) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;

   auto renderable = &manager.scene.renderables.items[renderable_index];
   auto instance   = &renderable->instances.items[instance_index];
   auto animation  = &manager.animations.items[renderable->animation_index.base + instance->animation_number];
   instance->animation_current_time = clamp(time, animation->time_begin, animation->time_end);
}

// From 0 to 1.0
void set_animation_time_percentage(Scene_Node node, f64 percentage) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;

   auto renderable = &manager.scene.renderables.items[renderable_index];
   auto instance   = &renderable->instances.items[instance_index];
   auto animation  = &manager.animations.items[renderable->animation_index.base + instance->animation_number];
   if (percentage > 1.0 || percentage < 0 ) {
      trace_warn("Trying to play animation at %.2f%% percentage on node=%d. Range should be [0 - 1] inclusive.", percentage*100, node.index);
   }
   f64 time = percentage * (animation->time_end - animation->time_begin);
   instance->animation_current_time = clamp(time, animation->time_begin, animation->time_end);
}

void set_animation_speed(Scene_Node node, f64 speed) {
   isz renderable_index = manager.scene.nodes.items[node.index].renderable_index;
   isz instance_index   = manager.scene.nodes.items[node.index].instance_index;

   auto renderable = &manager.scene.renderables.items[renderable_index];
   auto instance   = &renderable->instances.items[instance_index];
   auto animation  = &manager.animations.items[renderable->animation_index.base + instance->animation_number];
   instance->animation_speed = clamp(speed, -F64_MAX, F64_MAX);
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
   manager.materials.items    = nullptr;
   manager.materials.count    = 0;
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
   manager.vertices.positions  = calloc(initial_vertex_capacity,       size_of(manager.vertices.positions[0]));
   manager.vertices.normals    = calloc(initial_vertex_capacity,       size_of(manager.vertices.normals[0]));
   manager.vertices.uvs        = calloc(initial_vertex_capacity,       size_of(manager.vertices.uvs[0]));
   manager.indices.items       = calloc(initial_index_capacity,        size_of(manager.indices.items[0]));
   manager.joints.items        = calloc(initial_vertex_capacity,       size_of(manager.joints.items[0]));
   manager.draw_commands.items = calloc(initial_draw_command_capacity, size_of(manager.draw_commands.items[0]));
   manager.materials.items     = calloc(initial_material_capacity,     size_of(manager.materials.items[0]));


   auto buffer_flag = BUFFER_USAGE_SUBDATA;
   // auto buffer_flag = BUFFER_USAGE_ORPHANABLE;

   // Create GPU buffers
   isz vertex_item_size     = size_of(manager.vertices.positions[0]) + size_of(manager.vertices.normals[0]) + size_of(manager.vertices.uvs[0]);
   isz vertex_buffer_size   = initial_vertex_capacity * vertex_item_size;
   manager.vertices.buffer  = create_buffer(buffer_flag, nullptr, vertex_buffer_size);

   isz tangents_buffer_size   = initial_vertex_capacity * size_of(manager.tangents.items[0]);
   manager.tangents.buffer    = create_buffer(buffer_flag, nullptr, tangents_buffer_size);

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


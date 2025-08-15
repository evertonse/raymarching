#include "raymath.h"
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#undef Command
#include "tinyobj_loader_c.h"

#undef swap
#undef local
#include "ufbx.h"
#include "ufbx.c"
#define zero_of(x) ((typeof(x)) {0})

typedef struct Joint Joint;
typedef struct Joint {
   Transform transform; // Always valid
   struct {             // Matrices needs to be calculated first
      const Matrix geometry_to_node; // Binding Inverse whatever, don't ever change (todo: Move this to someplace else, this will duplicates a lot in animation)
      Matrix node_to_world;
      bool calculated;
   } matrices;
   isz parent; // index into Joint_List
} Joint;


typedef struct {
   Joint   *joints;
   ZString *names;
   u32 count;

   double time;
   Transform hierarchy_transform; // From root_bone_to_root_node transform, root_node is the scene root node and root_bone is the bone that has no other bone as parent, but that doesn't mean it doesnt have any parent NODE, it's just garanteed to not have a parent node that happens to be a bone.
} Joint_List;

typedef struct {
   // Keys for this joint
   struct {
      struct {
        union {
           Vector3    vec3;
           Quaternion quat;
           Vector4    vec4; // Generic way of dealing with both vec3 and quat
        };
        double time;
      }  *items;
      u32 count;
   } translation_keyframes, scale_keyframes, rotation_keyframes;

} Joint_Animation;

typedef struct {
   ZString name;
   struct {
      Joint_Animation *items;
      u32 count; // Must be the same amount as joints in Joint_List and in turn we have the same amount of scene->bones.count
   } joint_animations;

   double time_begin;
   double time_end;
   double time_current;
} Animation;

typedef struct {
    struct {
        Mesh* items;
        isz count;
    } meshes;

    struct {
        struct {
            const char* diffuse;
            const char* specular;
            const char* emissive;
        };
    } materials;

    Joint_List joints;

    struct {
        Animation* items;
        isz count;
    } animations;

} Model;


// Options: https://ufbx.github.io/reference#ufbx_load_opts
static ufbx_load_opts ufbx_default_opts = {
  .ignore_embedded = false,
  // .target_axes = ufbx_axes_right_handed_y_up,
  .generate_missing_normals = true,
  .strict = true,
  .target_unit_meters = 0.1f,
  // UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE
  // UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY
  .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY,
  .space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY, // This one is important to help on getting the animation matrices

  #if 1
  .obj_search_mtl_by_filename = true,
  .load_external_files = true, // IMPORTANT: Auto load mtl and other texture files (unsafe if user defined data)0
  #else
  // (.obj) Path to the .mtl file.
  // .obj_mtl_path = {
  //     .data = "res/models/backpack/backpack.mtl",
  //     .length = strlen("res/models/backpack/backpack.mtl")
  // },
  #endif

  // (.obj) Don't split geometry into meshes by object.
  .obj_merge_objects =  true,
  // (.obj) Don't split geometry into meshes by groups.
  .obj_merge_groups = true,
  // (.obj) Force splitting groups even on object boundaries.
  .obj_split_groups = false,
};

Transform transform_from_ufbx_node(ufbx_node *node) {
   Transform transform = {0};
   auto q = node->local_transform.rotation;
   auto t = node->local_transform.translation;
   auto s = node->local_transform.scale;
   transform.rotation.x = q.x;
   transform.rotation.y = q.y;
   transform.rotation.z = q.z;
   transform.rotation.w = q.w;
   transform.translation.x = t.x;
   transform.translation.y = t.y;
   transform.translation.z = t.z;
   transform.scale.x = s.x;
   transform.scale.y = s.y;
   transform.scale.z = s.z;
   return transform;
}

static const char* filepath_from_ufbx_material_map(ZString scene_path, const ufbx_material_map map) {
   if (!map.texture_enabled) {
      return nullptr;
   }

   ZString texture_path = map.texture->filename.data;
   if (!file_exists(texture_path)) {
      ZString texture_base_name = path_base_name(texture_path);
      ZString mode_directory = path_dir_of(scene_path);
      texture_path = path_create(mode_directory, texture_base_name);
      if (!file_exists(texture_path)) {
         return nullptr;
      }
   }
   assert_msg(file_exists(texture_path), "We should previously return null if the file doesnt exist, period.");
   trace_okay("`%s` Texture Path exists for scene `%s`", texture_path, scene_path);
   return strdup(texture_path); // @leak
}


#define UFBX_MAX_WARNING_COUNT 10

void ufbx_log_warnings(const ufbx_scene *scene) {
    if (!scene) return;

    int warning_count[UFBX_WARNING_TYPE_COUNT] = {0};
    int ignored_warning_count = 0;

    for (size_t i = 0; i < scene->metadata.warnings.count; i++) {
        ufbx_warning warning = scene->metadata.warnings.data[i];

        if (warning_count[warning.type]++ < UFBX_MAX_WARNING_COUNT) {
            if (warning.count > 1) {
                trace_warn("FBX: ufbx warning: %s (x%d)", warning.description, (int)warning.count);
            } else {
                const char *element_name = NULL;
                if (warning.element_id != UFBX_NO_INDEX &&
                    warning.element_id < scene->elements.count)
                {
                    ufbx_element *element = scene->elements.data[warning.element_id];
                    element_name = element->name.data;
                }

                if (element_name && element_name[0] != '\0') {
                    trace_warn("FBX: ufbx warning in '%s': %s", element_name, warning.description);
                } else {
                    trace_warn("FBX: ufbx warning: %s", warning.description);
                }
            }
        } else {
            ignored_warning_count++;
        }
    }

    if (ignored_warning_count > 0) {
        trace_warn("FBX: ignored %d further ufbx warnings", ignored_warning_count);
    }
}

Matrix raylib_matrix_from_ufbx_matrix(const ufbx_matrix ufbxmat) {
   Matrix rlmat = {0};

   // first column (x axis)
   rlmat.m0 = (float)ufbxmat.m00;
   rlmat.m1 = (float)ufbxmat.m10;
   rlmat.m2 = (float)ufbxmat.m20;
   rlmat.m3 = 0.0f;

   // second column (y axis)
   rlmat.m4 = (float)ufbxmat.m01;
   rlmat.m5 = (float)ufbxmat.m11;
   rlmat.m6 = (float)ufbxmat.m21;
   rlmat.m7 = 0.0f;

   // third column (z axis)
   rlmat.m8 = (float)ufbxmat.m02;
   rlmat.m9 = (float)ufbxmat.m12;
   rlmat.m10 = (float)ufbxmat.m22;
   rlmat.m11 = 0.0f;

   // fourth column (translation)
   rlmat.m12 = (float)ufbxmat.m03;
   rlmat.m13 = (float)ufbxmat.m13;
   rlmat.m14 = (float)ufbxmat.m23;
   rlmat.m15 = 1.0f;

   return rlmat;
}

// TODO: HashMap
usz joint_index_from_ufbx_bone_node(const ufbx_scene *scene, const ufbx_node *bone_node) {
   bool found_bone_idx = false;
   usz bone_idx = 0;
   for (; bone_idx < scene->bones.count; bone_idx++) {
      auto scene_bone_node = scene->bones.data[bone_idx]->instances.data[0];
      assert_msg(1 == scene->bones.data[bone_idx]->instances.count, "We assume each bone has exactly 1 instance that correspondes to its node");
      if (scene_bone_node == bone_node) {
         found_bone_idx = true;
         break;
      }
   }
   assert_msg(found_bone_idx, "Should have found because why does a bone from a cluster is not foundable from the scene bones? ");
   return bone_idx;
}

Joint_List create_joint_list_from_ufbx_scene(const ufbx_scene *scene) {
   assert(scene);

   Joint_List list = {0};
   if (!list.joints) {
      usz joints_size = size_of(list.joints[0]) * scene->bones.count;
      usz names_size = size_of(list.names[0]) * scene->bones.count;
      char* data = malloc(joints_size + names_size);
      list.joints = (typeof(list.joints))(data + 0);
      list.names  = (typeof(list.names ))(data + joints_size);
   } else {
      assert(list.count == scene->bones.count);
   }

   list.count = scene->bones.count;
   for (usz bones_idx = 0; bones_idx < scene->bones.count; bones_idx++) {
      auto bone = scene->bones.data[bones_idx];

      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];
      list.names[bones_idx] = strdup(bone_node->name.data); // @LEAK
      Joint *joint = &list.joints[bones_idx];
      bool has_parent = nullptr != bone_node->parent;
      if (!has_parent || nullptr == bone_node->parent->bone) {
         // Maybe we need to check this to make sure it's a bone
         // Considering the armature might have a final transformation it's important to apply it even it it doen'nt have a bone attach, so idk what to do in this situation?
         joint->parent = -1;
      } else {
         usz parent_idx = joint_index_from_ufbx_bone_node(scene, bone_node->parent);
         joint->parent = (isz)parent_idx;
      }

      // HACK: This joint_list is not correct when we have more nodes than bones
      // So we're combining transform from the parent and its parent and so one, we consider this the hierarchy transform, maybe we should only go as far as the mesh node goes?.
      // Maybe gltf is simpler with these transformations by only allowing exactly only bone hierarchy limiting each file to a single model? Unlikely.
      if (has_parent && (nullptr == bone_node->parent->bone)) {
         Transform child_transform  = transform_from_ufbx_node(bone_node);

         ufbx_node *tmp_node = bone_node->parent;
         Transform hierarchy_transform = transform_from_ufbx_node(tmp_node);
         while (tmp_node->parent) {
            Transform parent_transform = transform_from_ufbx_node(tmp_node->parent);
            hierarchy_transform = TransformCombine(parent_transform, hierarchy_transform);
            tmp_node = tmp_node->parent;
         }
         list.hierarchy_transform = hierarchy_transform;
         // todo make it so we don't need to tarnish this joint transform
         joint->transform = TransformCombine(hierarchy_transform, child_transform);
         assert_msg(bone_node->parent->parent->is_root && nullptr == bone_node->parent->parent->parent, "Expected to have no more parents but (%p) %s", bone_node->parent->parent, bone_node->parent->parent->name.data);
      } else {
         joint->transform = transform_from_ufbx_node(bone_node);
      }

      // Find the geomtry_to_bone (inverse bind matrix)
      ufbx_matrix geometry_to_node = ufbx_identity_matrix;
      for (usz cluster_idx = 0; cluster_idx < scene->skin_clusters.count; cluster_idx++) {
         auto cluster = scene->skin_clusters.data[cluster_idx];
         if (bone_node == cluster->bone_node) {
            geometry_to_node = cluster->geometry_to_bone;
            break;
         }
      }
      // The only place we set geometry_to_node should be here, thats why we break const promise;
      *(Matrix *)&joint->matrices.geometry_to_node = raylib_matrix_from_ufbx_matrix(geometry_to_node);

      joint->matrices.calculated = false;
   }
   return list;
}

void destroy_joint_list(Joint_List *list) {
   free(list->joints);
   *list = zero_of(*list);
}


// TODO: Single malloc, instead of malloc for each keyframes. Count every size for every data, fire 1 malloc then set all ptrs.
Animation create_animation_from_ufbx(ufbx_scene *scene, ufbx_anim *anim) {
   Animation result = {0};

   result.joint_animations.count = scene->bones.count;
   result.joint_animations.items = calloc(scene->bones.count, size_of(Joint_Animation));

   // Baked animation data is ufbx transforming the fbx data into linearly interpolatable keyframes. Easy enough.
   ufbx_baked_anim *baked = ufbx_bake_anim(scene, anim, NULL, NULL);


   result.time_begin = baked->playback_time_begin;
   result.time_end   = baked->playback_time_end;
   result.time_begin = anim->time_begin;
   result.time_end   = anim->time_end;

   result.time_current = result.time_begin;

   for (u32 bone_i = 0; bone_i < scene->bones.count; bone_i++) {
      ufbx_bone *bone = scene->bones.data[bone_i];
      ufbx_node *node = bone->instances.data[0]; // assuming 1 instance per bone

      Joint_Animation *ja = &result.joint_animations.items[bone_i];

      ufbx_baked_node *bnode = ufbx_find_baked_node(baked, node);
      ufbx_baked_node *bnode2 = ufbx_find_baked_node_by_typed_id(baked, node->typed_id);
      if (!bnode)
         continue;
      assert_msg(bnode == bnode2, "sanity check");

      // Translation
      ja->translation_keyframes.count = bnode->translation_keys.count;
      ja->translation_keyframes.items = malloc(size_of(*ja->translation_keyframes.items) * ja->translation_keyframes.count);
      for (u32 k = 0; k < bnode->translation_keys.count; k++) {
         ja->translation_keyframes.items[k].vec3 =
             (Vector3){(float)bnode->translation_keys.data[k].value.x, (float)bnode->translation_keys.data[k].value.y, (float)bnode->translation_keys.data[k].value.z};
         ja->translation_keyframes.items[k].time = bnode->translation_keys.data[k].time;
      }

      // Rotation
      ja->rotation_keyframes.count = bnode->rotation_keys.count;
      ja->rotation_keyframes.items = malloc(size_of(*ja->rotation_keyframes.items) * ja->rotation_keyframes.count);
      for (u32 k = 0; k < bnode->rotation_keys.count; k++) {
         ja->rotation_keyframes.items[k].quat =
             (Quaternion){(float)bnode->rotation_keys.data[k].value.x, (float)bnode->rotation_keys.data[k].value.y, (float)bnode->rotation_keys.data[k].value.z, (float)bnode->rotation_keys.data[k].value.w};
         ja->rotation_keyframes.items[k].time = bnode->rotation_keys.data[k].time;
      }

      // Scale
      ja->scale_keyframes.count = bnode->scale_keys.count;
      ja->scale_keyframes.items = malloc(size_of(*ja->scale_keyframes.items) * ja->scale_keyframes.count);
      for (u32 k = 0; k < bnode->scale_keys.count; k++) {
         ja->scale_keyframes.items[k].vec3
             = (Vector3){(float)bnode->scale_keys.data[k].value.x, (float)bnode->scale_keys.data[k].value.y, (float)bnode->scale_keys.data[k].value.z};
         ja->scale_keyframes.items[k].time
             = bnode->scale_keys.data[k].time;
      }
   }

   ufbx_free_baked_anim(baked);
   return result;
}

static constexpr int MAX_WEIGHTS = 4;
Model create_model(const char *filepath) {
   Model model = {0};
   ZString scene_filepath = filepath;
   ufbx_error error; // Optional, pass NULL if you don't care about errors
   ufbx_scene *scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   // scene = ufbx_evaluate_scene(scene, scene->anim, 0, nullptr, &error);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return model;
   }
   ufbx_log_warnings(scene);

   if (scene->texture_files.count > 0) {
      trace_info("%d texture files for this scene: ", scene->texture_files.count);
      for (size_t i = 0; i < scene->texture_files.count; i++) {
         trace_info("    texture file (%d) filename = \"%s\";", i, scene->texture_files.data[i].filename.data);
         trace_info("    texture file (%d) absolute_filename = \"%s\";", i, scene->texture_files.data[i].absolute_filename.data);
         trace_info("    texture file (%d) relative_filename = \"%s\";", i, scene->texture_files.data[i].relative_filename.data);
         trace_info("    texture file (%d) content.size = \"%lld\";", i, scene->texture_files.data[i].content.size);
      }
   }


   {   // Setup animations
       model.animations.count = 1;
       model.animations.items = malloc(model.animations.count * size_of(model.animations.items[0]));
       for (size_t i = 0; i < scene->anim_stacks.count; i++) {
          ufbx_anim_stack *stack = scene->anim_stacks.data[i];
          printf("i stack %s:\n", stack->name.data);
          Animation animation = create_animation_from_ufbx(scene, stack->anim);
          assert(animation.joint_animations.items);
          model.animations.items[i] = animation;
          break; // TODO: Get mo' animations
       }
   }

   {  //  Setup le joints/bones
      model.joints = create_joint_list_from_ufbx_scene(scene);
   }


   assert(scene->meshes.count > 0);
   // assert_msg(scene->meshes.count == scene->nodes.count - 1, "We got %lld meshes and %lld nodes", scene->meshes.count, scene->nodes.count);


   model.meshes.count = 0;
   model.meshes.items = malloc(scene->meshes.count * size_of(model.meshes.items[0]));
   for (usz node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
      ufbx_node *node = scene->nodes.data[node_idx];
      if (nullptr == node->mesh) {
         continue;
      }
      if (node->is_root) {
         trace_info("Node %s (%lld) is root. %lld faces\n", node->name.data, (isz)node_idx, node->mesh->faces.count);
      }

      {  // Setup Mesh data
         auto mesh = node->mesh;
         auto materials = node->materials;
         isz vertices_count = 0;
         usz triangles_count = mesh->num_triangles;
         isz tri_indices_count = mesh->max_face_triangles * 3;
         usz checkpoint = tsave();
         u32 *tri_indices = talloc(tri_indices_count * size_of(u32));

         usz indices_count = triangles_count * 3;

         // Malloc once and set the pointers
         Mesh model_mesh = {0};
         {
            usz positions_size = triangles_count * 3 * size_of(model_mesh.positions[0]);
            usz normals_size   = triangles_count * 3 * size_of(model_mesh.normals[0]);
            usz uvs_size       = triangles_count * 3 * size_of(model_mesh.uvs[0]);
            isz indices_size   = indices_count   * size_of(u32);
            // Final indices will occupy less memory that we're setting here
            char* data = malloc(positions_size + normals_size + uvs_size + indices_size);

            model_mesh.positions = (Vector3*)(data + 0);
            model_mesh.normals   = (Vector3*)(data + positions_size);
            model_mesh.uvs       = (Vector2*)(data + positions_size + normals_size);
            model_mesh.indices   = (u32*    )(data + positions_size + normals_size + uvs_size);

            model_mesh.positions_count = triangles_count * 3;
            model_mesh.normals_count   = triangles_count * 3;
            model_mesh.uvs_count       = triangles_count * 3;

            if (scene->bones.count > 0) {
               assert(size_of(model_mesh.joint_data[0]) == (4+4) * size_of(float));
               // TODO: Condense into 1 malloc call
               char* data = malloc(model_mesh.positions_count*size_of(model_mesh.joint_data[0]));
               model_mesh.joint_data = (typeof(model_mesh.joint_data))data;

            }
         }

         assert(1 == mesh->skin_deformers.count);
         auto skin = mesh->skin_deformers.data[0];
         for (usz face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
            ufbx_face face = mesh->faces.data[face_idx];
            u32 tri_count = ufbx_triangulate_face(tri_indices, tri_indices_count, mesh, face);
            // Iterate over each triangle corner contiguously.
            for (isz tri_idx = 0; tri_idx < tri_count * 3; tri_idx++) {
               u32 index = tri_indices[tri_idx];
               ufbx_vec3 ufbx_position = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
               ufbx_vec3 ufbx_normal   = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
               ufbx_vec2 ufbx_uv       = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);

               // ufbx_position  = ufbx_transform_position(&node->geometry_to_world, ufbx_position);

               Vector3 position = {(f32)ufbx_position.x, (f32)ufbx_position.y, (f32)ufbx_position.z};
               Vector3 normal   = {(f32)ufbx_normal.x,   (f32)ufbx_normal.y,   (f32)ufbx_normal.z};
               Vector2 uv       = {(f32)ufbx_uv.x,       (f32)ufbx_uv.y};
               uint32_t vertex = mesh->vertex_indices.data[index];
               ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];
               size_t num_weights = skin_vertex.num_weights;
               if (num_weights > MAX_WEIGHTS) {
                   num_weights = MAX_WEIGHTS;
               }

               float total_weight = 0.0f;
               Vector4    bone_weight    = {-1., -1., -1., -1.};
               Vector4Int bone_idxs = {-1,  -1,  -1,  -1 };
               for (size_t i = 0; i < num_weights; i++) {
                  ufbx_skin_weight skin_weight = skin->weights.data[skin_vertex.weight_begin + i];

                  // Nonchalantly finding the index by ptr comparison (uh!) in O(bones_count*num_wights*everysingle_vertice).
                  // Pray we ain't got thousands of bones, this is a job for either a hash or see it ufbx has some way to get the index from cluster to bones?
                  ufbx_skin_cluster *cluster = skin->clusters.data[skin_weight.cluster_index];
                  usz bone_idx = joint_index_from_ufbx_bone_node(scene, cluster->bone_node);
                  bone_idxs.items[i]   = (int  )bone_idx;
                  bone_weight.items[i] = (float)skin_weight.weight;
                  total_weight        += (float)skin_weight.weight;
               }

               // FBX does not guarantee that skin weights are normalized, and we may even
               // be dropping some, so we must renormalize them.
               for (size_t i = 0; i < num_weights; i++) {
                   bone_weight.items[i] /= total_weight;
               }

               model_mesh.positions [vertices_count] = position;
               model_mesh.normals   [vertices_count] = normal;
               model_mesh.uvs       [vertices_count] = uv;
               model_mesh.indices   [vertices_count] = vertices_count;
               model_mesh.joint_data[vertices_count].joint_idxs    = bone_idxs;
               model_mesh.joint_data[vertices_count].joint_weights = bone_weight;
               vertices_count += 1;
            }
         }

         trestore(checkpoint);

         assert((isz)vertices_count == (isz)triangles_count * 3
               && model_mesh.positions_count == vertices_count
               && model_mesh.normals_count   == vertices_count
               && model_mesh.uvs_count       == vertices_count
         );


         const bool reduce_indices = true; // DONE: Adjust for joint data
         if (reduce_indices) {
            // Generate the index buffer.

            ufbx_vertex_stream streams[] = {
                {model_mesh.positions,  vertices_count, size_of(model_mesh.positions[0]) },
                {model_mesh.normals,    vertices_count, size_of(model_mesh.normals[0])   },
                {model_mesh.uvs,        vertices_count, size_of(model_mesh.uvs[0])       },
                {model_mesh.joint_data, vertices_count, size_of(model_mesh.joint_data[0])},

            };
            isz streams_count = model_mesh.joint_data ? count_of(streams) : count_of(streams) - 1;


            // This call will deduplicate vertices, modifying the arrays passed in `streams[]`,
            // indices are written in `indices[]` and the number of unique vertices is returned.
            isz vertices_count_new = (isz)ufbx_generate_indices(streams, streams_count, model_mesh.indices, indices_count, NULL, NULL);
            model_mesh.positions_count = vertices_count_new;
            model_mesh.normals_count   = vertices_count_new;
            model_mesh.uvs_count       = vertices_count_new;

            // model_mesh.indices_count   = vertices_count_new;
            model_mesh.indices_count = indices_count;


            if (vertices_count_new < vertices_count) {
               trace_okay("ufbx_generate_indices optimized from %lld to %lld", vertices_count, vertices_count_new);
            } else if (vertices_count_new == vertices_count) {
               trace_info("ufbx_generate_indices did jack shit from %lld to %lld", vertices_count, vertices_count_new);
            } else {
               trace_error("ufbx_generate_indices did worsened (? ?) from %lld to %lld", vertices_count, vertices_count_new);
            }

         } else {
            model_mesh.indices_count = indices_count;
         }
         model.meshes.items[model.meshes.count++] = model_mesh;
      }

      {  // Setup Textures
         if (scene->textures.count > 0) {
            trace_info("%d textures for this scene: ", scene->textures.count);
            for (size_t i = 0; i < scene->textures.count; i++) {
               auto texture = *scene->textures.data[i];
               ZString base_name = path_base_name(texture.relative_filename.data);
               trace_info("    texture (%d): base_name %s", i, base_name);
               trace_info("    texture (%d): %s", i, texture.relative_filename.data);
               trace_info("    texture (%d): file_textures.count %ld ", i, texture.file_textures.count);
            }
         }
         assert(node->materials.count == 1);
         for (usz material_idx = 0; material_idx < node->materials.count; material_idx += 1) {
            ufbx_material material = *node->materials.data[material_idx];
            trace_okay("material_idx = %lld name = %s", material_idx, material.name);

            // Diffuse
            model.materials.diffuse = filepath_from_ufbx_material_map(scene_filepath, material.pbr.base_color);
            model.materials.diffuse = model.materials.specular ?: filepath_from_ufbx_material_map(scene_filepath, material.fbx.diffuse_color);

            // Specular
            model.materials.specular = filepath_from_ufbx_material_map(scene_filepath, material.fbx.specular_color);
            model.materials.specular = model.materials.specular ?: filepath_from_ufbx_material_map(scene_filepath, material.fbx.reflection_factor);
            trace_info("specular_color texture path %s", model.materials.specular);

            model.materials.emissive = filepath_from_ufbx_material_map(scene_filepath, material.fbx.emission_color);
            trace_info("emission_color texture path %s", model.materials.specular);
         }
      }
   }

   assert(scene->meshes.count == (usz)model.meshes.count);
   ufbx_free_scene(scene);
   return model;
}

typedef struct {
   Vector3 *positions;
   u32 count;
} Vector3_List;

ufbx_matrix get_node_to_model_space_matrix(ufbx_node *node) {
   auto node_to_parent = ufbx_transform_to_matrix(&node->local_transform);
   if (nullptr == node->parent) {
      return node_to_parent;
   }
   auto parent_to_world = get_node_to_model_space_matrix(node->parent);
   ufbx_matrix result = ufbx_matrix_mul(&parent_to_world, &node_to_parent);
   return result;
}


Matrix get_node_to_model_space_matrix2(ufbx_node *node) {
   Transform transform = transform_from_ufbx_node(node);
   Matrix node_to_parent = MatrixCompose(transform);

   if (nullptr == node->parent) {
      return node_to_parent;
   }
   Matrix parent_to_world = get_node_to_model_space_matrix2(node->parent);
   Matrix result = mul(parent_to_world, node_to_parent);
   return result;
}

Vector3_List bone_positions(const char *filepath, double time) {
   static Vector3_List list = {0};
   ZString scene_filepath = "res/models/boy/boy_animation.fbx";

   ufbx_error error; // Optional, pass NULL if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   }

   const ufbx_evaluate_opts eval_opts =  {0};
   ufbx_scene *scene = ufbx_evaluate_scene(orig_scene, orig_scene->anim, time, &eval_opts, &error);

   ufbx_pose* bind_pose = nullptr;
   for (usz pose_idx = 0; pose_idx < scene->poses.count; pose_idx++) {
      ufbx_pose* pose = scene->poses.data[pose_idx];
      if (pose->is_bind_pose) {
         trace_info("Found bind pose %p", pose);
         bind_pose = pose;
         break;
      }
   }
   assert(bind_pose);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }
   ufbx_log_warnings(scene);

   if (!list.positions) {
      list.positions = malloc(size_of(list.positions[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;
   for (usz bones_idx = 0; bones_idx < scene->bones.count; bones_idx++) {
      auto bone = scene->bones.data[bones_idx];
      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];

      trace_info("%d bone (%s):", bones_idx, bone_node->name.data);
      ufbx_vec3   pos                   = {0};
      ufbx_matrix ground_truth          = bone_node->geometry_to_world; // ufbx_matrix mat = bone_node->node_to_world;
      ufbx_matrix computed_ground_truth = get_node_to_model_space_matrix(bone_node);
      ufbx_vec3   ufbx_result           = ufbx_transform_position(&computed_ground_truth, pos);
      // Vector3     result                = {ufbx_result.x, ufbx_result.y, ufbx_result.z};
      Vector3     result                = {0};

      Matrix  raymat_from_ufbx = raylib_matrix_from_ufbx_matrix(computed_ground_truth);
      Matrix  raymat = get_node_to_model_space_matrix2(bone_node);
      Vector3 rayresult = mul(raymat, (Vector3){0});
      result = rayresult;
      trace_debug("res(%d) = {%f  %f  %f}", bones_idx, result.x, result.y, result.z);
      list.positions[list.count++] = result;
   }
   ufbx_free_scene(scene);
   return list;
}

// NOTE: We could pass an offset + size into .time instead of this typeof
static inline int find_keyframe_interval(const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys, u32 count, double time) {
   if (count < 2) {
      return -1;
   }
   int low = 0, high = count - 1;
   while (low <= high) {
      int mid = (low + high) / 2;
      if (time < keys[mid].time) {
         if (mid == 0)
            break;
         high = mid - 1;
      } else if (time > keys[mid + 1].time) {
         low = mid + 1;
      } else {
         return mid;
      }
   }
   return -1;
}

// TODO: Sketchy function, idk, i don't like it, is it fine? Can we do something about it?
static inline void interpolate_from_keyframes(const typeof(((Joint_Animation *)0)->translation_keyframes) keyframes, double time, Vector3 *out_vector3, Quaternion* out_quat) {
   assert_msg(out_quat && !out_vector3 || !out_quat && out_vector3, "Dont love this, there's a wrong way of calling this function");
   if (keyframes.count == 0) {
      return;
   }
   if (keyframes.count == 1) {
      if (out_vector3) {
         *out_vector3 = keyframes.items[0].vec3;
      } else {
         *out_quat    = keyframes.items[0].quat;
      }
      return;
   }

   assert(keyframes.count > 1);
   int idx = find_keyframe_interval(keyframes.items, keyframes.count, time);
   if (idx >= 0) {
      auto a = keyframes.items[idx];
      auto b = keyframes.items[idx + 1];
      float alpha = (float)((time - a.time) / (b.time - a.time));
      if (out_vector3) {
         *out_vector3 = Vector3Lerp(a.vec3, b.vec3, alpha);
      } else {
         *out_quat    = QuaternionSlerp(a.quat, b.quat, alpha);
      }
   }
}

void update_joints_transforms(Joint_List *joint_list, const Animation *animation, double time) {
   for (u32 i = 0; i < joint_list->count; i++) {
      const Joint_Animation *ja = &animation->joint_animations.items[i];

      Vector3    T = {0.0, 0.0, 0.0};
      Quaternion R = {0.0, 0.0, 0.0, 1.0};
      Vector3    S = {1.0, 1.0, 1.0};

      // Translation
      interpolate_from_keyframes(ja->translation_keyframes, time, &T,       nullptr);
      interpolate_from_keyframes(ja->rotation_keyframes,    time, nullptr,  &R     );
      interpolate_from_keyframes(ja->scale_keyframes,       time, &S,       nullptr);

      joint_list->joints[i].transform.translation = T;
      joint_list->joints[i].transform.rotation    = R;
      joint_list->joints[i].transform.scale       = S;
      joint_list->joints[i].matrices.calculated   = false;
   }
}



Matrix joint_calculate_node_to_world_matrix(Joint_List* list, Joint *node) {
   if (node->matrices.calculated) {
      return node->matrices.node_to_world;
   }
   Matrix node_to_parent = MatrixCompose(node->transform);
   if (node->parent < 0) {
      node->matrices.node_to_world = node_to_parent;
      node->matrices.calculated = true;
      return node->matrices.node_to_world;
   }

   Joint *parent = &list->joints[node->parent];
   Matrix parent_to_world = joint_calculate_node_to_world_matrix(list, parent);
   Matrix result = mul(parent_to_world, node_to_parent);
   node->matrices.node_to_world = result;
   node->matrices.calculated = true;
   return node->matrices.node_to_world;
}


typedef struct {
   float16 *matrices;
   u32 count;
} Geometry_To_World_List;

Geometry_To_World_List joint_matrices_original(double time) {
   static Geometry_To_World_List list = {0};
   ZString scene_filepath = "res/models/boy/boy_animation_textured.fbx";
   // Options: https://ufbx.github.io/reference#ufbx_load_opts

   ufbx_error error; // Optional, pass NULL if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   }

   const ufbx_evaluate_opts eval_opts =  {0};
   ufbx_scene *scene = ufbx_evaluate_scene(orig_scene, orig_scene->anim, time, &eval_opts, &error);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }
   ufbx_log_warnings(scene);

   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;
   auto joint_list = create_joint_list_from_ufbx_scene(scene);
   for (usz idx = 0; idx < joint_list.count; idx++) {
      auto joint = &joint_list.joints[idx];
      Matrix node_to_world     = joint_calculate_node_to_world_matrix(&joint_list, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", idx, joint_list.names[idx]);
      list.matrices[list.count++] = MatrixToFloatV(geometry_to_world);
   }
   destroy_joint_list(&joint_list);
   ufbx_free_scene(scene);
   return list;
}

Geometry_To_World_List joint_matrices_using_animation(double time) {
   static Geometry_To_World_List list = {0};
   ZString scene_filepath = "res/models/boy/boy_animation.fbx";
   // Options: https://ufbx.github.io/reference#ufbx_load_opts

   ufbx_error error; // Optional, pass NULL if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene || UFBX_ERROR_NONE != error.type) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }

   ufbx_scene *scene = orig_scene;
   ufbx_log_warnings(scene);

   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;
   // If I construct the list each frame, we gots yeppes, if not we get still animation
   static Joint_List joint_list = {0};
   if (0 == joint_list.count) {
      joint_list = create_joint_list_from_ufbx_scene(scene);
   }

   // Iterate over every animation stack (aka. clip/take) in the file
   static Animation animation = {0};
   if (!animation.joint_animations.items) {
       for (size_t i = 0; i < scene->anim_stacks.count; i++) {
          ufbx_anim_stack *stack = scene->anim_stacks.data[i];
          printf("i stack %s:\n", stack->name.data);
          animation = create_animation_from_ufbx(scene, stack->anim);
          break; // TODO: Get mo' animations
       }
   }
   assert(animation.joint_animations.items);

   update_joints_transforms(&joint_list, &animation, time);
   for (usz idx = 0; idx < joint_list.count; idx++) {
      auto joint = &joint_list.joints[idx];
      Matrix node_to_world     = joint_calculate_node_to_world_matrix(&joint_list, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", idx, joint_list.names[idx]);
      Matrix scene_hierarchy_matrix = MatrixCompose(joint_list.hierarchy_transform);
      geometry_to_world = mul(scene_hierarchy_matrix, geometry_to_world);
      float16 values = MatrixToFloatV(geometry_to_world);
      for (int i = 0; i < 16; i++) {
         if (i == 3 || i == 7 || i == 11 || i == 15) {
            continue;
         }
         // values.v[i] *= 100.;
      }
      list.matrices[list.count++] = values;
   }
   return list;
}

// You are supposed to call this function once to update the GPU buffers and then forget it. The next time you call this function, the old matrices are invalidated.
// The workflow is: auto matrices = joint_matrices_from_animation(); upload/update GPU buffers from matrices, and that's it. There is no need to free or allocate memory in 
// the middle of the frame just for this, so instead, we are going to reuse the same buffer over and over.
Geometry_To_World_List joint_matrices_from_animation(Joint_List *joints, const Animation *const animation,  double time) {
   static Geometry_To_World_List list = {0};
   static isz capacity = 0;
   if (!list.matrices || joints->count > capacity) {
      capacity = joints->count*2;
      list.matrices = realloc(list.matrices, size_of(list.matrices[0]) * capacity);
   }
   list.count = joints->count;

   assert(capacity >= joints->count);
   assert(list.count == joints->count);
   assert(animation->joint_animations.items);

   update_joints_transforms(joints, animation, time);
   for (usz idx = 0; idx < joints->count; idx++) {
      auto joint = &joints->joints[idx];
      Matrix node_to_world = joint_calculate_node_to_world_matrix(joints, joint);
      if (false) {
         Matrix scene_hierarchy_matrix = MatrixCompose(joints->hierarchy_transform);
         Matrix geometry_to_world = mul(scene_hierarchy_matrix, mul(node_to_world, joint->matrices.geometry_to_node));
      }
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", idx, joints->names[idx]);
      Matrix scene_hierarchy_matrix = MatrixCompose(joints->hierarchy_transform);
      geometry_to_world = mul(scene_hierarchy_matrix, geometry_to_world);
      list.matrices[idx] = MatrixToFloatV(geometry_to_world);
   }
   return list;
}

Geometry_To_World_List joint_matrices_roubadinha(double time) {
   static Geometry_To_World_List list = {0};
   ZString scene_filepath = "res/models/boy/boy_animation.fbx";
   // Options: https://ufbx.github.io/reference#ufbx_load_opts

   ufbx_error error; // Optional, pass NULL if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   }

   const ufbx_evaluate_opts eval_opts =  {0};
   ufbx_scene *scene = ufbx_evaluate_scene(orig_scene, orig_scene->anim, time, &eval_opts, &error);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }
   ufbx_log_warnings(scene);

   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;

   ufbx_pose *bind_pose = nullptr;
   auto bind_poses_count = 0;
   for (usz poses_idx = 0; poses_idx < scene->poses.count; poses_idx++) {
     auto pose = scene->poses.data[poses_idx];
     if (pose->is_bind_pose) {
       bind_pose = pose;
       bind_poses_count++;
     }
   }

   assert(bind_pose && bind_poses_count == 1);

   for (usz bones_idx = 0; bones_idx < scene->bones.count; bones_idx++) {
      auto bone = scene->bones.data[bones_idx];
      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];

      ufbx_matrix geometry_to_node = ufbx_identity_matrix;
      for (usz bone_poses_idx = 0; bone_poses_idx < bind_pose->bone_poses.count; bone_poses_idx++) {
         auto bone_pose = bind_pose->bone_poses.data[bone_poses_idx];
         if (bone_node == bone_pose.bone_node) {
            // geometry_to_node = bone_pose.bone_to_world;
            ufbx_matrix world_to_bind = ufbx_matrix_invert(&bone_pose.bone_to_world);
            geometry_to_node = world_to_bind;
            // geometry_to_node = ufbx_matrx_mul(&world_to_bind, &bone_node->node_to_world);
         }
      }

      for (usz cluster_idx = 0; cluster_idx < scene->skin_clusters.count; cluster_idx++) {
         auto cluster = scene->skin_clusters.data[cluster_idx];
         if (bone_node == cluster->bone_node) {
            geometry_to_node = cluster->geometry_to_bone;
            break;
         }
      }
      assert(scene->skin_clusters.count < scene->bones.count);

      auto geometry_to_world = ufbx_matrix_mul(&bone_node->node_to_world, &geometry_to_node);
      list.matrices[list.count++] = MatrixToFloatV(raylib_matrix_from_ufbx_matrix(geometry_to_world));
   }
   ufbx_free_scene(scene);
   return list;
}


Geometry_To_World_List joint_matrices_from_model(Model *model, double time) {
   assert(model->animations.count == 1);
   return joint_matrices_from_animation(&model->joints, &model->animations.items[0], time);
}

// HACK: This button hadling should be gonee
inline bool is_button_pressed(Button input);
Geometry_To_World_List joint_matrices(Model *model, double time) {
   set_trace_level(LOG_INFO);
   auto m4 = joint_matrices_from_model(model, time);
   auto m1 = joint_matrices_roubadinha(time);
   auto m2 = joint_matrices_original(time);
   auto m3 = joint_matrices_using_animation(time);
   trace_info("Time before is %f", time);
   Geometry_To_World_List result = {0};
   static int val = 4;
   trace_info("Time after is %f", time);

   if (is_button_pressed(BUTTON_1)) {
      val = 1;
   } else if (is_button_pressed(BUTTON_2)) {
      val = 2;
   } else if (is_button_pressed(BUTTON_3)) {
      val = 3;
   } else if (is_button_pressed(BUTTON_4)) {
      val = 4;
   }

   if (val == 1) {
      result = m1;
   } else if (val == 2) {
      result = m2;
   } else if (val == 3) {
      result = m3;
   } else if (val == 4) {
      result = m4;
   }
   return result;
}

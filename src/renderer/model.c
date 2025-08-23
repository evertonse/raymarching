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
        } *items;
        isz count;
    } materials;

    Joint_List joints;

    struct {
        Animation* items;
        isz count;
    } animations;

} Model;


// Options: https://ufbx.github.io/reference#ufbx_load_opts
static const ufbx_load_opts ufbx_default_opts = {
   .normalize_normals  = true,
   .normalize_tangents = true,
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
   .obj_merge_groups  = true,
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
      ZString base_directory = path_dir_of(scene_path);
      texture_path = path_create(base_directory, texture_base_name);
      if (!file_exists(texture_path)) {
         base_directory = path_create(path_dir_of(base_directory), "textures");
         texture_path   = path_create(base_directory, texture_base_name);
         if (!file_exists(texture_path)) {
            return nullptr;
         }
      }
   }
   assert_msg(file_exists(texture_path), "We should previously return null if the file doesnt exist, period.");
   trace_okay("`%s` Texture Path exists for scene `%s`", texture_path, scene_path);
   return strdup(texture_path); // @Leak
}


#define UFBX_MAX_WARNING_COUNT 10

void trace_ufbx_warnings(const ufbx_scene *scene) {
   if (!scene) {
      return;
   }

   int warning_count[UFBX_WARNING_TYPE_COUNT] = {0};
   int ignored_warning_count = 0;

   for (size_t i = 0; i < scene->metadata.warnings.count; i++) {
      ufbx_warning warning = scene->metadata.warnings.data[i];
      auto description = warning.description.data;

      if (warning_count[warning.type]++ < UFBX_MAX_WARNING_COUNT) {
         if (warning.count > 1) {
            trace_warn("FBX: ufbx warning: %s (x%d)", description, (int)warning.count);
         } else {
            const char *element_name = nullptr;
            if (warning.element_id != UFBX_NO_INDEX && warning.element_id < scene->elements.count) {
               ufbx_element *element = scene->elements.data[warning.element_id];
               element_name = element->name.data;
            }

            if (element_name && element_name[0] != '\0') {
               trace_warn("FBX: ufbx warning in '%s': %s", element_name, description);
            } else {
               trace_warn("FBX: ufbx warning: %s", description);
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
   bool found_bone_index = false;
   usz bone_index = 0;
   for (; bone_index < scene->bones.count; bone_index++) {
      auto scene_bone_node = scene->bones.data[bone_index]->instances.data[0];
      assert_msg(1 == scene->bones.data[bone_index]->instances.count, "We assume each bone has exactly 1 instance that correspondes to its node");
      if (scene_bone_node == bone_node) {
         found_bone_index = true;
         break;
      }
   }
   assert_msg(found_bone_index, "Should have found because why does a bone from a cluster is not foundable from the scene bones? ");
   return bone_index;
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
   for (usz bones_index = 0; bones_index < scene->bones.count; bones_index++) {
      auto bone = scene->bones.data[bones_index];

      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];
      list.names[bones_index] = strdup(bone_node->name.data); // @LEAK
      Joint *joint = &list.joints[bones_index];
      bool has_parent = nullptr != bone_node->parent;
      if (!has_parent || nullptr == bone_node->parent->bone) {
         // Maybe we need to check this to make sure it's a bone
         // Considering the armature might have a final transformation it's important to apply it even it it doen'nt have a bone attach, so idk what to do in this situation?
         joint->parent = -1;
      } else {
         usz parent_index = joint_index_from_ufbx_bone_node(scene, bone_node->parent);
         joint->parent = (isz)parent_index;
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
         // assert_msg(bone_node->parent->parent->is_root && nullptr == bone_node->parent->parent->parent, "Expected to have no more parents but (%p) %s", bone_node->parent->parent, bone_node->parent->parent->name.data);
      } else {
         joint->transform = transform_from_ufbx_node(bone_node);
      }

      // Find the geomtry_to_bone (inverse bind matrix)
      ufbx_matrix geometry_to_node = ufbx_identity_matrix;
      for (usz cluster_index = 0; cluster_index < scene->skin_clusters.count; cluster_index++) {
         auto cluster = scene->skin_clusters.data[cluster_index];
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
   ufbx_baked_anim *baked = ufbx_bake_anim(scene, anim, nullptr, nullptr);


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

void trace_ufbx_scene_statsold(ufbx_scene *scene) {
   auto checkpoint = tsave();
   ZString info = "";
   info = tprintf("%s %d textures for this scene: ", info, scene->textures.count);
   for (size_t i = 0; i < scene->textures.count; i++) {
      auto texture = *scene->textures.data[i];
      ZString base_name = path_base_name(texture.relative_filename.data);
      info = tprintf("%s    texture (%d): base_name %s", info, i, base_name);
      info = tprintf("%s                : %s", info, i, texture.relative_filename.data);
      info = tprintf("%s                : file_textures.count %ld ", info, i, texture.file_textures.count);
   }

   trestore(checkpoint);
   trace_info(info);

   info = tprintf("%s %d materials for this scene: ", info, scene->materials.count);
   for (size_t i = 0; i < scene->materials.count; i++) {
      auto material = *scene->materials.data[i];
      info = tprintf("%s    material '%s' (%d): has %ldd textures", info, i, material.name, material.textures.count);
      for (size_t j = 0; j < material.textures.count; j++) {
         auto texture = *(material.textures.data[j].texture);
         ZString base_name = path_base_name(texture.relative_filename.data);
         info = tprintf("%s        texture (%d): base_name %s", info, i, base_name);
         info = tprintf("%s                    : %s", info, i, texture.relative_filename.data);
         info = tprintf("%s                    : file_textures.count %ld ", info, i, texture.file_textures.count);
         info = tprintf("%s                    : content %p with size %ld ", info, i, texture.content.data, texture.content.size);
      }
   }

   trace_info(info);

   trestore(checkpoint);
}

void trace_ufbx_scene_stats(ufbx_scene *scene) {
   auto checkpoint = tsave();
   ZString info = "";
   info = tprintf("%s %d textures for this scene: ", info, scene->textures.count);
   for (size_t i = 0; i < scene->textures.count; i++) {
      auto texture = *scene->textures.data[i];
      auto base_name = path_base_name(texture.relative_filename.data);
      info = tprintf("%s    texture (%zu): base_name %s\n", info, i, base_name);
      info = tprintf("%s                : %s\n", info, texture.relative_filename.data);
      info = tprintf("%s                : file_textures.count %ld\n", info, texture.file_textures.count);
   }

   trestore(checkpoint);
   trace_info(info);

   info = tprintf("%s %d materials for this scene: ", info, scene->materials.count);
   for (size_t i = 0; i < scene->materials.count; i++) {
      auto material = *scene->materials.data[i];
      info = tprintf("%s    material '%s' (%zu): has %ld textures\n", info, material.name.data, i, material.textures.count);
      for (size_t j = 0; j < material.textures.count; j++) {
         auto texture = *(material.textures.data[j].texture);
         auto base_name = path_base_name(texture.relative_filename.data);
         info = tprintf("%s        texture (%zu): base_name %s\n", info, j, base_name);
         info = tprintf("%s                    : %s\n", info, texture.relative_filename.data);
         info = tprintf("%s                    : file_textures.count %ld\n", info, texture.file_textures.count);
         info = tprintf("%s                    : content %p with size %ld\n", info, texture.content.data, texture.content.size);
      }
   }

   trace_info(info);

   trestore(checkpoint);
}

static isz material_index_from_ufbx_scene(ufbx_material* material, ufbx_scene *scene) {
   isz index = -1;
   // We're gonna fully loop everytime to make sure no repeated material
   for (usz material_index = 0; material_index < scene->materials.count; material_index += 1) {
      ufbx_material *scene_material = scene->materials.data[material_index];
      if (material == scene_material) {
         assert(-1 == index);
         index = material_index;
      }
   }
   // We expect to found it 100% of the time, so assert we actually found it
   assert(-1 != index);
   return index;
}

// TODO: setup -> create for consistency
static void setup_materials_from_ufbx_scene(Model *model, ufbx_scene *scene, const char* scene_filepath) {
   // Setup Textures
   if (!model->materials.items) {
      model->materials.count = scene->materials.count;
      model->materials.items = malloc(size_of(model->materials.items[0])*model->materials.count);
   }
   for (usz material_index = 0; material_index < scene->materials.count; material_index += 1) {
      ufbx_material fbx_material = *scene->materials.data[material_index];
      auto material = &model->materials.items[material_index];
      // Diffuse
      material->diffuse = filepath_from_ufbx_material_map(scene_filepath, fbx_material.pbr.base_color);
      material->diffuse = material->diffuse ?: filepath_from_ufbx_material_map(scene_filepath, fbx_material.fbx.diffuse_color);

      // Specular
      material->specular = filepath_from_ufbx_material_map(scene_filepath, fbx_material.fbx.specular_color);
      material->specular = material->specular ?: filepath_from_ufbx_material_map(scene_filepath, fbx_material.fbx.reflection_factor);

      // Emisse ignored
      material->emissive = filepath_from_ufbx_material_map(scene_filepath, fbx_material.fbx.emission_color);
   }
}

static Mesh create_mesh_from_ufbx_node(ufbx_node *node, ufbx_scene *scene) {
   static constexpr int MAX_WEIGHTS = 4;

   auto fbx_mesh = node->mesh;
   assert(fbx_mesh);

   usz checkpoint = tsave();

   usz total_triangles = fbx_mesh->num_triangles;
   usz total_indices   = total_triangles * 3;

   isz  tri_indices_count = fbx_mesh->max_face_triangles * 3;
   u32 *tri_indices       = talloc(tri_indices_count * size_of(u32));

   bool has_bones = scene->bones.count > 0;

   usz surfaces_count = fbx_mesh->material_parts.count;

   // Allocate mesh data
   Mesh mesh = {0};
   {
      usz total_size = 0;

      usz positions_size   = total_indices             * size_of(mesh.vertices.positions[0]);
      usz normals_size     = total_indices             * size_of(mesh.vertices.normals[0]);
      usz uvs_size         = total_indices             * size_of(mesh.vertices.uvs[0]);
      usz joints_size  = has_bones ?
                             total_indices             * size_of(mesh.vertices.joints[0]) : 0;
      usz indices_size     = total_indices             * size_of(u32);
      usz surfaces_size    = surfaces_count            * size_of(mesh.surfaces.items[0]);

      total_size = positions_size + normals_size + uvs_size + indices_size + surfaces_size + joints_size;

      // Allocate a single block of memory
      char *data = malloc(total_size);

      // Assign pointers
      mesh.vertices.positions  = (Vector3*)(data);
      mesh.vertices.normals    = (Vector3*)(data + positions_size);
      mesh.vertices.uvs        = (Vector2*)(data + positions_size + normals_size);
      mesh.vertices.joints =
         has_bones ?                  (void   *)(data + positions_size + normals_size + uvs_size) : nullptr;
      mesh.indices.items                  = (u32    *)(data + positions_size + normals_size + uvs_size + joints_size);
      mesh.surfaces.items           = (void   *)(data + positions_size + normals_size + uvs_size + joints_size + indices_size);
   }


   auto skin = fbx_mesh->skin_deformers.count > 0 ? fbx_mesh->skin_deformers.data[0] : nullptr;

   isz total_vertex_count = 0;
   isz total_index_count = 0;

   // Process each material part (surface) - always at least 1
   for (usz part_index = 0; part_index < fbx_mesh->material_parts.count; part_index++) {
      auto material_part = fbx_mesh->material_parts.data[part_index];

      auto surface = &mesh.surfaces.items[part_index];

      // Set surface start
      surface->indices_offset = total_index_count;
      surface->material_index = part_index < node->materials.count ?
            material_index_from_ufbx_scene(node->materials.data[part_index], scene)
          :-1;

      isz part_vertex_start = total_vertex_count;

      // Process faces in this material part
      for (usz face_idx = 0; face_idx < material_part.face_indices.count; face_idx++) {
         u32 face_index = material_part.face_indices.data[face_idx];
         ufbx_face face = fbx_mesh->faces.data[face_index];

         u32 tri_count = ufbx_triangulate_face(tri_indices, tri_indices_count, fbx_mesh, face);

         // Process triangles in this face
         for (isz tri_index = 0; tri_index < tri_count * 3; tri_index++) {
            u32 index = tri_indices[tri_index];

            // Get vertex data
            ufbx_vec3 ufbx_position = ufbx_get_vertex_vec3(&fbx_mesh->vertex_position, index);
            ufbx_vec3 ufbx_normal = ufbx_get_vertex_vec3(&fbx_mesh->vertex_normal, index);
            ufbx_vec2 ufbx_uv = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, index);

            Vector3 position = {(f32)ufbx_position.x, (f32)ufbx_position.y, (f32)ufbx_position.z};
            Vector3 normal = {(f32)ufbx_normal.x, (f32)ufbx_normal.y, (f32)ufbx_normal.z};
            Vector2 uv = {(f32)ufbx_uv.x, (f32)ufbx_uv.y};

            // Handle skinning data
            if (skin && mesh.vertices.joints) {
               uint32_t vertex = fbx_mesh->vertex_indices.data[index];
               ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];
               size_t num_weights = skin_vertex.num_weights;
               if (num_weights > MAX_WEIGHTS)
                  num_weights = MAX_WEIGHTS;

               float total_weight      = 0.0f;
               Vector4 bone_weight     = {0, 0, 0, 0};
               Vector4Int bone_indices = {-1, -1, -1, -1};

               for (size_t i = 0; i < num_weights; i++) {
                  ufbx_skin_weight skin_weight = skin->weights.data[skin_vertex.weight_begin + i];
                  ufbx_skin_cluster *cluster = skin->clusters.data[skin_weight.cluster_index];
                  usz bone_index = joint_index_from_ufbx_bone_node(scene, cluster->bone_node);

                  bone_indices.items[i] = (int)bone_index;
                  bone_weight.items[i] = (float)skin_weight.weight;
                  total_weight += (float)skin_weight.weight;
               }

               // Normalize weights
               if (total_weight > 0.0f) {
                  for (size_t i = 0; i < num_weights; i++) {
                     bone_weight.items[i] /= total_weight;
                  }
               }

               mesh.vertices.joints[total_vertex_count].indices = bone_indices;
               mesh.vertices.joints[total_vertex_count].weights = bone_weight;
            }

            // Store vertex data
            mesh.vertices.positions[total_vertex_count] = position;
            mesh.vertices.normals[total_vertex_count] = normal;
            mesh.vertices.uvs[total_vertex_count] = uv;
            mesh.indices.items[total_index_count] = total_vertex_count;

            total_vertex_count++;
            total_index_count++;
         }
      }

      // Set surface index count
      surface->indices_count = total_index_count - surface->indices_offset;
   }

   // Set final counts
   mesh.vertices.count = total_vertex_count;
   mesh.indices.count       = total_index_count;
   mesh.surfaces.count      = surfaces_count;

   trestore(checkpoint);

   // Optional: Optimize with vertex deduplication
   const bool reduce_indices = true;
   if (reduce_indices) {
      ufbx_vertex_stream streams[] = {
          {mesh.vertices.positions,  total_vertex_count, size_of(mesh.vertices.positions[0])},
          {mesh.vertices.normals,    total_vertex_count, size_of(mesh.vertices.normals[0])},
          {mesh.vertices.uvs,        total_vertex_count, size_of(mesh.vertices.uvs[0])},
          {mesh.vertices.joints,     total_vertex_count, size_of(mesh.vertices.joints[0])},
      };
      isz streams_count = mesh.vertices.joints ? count_of(streams) : count_of(streams) - 1;

      isz vertices_count_new = (isz)ufbx_generate_indices(streams, streams_count, mesh.indices.items, total_index_count, nullptr, nullptr);

      mesh.vertices.count = vertices_count_new;

      // Surface indices are still valid - they reference the same index buffer positions
      // ufbx_generate_indices only remaps vertex data and updates the index values
      // but preserves the index buffer structure and ordering. Or so I believe.

      trace_okay("ufbx_generate_indices optimized from %lld to %lld vertices", total_vertex_count, vertices_count_new);
   }

   return mesh;
}


Model create_model(const char *filepath) {
   Model model = {0};
   ZString scene_filepath = filepath;
   ufbx_error error; // Optional, pass nullptr if you don't care about errors
   ufbx_scene *scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   // scene = ufbx_evaluate_scene(scene, scene->anim, 0, nullptr, &error);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return model;
   }
   trace_ufbx_warnings(scene);

   trace_ufbx_scene_stats(scene);

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

   {
      setup_materials_from_ufbx_scene(&model, scene, scene_filepath);
   }

   {  //  Setup le joints/bones
      model.joints = create_joint_list_from_ufbx_scene(scene);
   }


   assert(scene->meshes.count > 0);
   // assert_msg(scene->meshes.count == scene->nodes.count - 1, "We got %lld meshes and %lld nodes", scene->meshes.count, scene->nodes.count);


   model.meshes.count = 0;
   model.meshes.items = malloc(scene->meshes.count * size_of(model.meshes.items[0]));
   for (usz node_index = 0; node_index < scene->nodes.count; node_index++) {
      ufbx_node *node = scene->nodes.data[node_index];
      if (nullptr == node->mesh) {
         continue;
      }
      auto mesh = create_mesh_from_ufbx_node(node, scene);
      model.meshes.items[model.meshes.count++] = mesh;
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

   ufbx_error error; // Optional, pass nullptr if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   }

   const ufbx_evaluate_opts eval_opts =  {0};
   ufbx_scene *scene = ufbx_evaluate_scene(orig_scene, orig_scene->anim, time, &eval_opts, &error);

   ufbx_pose* bind_pose = nullptr;
   for (usz pose_index = 0; pose_index < scene->poses.count; pose_index++) {
      ufbx_pose* pose = scene->poses.data[pose_index];
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
   trace_ufbx_warnings(scene);

   if (!list.positions) {
      list.positions = malloc(size_of(list.positions[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;
   for (usz bones_index = 0; bones_index < scene->bones.count; bones_index++) {
      auto bone = scene->bones.data[bones_index];
      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];

      trace_info("%d bone (%s):", bones_index, bone_node->name.data);
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
      trace_debug("res(%d) = {%f  %f  %f}", bones_index, result.x, result.y, result.z);
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
   int index = find_keyframe_interval(keyframes.items, keyframes.count, time);
   if (index >= 0) {
      auto a = keyframes.items[index];
      auto b = keyframes.items[index + 1];
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

   ufbx_error error; // Optional, pass nullptr if you don't care about errors
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
   trace_ufbx_warnings(scene);

   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;
   auto joint_list = create_joint_list_from_ufbx_scene(scene);
   for (usz index = 0; index < joint_list.count; index++) {
      auto joint = &joint_list.joints[index];
      Matrix node_to_world     = joint_calculate_node_to_world_matrix(&joint_list, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", index, joint_list.names[index]);
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

   ufbx_error error; // Optional, pass nullptr if you don't care about errors
   static ufbx_scene *orig_scene = nullptr;
   if (!orig_scene || UFBX_ERROR_NONE != error.type) {
      orig_scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }

   ufbx_scene *scene = orig_scene;
   trace_ufbx_warnings(scene);

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
   for (usz index = 0; index < joint_list.count; index++) {
      auto joint = &joint_list.joints[index];
      Matrix node_to_world     = joint_calculate_node_to_world_matrix(&joint_list, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", index, joint_list.names[index]);
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
   for (usz index = 0; index < joints->count; index++) {
      auto joint = &joints->joints[index];
      Matrix node_to_world = joint_calculate_node_to_world_matrix(joints, joint);
      if (false) {
         Matrix scene_hierarchy_matrix = MatrixCompose(joints->hierarchy_transform);
         Matrix geometry_to_world = mul(scene_hierarchy_matrix, mul(node_to_world, joint->matrices.geometry_to_node));
      }
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", index, joints->names[index]);
      Matrix scene_hierarchy_matrix = MatrixCompose(joints->hierarchy_transform);
      geometry_to_world = mul(scene_hierarchy_matrix, geometry_to_world);
      list.matrices[index] = MatrixToFloatV(geometry_to_world);
   }
   return list;
}

Geometry_To_World_List joint_matrices_roubadinha(double time) {
   static Geometry_To_World_List list = {0};
   ZString scene_filepath = "res/models/boy/boy_animation.fbx";
   // Options: https://ufbx.github.io/reference#ufbx_load_opts

   ufbx_error error; // Optional, pass nullptr if you don't care about errors
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
   trace_ufbx_warnings(scene);

   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   } else {
      assert(list.count == scene->bones.count);
   }
   list.count = 0;

   ufbx_pose *bind_pose = nullptr;
   auto bind_poses_count = 0;
   for (usz poses_index = 0; poses_index < scene->poses.count; poses_index++) {
     auto pose = scene->poses.data[poses_index];
     if (pose->is_bind_pose) {
       bind_pose = pose;
       bind_poses_count++;
     }
   }

   assert(bind_pose && bind_poses_count == 1);

   for (usz bones_index = 0; bones_index < scene->bones.count; bones_index++) {
      auto bone = scene->bones.data[bones_index];
      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];

      ufbx_matrix geometry_to_node = ufbx_identity_matrix;
      for (usz bone_poses_index = 0; bone_poses_index < bind_pose->bone_poses.count; bone_poses_index++) {
         auto bone_pose = bind_pose->bone_poses.data[bone_poses_index];
         if (bone_node == bone_pose.bone_node) {
            // geometry_to_node = bone_pose.bone_to_world;
            ufbx_matrix world_to_bind = ufbx_matrix_invert(&bone_pose.bone_to_world);
            geometry_to_node = world_to_bind;
            // geometry_to_node = ufbx_matrx_mul(&world_to_bind, &bone_node->node_to_world);
         }
      }

      for (usz cluster_index = 0; cluster_index < scene->skin_clusters.count; cluster_index++) {
         auto cluster = scene->skin_clusters.data[cluster_index];
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

   auto m1 = m4;
   auto m2 = m4;
   auto m3 = m4;

   // auto m1 = joint_matrices_roubadinha(time);
   // auto m2 = joint_matrices_original(time);
   // auto m3 = joint_matrices_using_animation(time);
   Geometry_To_World_List result = {0};
   static int val = 4;

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

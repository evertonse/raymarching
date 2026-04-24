#include "raymath.h"
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#undef Command
#include "tinyobj_loader_c.h"

#include "mikktspace.c"
#include "./mikk.c"

#undef swap
#undef local
#include "ufbx.h"
#include "ufbx.c"

typedef struct {
   struct {
      Mesh *items;
      isz count;
   } meshes;

   struct {
      struct {
         const char *diffuse;
         const char *specular;
         const char *emissive;
         const char *normal;
      } *items;
      isz count;
   } materials;

   Joint_List joints;

   struct {
      Animation *items;
      isz count;
   } animations;

} Model;

void trace_model(const Model *model) {
  if (!model) {
    trace_info("[Model] null");
    return;
  }

  trace_info("[Model] ==========================================");

  // == Meshes =========================================
  trace_info("  meshes: %lld", (isz)model->meshes.count);

  usz total_triangles = 0;

  for (isz mi = 0; mi < model->meshes.count; mi += 1) {
    auto mesh = &model->meshes.items[mi];

    usz mesh_triangles = mesh->indices.count / 3;
    total_triangles += mesh_triangles;

    trace_info("  mesh[%lld]", mi);
    trace_info("    vertices : %lld", (isz)mesh->vertices.count);
    trace_info("    indices  : %lld", (isz)mesh->indices.count);
    trace_info("    triangles: %lld", (isz)mesh_triangles);
    trace_info("    surfaces : %lld", (isz)mesh->surfaces.count);
    trace_info("    skinned  : %s", mesh->vertices.joints ? "yes" : "no");

    for (isz si = 0; si < (isz)mesh->surfaces.count; si += 1) {
      auto s = &mesh->surfaces.items[si];

      usz surface_triangles = s->indices_count / 3;

      trace_info("    surface[%lld]", si);
      trace_info("      material_index : %lld", (isz)s->material_index);
      trace_info("      indices_offset : %lld", (isz)s->indices_offset);
      trace_info("      indices_count  : %lld", (isz)s->indices_count);
      trace_info("      triangles      : %lld", (isz)surface_triangles);
    }
  }

  trace_info("  total triangles: %lld", (isz)total_triangles);

  // == Materials ======================================
  trace_info("  materials: %lld", (isz)model->materials.count);

  for (isz mi = 0; mi < model->materials.count; mi += 1) {
    auto mat = &model->materials.items[mi];

    trace_info("  material[%lld]", mi);
    trace_info("    diffuse  : %s", mat->diffuse ? mat->diffuse : "(none)");
    trace_info("    specular : %s", mat->specular ? mat->specular : "(none)");
    trace_info("    emissive : %s", mat->emissive ? mat->emissive : "(none)");
    trace_info("    normal   : %s", mat->normal ? mat->normal : "(none)");
  }

  // == Joints =========================================
  trace_info("  joints: %lld", (isz)model->joints.count);

  for (isz ji = 0; ji < (isz)model->joints.count; ji += 1) {
    auto joint = &model->joints.joints[ji];
    const char *name = model->joints.names ? model->joints.names[ji] : "?";

    trace_info("  joint[%lld] '%s' parent=%lld", ji, name ? name : "?",
               (isz)joint->parent);
  }

  // == Animations =====================================
  trace_info("  animations: %lld", (isz)model->animations.count);

  for (isz ai = 0; ai < model->animations.count; ai += 1) {
    auto anim = &model->animations.items[ai];

    f64 duration = anim->time_end - anim->time_begin;

    trace_info("  animation[%lld]", ai);
    trace_info("    time_begin       : %.4f", anim->time_begin);
    trace_info("    time_end         : %.4f", anim->time_end);
    trace_info("    duration         : %.4f s", duration);
    trace_info("    joint_animations : %lld",
               (isz)anim->joints_animation.count);

    usz total_keys = 0;

    for (isz ji = 0; ji < (isz)anim->joints_animation.count; ji += 1) {
      auto ja = &anim->joints_animation.items[ji];

      total_keys += ja->translation_keyframes.count +
                    ja->rotation_keyframes.count + ja->scale_keyframes.count;
    }

    trace_info("    total keyframes  : %lld", (isz)total_keys);
  }

  trace_info("[Model] ==========================================");
}

const bool internal please_obj_merge = false;
// Options: https://ufbx.github.io/reference#ufbx_load_opts

static const ufbx_load_opts ufbx_default_opts_godot = {
   .target_axes = ufbx_axes_right_handed_y_up,
   .target_unit_meters = 1.0f,
   .space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY,
#if 1
		.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK,
		.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE_NO_FALLBACK,
#else
		.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES,
		.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE,
#endif
   // Bone stuff
   .connect_broken_elements = false,

	.pivot_handling = UFBX_PIVOT_HANDLING_ADJUST_TO_PIVOT,
	.geometry_transform_helper_name.data = "GeometryTransformHelper",
	.geometry_transform_helper_name.length = SIZE_MAX,
	.scale_helper_name.data = "ScaleHelper",
	.scale_helper_name.length = SIZE_MAX,
	.node_depth_limit = 2048,
	.target_camera_axes = ufbx_axes_right_handed_y_up,
	.target_light_axes = ufbx_axes_right_handed_y_up,
	.clean_skin_weights = true,
	.generate_missing_normals = true
};

static const ufbx_load_opts ufbx_default_opts = {
   // .evaluate_skinning = true,
   .clean_skin_weights = true,
   .index_error_handling  = UFBX_INDEX_ERROR_HANDLING_ABORT_LOADING,
   .connect_broken_elements = true,
   .handedness_conversion_retain_winding = true,

   .generate_missing_normals = true,
   .normalize_normals  = true,
   .normalize_tangents = true,
   .ignore_embedded = false,

   .target_axes        = ufbx_axes_right_handed_y_up,
   .target_camera_axes = ufbx_axes_right_handed_y_up,
   .target_light_axes  = ufbx_axes_right_handed_y_up,

   .strict = true,
   .target_unit_meters = 0.05f,

   // UFBX_GEOMETRY_TRANSFORM_HANDLING_PRESERVE

#if 1
   .space_conversion            = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY, // This one is important to help on getting the animation matrices
   // Modified the geomtry and be done with it
   .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY,
   .inherit_mode_handling       = UFBX_INHERIT_MODE_HANDLING_COMPENSATE,
#elif 0
   // .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES,
   .space_conversion            = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY, // This one is important to help on getting the animation matrices
   .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY_NO_FALLBACK,
   .inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE_NO_FALLBACK,
#else
   // Helper nodes all around
   .space_conversion            = UFBX_SPACE_CONVERSION_ADJUST_TRANSFORMS,
   .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES,
   .inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_HELPER_NODES,
#endif



   	// Don't fail loading if external files are not found.

   .obj_search_mtl_by_filename = true,

   .load_external_files = true,           // IMPORTANT: Auto load mtl and other texture files (unsafe if user defined data)0
   .ignore_missing_external_files = true, // IMPORTANT: Don't fail in case the external file doesn't exist (warn only)

   // (.obj) Don't split geometry into meshes by object.
   .obj_merge_objects = please_obj_merge,
   // (.obj) Don't split geometry into meshes by groups.
   .obj_merge_groups  = please_obj_merge,
   // (.obj) Force splitting groups even on object boundaries.
   .obj_split_groups = !please_obj_merge,
   .obj_unit_meters = 0.5f,
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

	// Wrapping mode
   ufbx_wrap_mode wrap_u = map.texture->wrap_u;
   ufbx_wrap_mode wrap_v = map.texture->wrap_v;
   assert(!map.texture->has_uv_transform);
   assert_msg((UFBX_WRAP_REPEAT == wrap_u) && (wrap_v == UFBX_WRAP_REPEAT), "%s %d %d", scene_path, wrap_u, wrap_v);
   assert_msg(map.texture->layers.count <= 1, "%s %d", scene_path, map.texture->layers.count);


   ZString texture_path = map.texture->filename.data;
   if (!file_exists(texture_path)) {

      ZString texture_base_name = path_base_name(texture_path);
      ZString base_directory = path_dir_of(scene_path);
      texture_path = path_create(base_directory, texture_base_name);

      auto content = map.texture->content;
      if (content.size > 0) {
         // FIX: We're gonna end up loading this twice, first we loaded from content and writting to disk
         //      then read again from disk when loading materials >.<.
         if (!file_exists(texture_path)) {
            write_file(texture_path, content.data, content.size);
         }
         trace_debug("Embeded content inside model %s. Wrote %llu bytes to path '%s'", scene_path, (usz)content.size, texture_path);
         goto done;
      }

      if (!file_exists(texture_path)) {
         base_directory = path_create(path_dir_of(base_directory), "textures");
         texture_path   = path_create(base_directory, texture_base_name);
         if (!file_exists(texture_path)) {
            return nullptr;
         }
      }
   }

done:
   assert_msg(file_exists(texture_path), "We should previously return null if the file doesnt exist, period.");
   trace_debug("`%s` Texture Path exists for scene `%s`", texture_path, scene_path);
   return strdup(texture_path); // @Leak
}


#define UFBX_MAX_WARNING_COUNT 10

void trace_ufbx_warnings(const ufbx_scene *scene) {
   if (!scene) {
      return;
   }

   int warning_count[UFBX_WARNING_TYPE_COUNT] = {0};
   int ignored_warning_count = 0;

   for (usz i = 0; i < scene->metadata.warnings.count; i += 1) {
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
         ignored_warning_count += 1;
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
   rlmat.m8  = (float)ufbxmat.m02;
   rlmat.m9  = (float)ufbxmat.m12;
   rlmat.m10 = (float)ufbxmat.m22;
   rlmat.m11 = 0.0f;

   // fourth column (translation)
   rlmat.m12 = (float)ufbxmat.m03;
   rlmat.m13 = (float)ufbxmat.m13;
   rlmat.m14 = (float)ufbxmat.m23;
   rlmat.m15 = 1.0f;

   return rlmat;
}

// TODO: HashMap, let's be honest, i don't think will ever change this to a map 2025-09-03 Deccan_Lea
usz joint_index_from_ufbx_bone_node(const ufbx_scene *scene, const ufbx_node *bone_node) {
   bool found_bone_index = false;
   usz result_bone_index = 0;
   // @BONE @JOINT
   for (usz bone_index = 0; bone_index < scene->bones.count; bone_index += 1) {
      if (0 == scene->bones.data[bone_index]->instances.count)  {
         // Make sure to ignore bones that arent related to any nodes
         continue;
      }
      auto scene_bone_node = scene->bones.data[bone_index]->instances.data[0];
      assert_msg(1 == scene->bones.data[bone_index]->instances.count, "We assume each bone has exactly 1 instance that correspondes to its node");
      if (scene_bone_node == bone_node) {
         found_bone_index = true;
         break;
      }
      result_bone_index += 1;
   }
   assert_msg(found_bone_index, "Should have found because why does a bone from a cluster is not foundable from the scene bones? ");
   return result_bone_index;
}

Joint_List create_joint_list_from_ufbx_scene(const ufbx_scene *scene) {
   assert(scene);

   Joint_List list = {0};
   if (!list.joints) {
      // joints_size might be overshot because not all bone's have nodes related to it
      usz joints_size = size_of(list.joints[0]) * scene->bones.count;
      usz names_size = size_of(list.names[0]) * scene->bones.count;
      char* data = malloc(joints_size + names_size);
      memset(data, 0, joints_size + names_size);
      list.joints = (typeof(list.joints))(data + 0);
      list.names  = (typeof(list.names ))(data + joints_size);
   } else {
      assert(list.count <= scene->bones.count);
   }

   list.count = 0;
   // @BONE @JOINT
   for (usz bones_index = 0; bones_index < scene->bones.count; bones_index += 1) {
      auto bone = scene->bones.data[bones_index];
      if (0 == bone->instances.count) {
         // TODO: Update warning
         trace_warn(
           "%s %-th ufbx bone has no correspoding node. Counting it as \"zero\" joint (useless but will be added to the joint list)."
           "The belief rn is that if the ufbx bone has no node attached then it's not referenced by the deformers (e.g. irrelevant to animation).",
           __func__, bones_index
         );
         continue;
      }

      Joint *joint = &list.joints[list.count];
      list.count += 1;

      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      auto bone_node = bone->instances.data[0];

      // Copy the joint name
      list.names[list.count] = strdup(bone_node->name.data); // @LEAK

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
      for (usz cluster_index = 0; cluster_index < scene->skin_clusters.count; cluster_index += 1) {
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


// TODO: Single malloc, instead of malloc for each keyframes. Count every size for every data, fire 1 malloc then set all ptrs.
Animation create_animation_from_ufbx(ufbx_scene *scene, ufbx_anim *anim) {
   Animation result = {0};
   result.scene = scene;

   result.joints_animation.count = 0;
   // NOTE: We might be overshooting the count here, since not all bones has a node
   result.joints_animation.items = calloc(scene->bones.count, size_of(Joint_Animation));

   ufbx_error error = {0};
   ufbx_bake_opts opts = {
      .resample_rate = 120,
      .minimum_sample_rate = 120,
      .max_keyframe_segments = 8*1024,
   };
   // Baked animation data is ufbx transforming the fbx data into linearly interpolatable keyframes. Easy enough.
   ufbx_baked_anim *baked = ufbx_bake_anim(scene, anim, &opts, &error);
   if (!baked) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error(err_buf);
   }

   assert_msg(baked->playback_time_begin == anim->time_begin && baked->playback_time_end == anim->time_end, "We are assuming they are equal baked = %f anim = %f", baked->playback_time_end, anim->time_end);

   result.time_begin = baked->playback_time_begin;
   result.time_end   = baked->playback_time_end;
   result.time_begin = anim->time_begin;
   result.time_end   = anim->time_end;


   {  // Counting bones
      usz bone_count = 0;
      for (usz node_index = 0; node_index < scene->nodes.count; node_index += 1) {
         auto node = scene->nodes.data[node_index];
         if (node->bone) {
            bone_count++;
         }
      }
      if (bone_count != scene->bones.count) {
         trace_warn("%s Bone count actually equals %llu/%llu", scene->metadata.filename.data, (usz)bone_count, (usz)scene->bones.count);
      }
   }

   // @BONE @JOINT
   for (u32 bone_index = 0; bone_index < scene->bones.count; bone_index += 1) {
      ufbx_bone *bone = scene->bones.data[bone_index];
      if (0 == bone->instances.count) {
         // TODO: Update warning.
         trace_warn("%-th ufbx bone has no correspoding node. Counting it as \"zero\" joint (useless but will be added to the joint list)."
           "The belief rn is that if the ufbx bone has no node attached then it's not referenced by the deformers (e.g. irrelevant to animation).",
            bone_index
         );
         continue;
      }

      Joint_Animation *ja = &result.joints_animation.items[result.joints_animation.count];
      // We're only couting joints that actually affect animation (e.g. bones that has a node/intance)
      result.joints_animation.count += 1;

      assert_msg(1 == bone->instances.count, "We're assuming instances is how we get THE (as in only one makes sense for us as this moment) node from a bone");
      ufbx_node *node = bone->instances.data[0]; // assuming 1 instance per bone
      assert(node);


      ufbx_baked_node *bnode = ufbx_find_baked_node(baked, node);
      ufbx_baked_node *bnode2 = ufbx_find_baked_node_by_typed_id(baked, node->typed_id);
      assert_msg(bnode == bnode2, "sanity check");
      if (!bnode) {
         continue;
      }

      // Translation
      ja->translation_keyframes.count = bnode->translation_keys.count;
      ja->translation_keyframes.items = malloc(size_of(*ja->translation_keyframes.items) * ja->translation_keyframes.count);
      for (u32 k = 0; k < bnode->translation_keys.count; k += 1) {
         ja->translation_keyframes.items[k].vec3 =
             (Vector3){(float)bnode->translation_keys.data[k].value.x, (float)bnode->translation_keys.data[k].value.y, (float)bnode->translation_keys.data[k].value.z};
         ja->translation_keyframes.items[k].time = bnode->translation_keys.data[k].time;
      }

      // Rotation
      ja->rotation_keyframes.count = bnode->rotation_keys.count;
      ja->rotation_keyframes.items = malloc(size_of(*ja->rotation_keyframes.items) * ja->rotation_keyframes.count);
      for (u32 k = 0; k < bnode->rotation_keys.count; k += 1) {
         ja->rotation_keyframes.items[k].quat =
             (Quaternion){(float)bnode->rotation_keys.data[k].value.x, (float)bnode->rotation_keys.data[k].value.y, (float)bnode->rotation_keys.data[k].value.z, (float)bnode->rotation_keys.data[k].value.w};
         ja->rotation_keyframes.items[k].time = bnode->rotation_keys.data[k].time;
      }

      // Scale
      ja->scale_keyframes.count = bnode->scale_keys.count;
      ja->scale_keyframes.items = malloc(size_of(*ja->scale_keyframes.items) * ja->scale_keyframes.count);
      for (u32 k = 0; k < bnode->scale_keys.count; k += 1) {
         ja->scale_keyframes.items[k].vec3
             = (Vector3){(float)bnode->scale_keys.data[k].value.x, (float)bnode->scale_keys.data[k].value.y, (float)bnode->scale_keys.data[k].value.z};
         ja->scale_keyframes.items[k].time
             = bnode->scale_keys.data[k].time;
      }
   }

   ufbx_free_baked_anim(baked);
   return result;
}

void trace_ufbx_scene_stats(ufbx_scene *scene) {
   auto checkpoint = tsave();
   ZString info = "";
   info = tprintf("%s %d textures for this scene: ", info, scene->textures.count);
   for (usz i = 0; i < scene->textures.count; i += 1) {
      auto texture = *scene->textures.data[i];
      auto base_name = path_base_name(texture.relative_filename.data);
      info = tprintf("%s    texture (%zu): base_name %s\n", info, i, base_name);
      info = tprintf("%s                : %s\n", info, texture.relative_filename.data);
      info = tprintf("%s                : file_textures.count %ld\n", info, texture.file_textures.count);
      assert_msg(false == texture.has_uv_transform, "We do not handle that");
   }

   trestore(checkpoint);
   trace_debug("%s", info);

   info = tprintf("%s %d materials for this scene: ", info, scene->materials.count);
   for (usz i = 0; i < scene->materials.count; i += 1) {
      auto material = *scene->materials.data[i];

      info = tprintf("%s    material '%s' (%zu): has %ld textures\n", info, material.name.data, i, material.textures.count);
      for (usz j = 0; j < material.textures.count; j += 1) {
         auto texture = *(material.textures.data[j].texture);
         auto base_name = path_base_name(texture.relative_filename.data);
         info = tprintf("%s        texture (%zu): base_name %s\n", info, j, base_name);
         info = tprintf("%s                    : %s\n", info, texture.relative_filename.data);
         info = tprintf("%s                    : file_textures.count %ld\n", info, texture.file_textures.count);
         info = tprintf("%s                    : content %p with size %ld\n", info, texture.content.data, texture.content.size);
      }
   }

   trace_debug("%s", info);

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

// TODO: `setup -> create` for consistency
// TODO: Support content.data content.size extraction as models some only have this embeded content.
static void setup_materials_from_ufbx_scene(Model *model, const ufbx_scene *const scene, const char* scene_filepath) {
   // Setup Textures
   if (0 == scene->materials.count) {
      model->materials.count = scene->materials.count;
      model->materials.items = nullptr;
      return;
   }

   if (!model->materials.items) {
      model->materials.count = scene->materials.count;
      model->materials.items = calloc(model->materials.count, size_of(model->materials.items[0]));
   }

   for (usz material_index = 0; material_index < scene->materials.count; material_index += 1) {
      ufbx_material fbx_material = *scene->materials.data[material_index];

      auto material = &model->materials.items[material_index];
      // TODO: Update this when pbr pipeline comes in.
      // Diffuse
      const ufbx_material_map diffuse_maps[] = {fbx_material.pbr.base_color, fbx_material.fbx.diffuse_color};
      for (usz i = 0; i < count_of(diffuse_maps); i += 1) {
         material->diffuse = filepath_from_ufbx_material_map(scene_filepath, diffuse_maps[i]);
         if (material->diffuse) {
            break;
         }
      }

      // Specular
      const ufbx_material_map specular_maps[] = {fbx_material.fbx.specular_color, fbx_material.pbr.roughness, fbx_material.pbr.specular_color, fbx_material.fbx.reflection_factor};
      for (usz i = 0; i < count_of(specular_maps); i += 1) {
         material->specular = filepath_from_ufbx_material_map(scene_filepath, specular_maps[i]);
         if (material->specular) {
            break;
         }
      }

      // Emisse
      material->emissive = filepath_from_ufbx_material_map(scene_filepath, fbx_material.fbx.emission_color);

      const ufbx_material_map normal_maps[] = {fbx_material.pbr.normal_map, fbx_material.fbx.normal_map};
      for (usz i = 0; i < count_of(normal_maps); i += 1) {
         material->normal = filepath_from_ufbx_material_map(scene_filepath, normal_maps[i]);
         if (material->normal) {
            break;
         }
      }
   }
}

#define MODEL_ANIMATIONS_LIMIT 200

static void setup_animations_from_ufbx_scene(Model *model, ufbx_scene *scene, const char* scene_filepath) {
   if (scene->anim_stacks.count > 0) {   // Setup animations
      model->animations.count = scene->anim_stacks.count;
      if (model->animations.count > MODEL_ANIMATIONS_LIMIT) {
         trace_warn("Truncating animation count for %s from %d to %d (-%d)", scene_filepath, model->animations.count, MODEL_ANIMATIONS_LIMIT, model->animations.count - MODEL_ANIMATIONS_LIMIT);
         model->animations.count = MODEL_ANIMATIONS_LIMIT;
      }

      model->animations.items = malloc(model->animations.count * size_of(model->animations.items[0]));
      for (isz i = 0; i < model->animations.count; i += 1) {
         ufbx_anim_stack *stack = scene->anim_stacks.data[i];
         trace_info("[Animation] Model from %s animation stack %d called '%s':\n", scene_filepath, i, stack->name.data);
         Animation animation = create_animation_from_ufbx(scene, stack->anim);
         assert(is_valid_animation(&animation));
         assert(animation.joints_animation.items);
         model->animations.items[i] = animation;
         // break; // TODO: Get mo' animations
      }
   }
}

void generate_tangent_space(Mesh *mesh) {
   SMikkTSpaceInterface mikk_interface = {
      .m_getNumFaces          = mikk_get_num_faces,
      .m_getNumVerticesOfFace = mikk_get_num_vertices_of_face,
      .m_getPosition          = mikk_get_position,
      .m_getNormal            = mikk_get_normal,
      .m_getTexCoord          = mikk_get_tex_coord,
      .m_setTSpaceBasic       = mikk_set_tagent_space_basic,
   };

   Mikk_User_Data user_data = {mesh};

   SMikkTSpaceContext ctx = {
      .m_pInterface = &mikk_interface,
      .m_pUserData  = &user_data,
   };

   genTangSpaceDefault(&ctx);
}

bool mesh_requires_tangents(const ufbx_mesh *mesh) {
   if (!mesh) {
      return false;
   }

   // if the mesh already has tangents, we don't need to regenerate. But we're gonna anyways
   if (mesh->vertex_tangent.values.count > 0 || mesh->vertex_bitangent.values.count > 0) {
      // return (undecided for now);
   }

   // Tangents require valid UVs (can't generate without them).
   if (!(mesh->vertex_uv.values.count > 0)) {
      return false;
   }

   // Check if any material assigned to the mesh uses a normal map.
   for (size_t i = 0; i < mesh->materials.count; i++) {
      const ufbx_material *mat = mesh->materials.data[i];
      if (!mat) {
         continue;
      }

      if (mat->pbr.normal_map.texture_enabled || mat->fbx.normal_map.texture_enabled || 0 /* Other maps in the future would go here */ ) {
         return true;
      }
   }

   // No material with a normal map found, no need for tangents
   return false;
}


static Mesh create_mesh_from_ufbx_node(ufbx_node *node, ufbx_scene *scene) {
   static constexpr int MAX_WEIGHTS = 4;

   auto fbx_mesh = node->mesh;
   assert(fbx_mesh);
   if (fbx_mesh->uv_sets.count > 1) {
      trace_warn("fbx mesh %s has more than 1 uv_sets (%llu) which we're ignoring.", node->name.data, (usz)fbx_mesh->uv_sets.count);
   }

   if (fbx_mesh->reversed_winding) {
      trace_warn("fbx mesh %s had its winding reversed", node->name.data);
   }

   if (fbx_mesh->blend_deformers.count > 0) {
      trace_warn("We ain't support blend deformers. node %s scene %s blend_deformers count = %lld.", node->name.data, scene->metadata.filename.data, (isz)fbx_mesh->blend_deformers.count);
   }

   usz checkpoint = tsave();

   usz total_triangles = fbx_mesh->num_triangles;
   usz total_indices   = total_triangles * 3;

   isz  tri_indices_count = fbx_mesh->max_face_triangles * 3;
   u32 *tri_indices       = talloc(tri_indices_count * size_of(u32));

   // old way was bool has_bones = scene->bones.count > 0;
   bool has_bones = fbx_mesh->skin_deformers.count > 0;

   usz surfaces_count = fbx_mesh->material_parts.count;

   // Allocate mesh data
   Mesh mesh = {0};
   {
      usz total_size = 0;

      usz positions_size = total_indices             * size_of(mesh.vertices.positions[0]);
      usz normals_size   = total_indices             * size_of(mesh.vertices.normals[0]);
      usz uvs_size       = total_indices             * size_of(mesh.vertices.uvs[0]);
      usz joints_size    = has_bones ?
                           total_indices             * size_of(mesh.vertices.joints[0]) : 0;
      usz indices_size   = total_indices             * size_of(u32);
      usz surfaces_size  = surfaces_count            * size_of(mesh.surfaces.items[0]);

      total_size = positions_size + normals_size + uvs_size + indices_size + surfaces_size + joints_size;

      // Allocate a single block of memory
      char *data = malloc(total_size);

      // Assign pointers
      mesh.vertices.positions = (Vector3*)(data);
      mesh.vertices.normals   = (Vector3*)(data + positions_size);
      mesh.vertices.uvs       = (Vector2*)(data + positions_size + normals_size);
      mesh.vertices.joints    =
         has_bones ?            (void   *)(data + positions_size + normals_size + uvs_size) : nullptr;
      mesh.indices.items      = (u32    *)(data + positions_size + normals_size + uvs_size + joints_size);
      mesh.surfaces.items     = (void   *)(data + positions_size + normals_size + uvs_size + joints_size + indices_size);
   }


   auto skin = fbx_mesh->skin_deformers.count > 0 ? fbx_mesh->skin_deformers.data[0] : nullptr;

   isz total_vertex_count = 0;
   isz total_index_count = 0;
   usz max_weight_count_found = 0;
   bool needs_tanget_space = mesh_requires_tangents(fbx_mesh);

   // Process each material part (surface), always at least 1.
   for (usz part_index = 0; part_index < fbx_mesh->material_parts.count; part_index += 1) {
      ufbx_mesh_part material_part = fbx_mesh->material_parts.data[part_index];
      assert(material_part.num_triangles > 0);

      auto surface = &mesh.surfaces.items[part_index];

      auto fbx_material = node->materials.data[part_index];

      // Set surface start
      surface->indices_offset = total_index_count;
      surface->material_index = part_index < node->materials.count ?
            material_index_from_ufbx_scene(fbx_material, scene)
          :-1;


      isz part_vertex_start = total_vertex_count;

      // Process faces in this material part
      for (usz face_idx = 0; face_idx < material_part.face_indices.count; face_idx += 1) {
         u32 face_index = material_part.face_indices.data[face_idx];
         ufbx_face face = fbx_mesh->faces.data[face_index];


         static ufbx_panic panic = {0};
         static const bool use_panic_triangulate = true;
         u32 tri_count = use_panic_triangulate ?
              ufbx_catch_triangulate_face(&panic, tri_indices, tri_indices_count, fbx_mesh, face)
            : ufbx_triangulate_face      (        tri_indices, tri_indices_count, fbx_mesh, face);

         if (panic.did_panic) {
            trace_fatal("Failed triangule on %s, %s", scene->metadata.original_file_path.data, panic.message);
         }

         // Process triangles in this face
         for (isz tri_index = 0; tri_index < tri_count * 3; tri_index += 1) {
            u32 index = tri_indices[tri_index];

            // Get vertex data
            ufbx_vec3 ufbx_position = ufbx_get_vertex_vec3(&fbx_mesh->vertex_position, index);
            // ufbx_vec3 ufbx_normal   = ufbx_get_vertex_vec3(&fbx_mesh->vertex_normal, index);
            ufbx_vec3 ufbx_normal   = ufbx_catch_get_vertex_vec3(&panic, &fbx_mesh->vertex_normal, index);
            if (panic.did_panic) {
               trace_fatal("Failed get normal on %s, %s", scene->metadata.original_file_path.data, panic.message);
            }
            assert_msg(!isnan(ufbx_normal.x) && !isnan(ufbx_normal.y) && !isnan(ufbx_normal.z), "Found when loading nan normals");
            assert(fbx_mesh->uv_sets.count);

            static const bool uv_from_sets = false;
            ufbx_vec2 ufbx_uv = ufbx_get_vertex_vec2(&fbx_mesh->vertex_uv, index);
            if (uv_from_sets) {
               // Just as a reminder that we might need to support this later on.
               ufbx_uv = ufbx_get_vertex_vec2(&fbx_mesh->uv_sets.data[0].vertex_uv, index);
            }

            assert_msg(!isnan(ufbx_uv.x) && !isnan(ufbx_uv.y), "Found when loading nan uvs");

            Vector3 position = {(f32)ufbx_position.x, (f32)ufbx_position.y, (f32)ufbx_position.z};

            // We dont need to normalize since we passed .normalize_normals = true to ufbx
            // Vector3 normal   = Vector3Normalize((Vector3){(f32)ufbx_normal.x, (f32)ufbx_normal.y, (f32)ufbx_normal.z});
            Vector3 normal   = (Vector3){(f32)ufbx_normal.x, (f32)ufbx_normal.y, (f32)ufbx_normal.z};

            Vector2 uv       = {(f32)ufbx_uv.x, (f32)ufbx_uv.y};

            // Handle skinning data
            if (skin && mesh.vertices.joints) {
               uint32_t vertex = fbx_mesh->vertex_indices.data[index];
               ufbx_skin_vertex skin_vertex = skin->vertices.data[vertex];
               usz num_weights = skin_vertex.num_weights;
               if (num_weights > MAX_WEIGHTS) {
                  max_weight_count_found = max(num_weights, max_weight_count_found);
                  num_weights = MAX_WEIGHTS;
               }
               float total_weight      = 0.0f;
               Vector4    bone_weight  = {0, 0, 0, 0};
               Vector4Int bone_indices = {0, 0, 0, 0};

               for (usz i = 0; i < num_weights; i += 1) {
                  ufbx_skin_weight skin_weight = skin->weights.data[skin_vertex.weight_begin + i];
                  ufbx_skin_cluster *cluster = skin->clusters.data[skin_weight.cluster_index];
                  usz bone_index = joint_index_from_ufbx_bone_node(scene, cluster->bone_node);

                  bone_indices.items[i] = (int  )bone_index;
                  bone_weight.items [i] = (float)skin_weight.weight;
                  total_weight += (float)skin_weight.weight;
               }

               // Normalize weights
               if (total_weight > 0.0f) {
                  for (usz i = 0; i < num_weights; i += 1) {
                     bone_weight.items[i] /= total_weight;
                  }
               }

               mesh.vertices.joints[total_vertex_count].indices = bone_indices;
               mesh.vertices.joints[total_vertex_count].weights = bone_weight;
            }



            // Store vertex data
            mesh.vertices.positions[total_vertex_count] = position;
            mesh.vertices.normals  [total_vertex_count] = normal;
            mesh.vertices.uvs      [total_vertex_count] = uv;
            mesh.indices.items     [total_index_count]  = total_vertex_count;

            total_vertex_count += 1;
            total_index_count += 1;
         }
      }


      // Set surface index count
      surface->indices_count = total_index_count - surface->indices_offset;
   }

   if (max_weight_count_found) {
      trace_warn("We're ignoring some joint influences. %s max_weight_count_found %llu > max_weights %llu",
         scene->metadata.filename.data, (usz)max_weight_count_found, (usz)MAX_WEIGHTS
      );
   }

   // Set final counts
   mesh.vertices.count = total_vertex_count;
   mesh.indices.count  = total_index_count;
   mesh.surfaces.count = surfaces_count;

   trestore(checkpoint);

   // Optimize with vertex deduplication
   const bool reduce_indices = true;
   if (reduce_indices) {
      ufbx_vertex_stream streams[] = {
         {mesh.vertices.positions,  total_vertex_count, size_of(mesh.vertices.positions[0])},
         {mesh.vertices.normals,    total_vertex_count, size_of(mesh.vertices.normals[0])},
         {mesh.vertices.uvs,        total_vertex_count, size_of(mesh.vertices.uvs[0])},
         // joints has to be last
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

   if (needs_tanget_space) {
      trace_okay("Generating tangent space for node %s scene %s.", node->name.data, scene->metadata.filename.data);
      // TODO: make this function check for already allocated memory to allow pre-allocation
      generate_tangent_space(&mesh);
   }

   return mesh;
}


// void trace_model(const Model *model) {
//    if (!model) {
//       return;
//    }
//
//    trace_struct(*model);
//    for (isz mesh_index = 0; mesh_index < model->meshes.count; mesh_index += 1) {
//       auto mesh = model->meshes.items[mesh_index];
//       trace_struct(mesh);
//       for (isz surface_index = 0; surface_index < (isz)mesh.surfaces.count; surface_index += 1) {
//          auto surface = mesh.surfaces.items[surface_index];
//          trace_info("mesh = %d, surface %d", mesh_index, surface_index);
//          trace_struct(surface);
//       }
//    }
// }

Model create_model(const char *filepath) {
   Model model = {0};
   ZString scene_filepath = filepath;
   ufbx_error error; // Optional, pass nullptr if you don't care about errors
   ufbx_scene *scene = ufbx_load_file(scene_filepath, &ufbx_default_opts, &error);
   const bool evaluate_scene_for_debugging_purposes = false;
   if (evaluate_scene_for_debugging_purposes) {
      scene = ufbx_evaluate_scene(scene, scene->anim, 3.5, nullptr, &error);
   }

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return model;
   }

   trace_ufbx_warnings(scene);
   trace_ufbx_scene_stats(scene);

   {
      setup_animations_from_ufbx_scene(&model, scene, scene_filepath);
   }

   {
      setup_materials_from_ufbx_scene(&model, scene, scene_filepath);
   }

   {  //  Setup le joints/bones
      model.joints = create_joint_list_from_ufbx_scene(scene);
   }


   assert_msg(scene->meshes.count > 0, "Right now we don't think it's particularly useful to load a model with no meshes. But there's a world we load a model for just animations or bone or something that doesn't hav meshes.");


   model.meshes.count = 0;
   model.meshes.items = calloc(scene->meshes.count, size_of(model.meshes.items[0]));
   for (usz node_index = 0; node_index < scene->nodes.count; node_index += 1) {
      ufbx_node *node = scene->nodes.data[node_index];
      if (nullptr == node->mesh) {
         continue;
      }
      auto mesh = create_mesh_from_ufbx_node(node, scene);
      model.meshes.items[model.meshes.count++] = mesh;
   }

   trace_info("[Animation] Model from %s model.meshes.count %d ", scene_filepath, model.meshes.count);

   assert(scene->meshes.count == (usz)model.meshes.count);

   // @REMOVEME: when we're done debugging animation, make sure to free the scene
   // ufbx_free_scene(scene);

   if (model.animations.count > 0 && 0 == model.joints.count) {
      trace_fatal("We DO NOT handle animations with no joints, does that even make sense? Maybe for retargeting.");
   }

   return model;
}

Model create_model_from_mesh(Mesh mesh, const char *diffuse_path, const char *specular_path, const char *emissive_path, const char *normal_path) {
   Model model = {0};
   // Calculate total memory needed for paths
   usz total_path_len = 0;
   if (diffuse_path)
      total_path_len += strlen(diffuse_path) + 1;
   if (specular_path)
      total_path_len += strlen(specular_path) + 1;
   if (emissive_path)
      total_path_len += strlen(emissive_path) + 1;
   if (normal_path)
      total_path_len += strlen(normal_path) + 1;

   usz total_bytes = size_of(Mesh) + size_of(*model.materials.items) + total_path_len;
   byte *memory_block = malloc(total_bytes);
   if (!memory_block)
      return (Model){0};

   byte *ptr = memory_block;
   Mesh *mesh_ptr = (Mesh *)ptr;
   ptr += size_of(Mesh);

   typeof(*model.materials.items) *material_ptr = (void *)ptr;
   ptr += size_of(*model.materials.items);

   // Copy mesh and update material indices
   *mesh_ptr = mesh;
   for (u32 i = 0; i < mesh_ptr->surfaces.count; i++) {
      mesh_ptr->surfaces.items[i].material_index = 0;
   }

   material_ptr->diffuse = diffuse_path ? (const char *)ptr : nullptr;
   if (diffuse_path) {
      usz len = strlen(diffuse_path) + 1;
      memcpy(ptr, diffuse_path, len);
      ptr += len;
   }

   material_ptr->specular = specular_path ? (const char *)ptr : nullptr;
   if (specular_path) {
      usz len = strlen(specular_path) + 1;
      memcpy(ptr, specular_path, len);
      ptr += len;
   }

   material_ptr->emissive = emissive_path ? (const char *)ptr : nullptr;
   if (emissive_path) {
      usz len = strlen(emissive_path) + 1;
      memcpy(ptr, emissive_path, len);
      ptr += len;
   }

   material_ptr->normal = normal_path ? (const char *)ptr : nullptr;
   if (normal_path) {
      usz len = strlen(normal_path) + 1;
      memcpy(ptr, normal_path, len);
      ptr += len;
   }

   model.meshes.items = mesh_ptr;
   model.meshes.count = 1;
   model.materials.items = material_ptr;
   model.materials.count = 1;

   return model;
}

Model overload create_model_from_mesh(Mesh mesh) {
   return create_model_from_mesh(mesh, nullptr, nullptr, nullptr, nullptr);
}

// NOTE: The textures strings gotta live as long as the model (static please)
Model create_sphere_model(float radius, int rings, int slices,
      const char *diffuse_texture,
      const char *specular_texture,
      const char *emissive_texture,
      const char *normal_texture
) {
   Model model = {0};
   byte *ptr = malloc(size_of(model.meshes.items[0]) + size_of(model.materials.items[0]));

   model.meshes.count = 1;
   model.meshes.items = (void*)ptr;
   model.meshes.items[0] = generate_sphere_mesh(radius, rings, slices);
   model.meshes.items[0].surfaces.items[0].material_index = 0;

   model.materials.count = 1;
   model.materials.items = (void*)(ptr + size_of(model.meshes.items[0]));
   model.materials.items[0].diffuse  = diffuse_texture;
   model.materials.items[0].specular = specular_texture;
   model.materials.items[0].emissive = emissive_texture;
   model.materials.items[0].normal   = normal_texture;
   return model;
}

Model create_torus_model(
   unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
   Mesh mesh = generate_torus_mesh(1.0f, 0.01f, 4 * 32, 32);
   return create_model_from_mesh(mesh, nullptr, nullptr, nullptr, nullptr);
}

Model create_cube_model(
   const char *diffuse_tex,
   const char *specular_tex,
   const char *emissive_tex,
   const char *normal_tex
) {
   static constexpr float interleaved[] = {
      // positions          // normals           // texture coords
      // Front face (z = 0.5) - Counter-clockwise when viewed from outside
      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
       0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
      -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
      -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

      // Back face (z = -0.5) - Counter-clockwise when viewed from outside
      -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
      -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
       0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
       0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
       0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
      -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,

      // Left face (x = -0.5) - Counter-clockwise when viewed from outside
      -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
      -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
      -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
      -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
      -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
      -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,

      // Right face (x = 0.5) - Counter-clockwise when viewed from outside
       0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
       0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
       0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
       0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
       0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
       0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

      // Bottom face (y = -0.5) - Counter-clockwise when viewed from outside
      -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
       0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
       0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
       0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
      -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
      -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

      // Top face (y = 0.5) - Counter-clockwise when viewed from outside
      -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
      -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
       0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
       0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
      -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
   };

   Model model = {0};

   // 1 mesh and 1 material
   size_t meshes_size    = size_of(Mesh);
   size_t materials_size = size_of(*model.materials.items);
   byte *memory = malloc(meshes_size + materials_size);

   model.meshes.count = 1;
   model.meshes.items = (Mesh*)memory;
   model.meshes.items[0] = create_mesh_from_interleaved(interleaved, count_of(interleaved));
   model.meshes.items[0].surfaces.items[0].material_index = 0;

   model.materials.count = 1;
   model.materials.items = (typeof(model.materials.items))(memory + meshes_size);

   model.materials.items[0].diffuse  = diffuse_tex;
   model.materials.items[0].specular = specular_tex;
   model.materials.items[0].emissive = emissive_tex;
   model.materials.items[0].normal   = normal_tex;
   generate_tangent_space(&model.meshes.items[0]);
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

Geometry_To_World_List joint_matrices_from_model(Model *model, double time) {
   assert(model->animations.count == 1);
   return joint_matrices_from_animation(&model->joints, &model->animations.items[0], time);
}

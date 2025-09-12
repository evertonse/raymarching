
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
      } *items;
      u32 count;
   } translation_keyframes, scale_keyframes, rotation_keyframes;

} Joint_Animation;

#include "ufbx.h" // @REMOVEME


typedef struct {
   ZString name;
   // Each joint has its one set of keyframes that needs to go through at a certain time
   // So we have a Joint_Animation for each joint Joint.
   struct {
      Joint_Animation *items;
      u32 count; // Must be the same amount as joints in Joint_List and in turn we have the same amount of scene->bones.count at the time of load from .fbx file.
   } joints_animation;

   double time_begin;
   double time_end;
   ufbx_scene *scene;
} Animation;

static bool is_valid_keyframe_order(
   const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys,
   u32 count,
   const char *name
) {
   for (u32 i = 1; i < count; i++) {
      double prev_time = keys[i - 1].time;
      double curr_time = keys[i].time;
      assert(prev_time >= 0.0  && curr_time >= 0);

      if (curr_time < prev_time) {
         trace_fatal(
            "Keyframe order error in %s: index %u (time=%f) "
            "is earlier than index %u (time=%f)\n",
            name, i, curr_time, i - 1, prev_time
         );
         return false;
      }
   }
   return true;
}

bool is_valid_joint_animation(const Joint_Animation *joint) {
   if (!joint) {
      trace_error("Invalid Joint_Animation pointer (null)\n");
      return false;
   }

   bool ok = true;

   if (!is_valid_keyframe_order(joint->translation_keyframes.items, joint->translation_keyframes.count, "translation")) {
      ok = false;
   }

   if (!is_valid_keyframe_order(joint->scale_keyframes.items, joint->scale_keyframes.count, "scale")) {
      ok = false;
   }

   if (!is_valid_keyframe_order(joint->rotation_keyframes.items, joint->rotation_keyframes.count, "rotation")) {
      ok = false;
   }

   return ok;
}

bool is_valid_animation(const Animation *anim) {
   if (!anim) {
      trace_error("Invalid Animation pointer (null)\n");
      return false;
   }

   bool ok = true;

   if (!anim->scene) {
      trace_error("Animation '%s' has null scene\n",
         anim->name ? anim->name : "(unnamed)");
      ok = false;
   }

   if (anim->time_begin < 0.0) {
      trace_error(
         "Animation '%s' has invalid negative time start: begin=%f\n",
         anim->name ? anim->name : "(unnamed)",
         anim->time_begin
      );
      ok = false;
   }

   if (anim->time_end <= anim->time_begin) {
      trace_error(
         "Animation '%s' has invalid time range: begin=%f end=%f\n",
         anim->name ? anim->name : "(unnamed)",
         anim->time_begin, anim->time_end
      );
      ok = false;
   }

   if (anim->joints_animation.count > 0 && !anim->joints_animation.items) {
      trace_error(
         "Animation '%s' has joints_animation.count=%u but items=null\n",
         anim->name ? anim->name : "(unnamed)",
         anim->joints_animation.count
      );
      ok = false;
   }

   for (u32 i = 0; i < anim->joints_animation.count; i++) {
      if (!is_valid_joint_animation(&anim->joints_animation.items[i])) {
         trace_error(
            "Animation '%s' joint %u failed validation\n",
            anim->name ? anim->name : "(unnamed)", i
         );
         ok = false;
      }
   }

   return ok;
}

///-------------------------- Copies --------------------------///


Joint_List joint_list_deep_copy(const Joint_List *src) {
   Joint_List dst = zero_of(Joint_List);

   if (src->count == 0) {
      return dst;
   }

   size_t total_bytes = 0;

   // Add space for joints array
   total_bytes += src->count * sizeof(Joint);

   // Add space for names array (array of ZString pointers)
   total_bytes += src->count * sizeof(ZString);

   // Add space for each name string
   for (u32 i = 0; i < src->count; i++) {
      if (src->names[i] != nullptr) {
         total_bytes += strlen(src->names[i]) + 1; // +1 for null terminator
      }
   }

   // Single allocation for all data
   uint8_t *memory_block = malloc(total_bytes);
   if (!memory_block) {
      return zero_of(Joint_List);
   }

   uint8_t *ptr = memory_block;

   // Copy joints array
   dst.joints = (Joint *)ptr;
   dst.count = src->count;
   ptr += src->count * sizeof(Joint);
   memcpy(dst.joints, src->joints, src->count * sizeof(Joint));

   // Copy the pointers to strings
   dst.names = (ZString *)ptr;
   ptr += src->count * sizeof(ZString);

   // New we copy each name string
   for (u32 i = 0; i < src->count; i++) {
      if (src->names[i] != nullptr) {
         size_t name_len = strlen(src->names[i]) + 1;
         dst.names[i] = (ZString)ptr;
         memcpy(ptr, src->names[i], name_len);
         ptr += name_len;
      } else {
         dst.names[i] = nullptr;
      }
   }

   dst.hierarchy_transform = src->hierarchy_transform;

   return dst;
}

void destroy_joint_list(Joint_List *list) {
   if (list->joints != nullptr) {
      free(list->joints);
   }
   *list = zero_of(*list);
}

Animation animation_deep_copy(const Animation *src) {
   Animation dst = {0};

   // Copy basic timing fields
   dst.time_begin   = src->time_begin;
   dst.time_end     = src->time_end;

   usz total_bytes = 0;

   // Add space for joints array
   total_bytes += src->joints_animation.count * size_of(Joint_Animation);

   // Add space for all keyframe data
   for (u32 i = 0; i < src->joints_animation.count; i++) {
      const Joint_Animation *joint = &src->joints_animation.items[i];
      total_bytes += joint->translation_keyframes.count * size_of(*joint->translation_keyframes.items);
      total_bytes += joint->scale_keyframes.count       * size_of(*joint->scale_keyframes.items);
      total_bytes += joint->rotation_keyframes.count    * size_of(*joint->rotation_keyframes.items);
   }

   // Add space for name (if it exists)
   usz name_len = src->name ? strlen(src->name) + 1 : 0;
   total_bytes += name_len;

   // Single allocation for all data
   byte *memory_block = malloc(total_bytes);
   if (!memory_block) {
      return (Animation){0};
   }

   byte *ptr = memory_block;

   // Copy joints array
   if (src->joints_animation.count > 0) {
      dst.joints_animation.items = (Joint_Animation *)ptr;
      dst.joints_animation.count = src->joints_animation.count;
      ptr += src->joints_animation.count * size_of(Joint_Animation);

      // Copy each joint's data
      for (u32 i = 0; i < src->joints_animation.count; i++) {
         const Joint_Animation *src_joint = &src->joints_animation.items[i];
         Joint_Animation *dst_joint = &dst.joints_animation.items[i];

         // Copy translation keyframes
         if (src_joint->translation_keyframes.count > 0) {
            usz size = src_joint->translation_keyframes.count * size_of(*src_joint->translation_keyframes.items);
            dst_joint->translation_keyframes.items = (typeof(src_joint->translation_keyframes.items))ptr;
            dst_joint->translation_keyframes.count = src_joint->translation_keyframes.count;
            memcpy(ptr, src_joint->translation_keyframes.items, size);
            ptr += size;
         }

         // Copy scale keyframes
         if (src_joint->scale_keyframes.count > 0) {
            usz size = src_joint->scale_keyframes.count * size_of(*src_joint->scale_keyframes.items);
            dst_joint->scale_keyframes.items = (typeof(src_joint->scale_keyframes.items))ptr;
            dst_joint->scale_keyframes.count = src_joint->scale_keyframes.count;
            memcpy(ptr, src_joint->scale_keyframes.items, size);
            ptr += size;
         }

         // Copy rotation keyframes
         if (src_joint->rotation_keyframes.count > 0) {
            usz size = src_joint->rotation_keyframes.count * size_of(*src_joint->rotation_keyframes.items);
            dst_joint->rotation_keyframes.items = (typeof(src_joint->rotation_keyframes.items))ptr;
            dst_joint->rotation_keyframes.count = src_joint->rotation_keyframes.count;
            memcpy(ptr, src_joint->rotation_keyframes.items, size);
            ptr += size;
         }
      }
   }

   // Copy name
   if (name_len > 0) {
      dst.name = (char *)ptr;
      memcpy(ptr, src->name, name_len);
   }

   return dst;
}

// Only when deep copied because we know we only use a single malloc
void animation_deep_free(Animation* anim) {
    if (anim->joints_animation.items) {
        free(anim->joints_animation.items);
    }
    *anim = (Animation){0};
}

///-------------------------- Keyframes --------------------------///

// NOTE: We could pass an offset + size into .time instead of this typeof
static inline isz find_keyframe_interval_old(const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys, u32 count, double time) {
   assert_msg(count >= 2, "The caller should have checked this");

   isz low = 0, high = count - 2;
   while (low <= high) {
      isz mid = (low + high) / 2;
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
   // Time is past the last keyframe time, so just clamp to last two frames
   return count - 2;
}

// In this version we're trying not to pay for 2 ifs at the start always
static inline isz find_keyframe_interval(
   const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys,
   u32 count,
   double time
) {
   assert_msg(count >= 2, "The caller should have checked this");

   isz low = 0, high = count - 2; // search over intervals

   while (low <= high) {
      isz mid = (low + high) / 2;
      double t0 = keys[mid].time;
      double t1 = keys[mid + 1].time;
      if (time < t0) {
         // We are before this interval, move left
         high = mid - 1;
      } else if (time > t1) {
         // We are after this interval, move right
         low = mid + 1;
      } else {
         // time ∈ [t0, t1]
         return mid;
      }
   }

   // Fell out of the loop → clamp
   return (time < keys[0].time) ? 0 : (count - 2);
}

// @REMOVEME change comments
static inline isz find_keyframe_interval2(const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys, u32 count, double time) {
   assert_msg(count >= 2, "The caller should have checked this");

   // Handle edge cases first
   if (time <= keys[0].time) {
      return 0; // Before first keyframe - use first interval
   }
   if (time >= keys[count - 1].time) {
      return count - 2; // After last keyframe - use last interval
   }

   isz low = 0, high = count - 2; // high is max valid interval index
   while (low <= high) {
      isz mid = (low + high) / 2;

      // Check if time is in the interval [keys[mid].time, keys[mid + 1].time]
      if (time >= keys[mid].time && time <= keys[mid + 1].time) {
         return mid;
      } else if (time < keys[mid].time) {
         high = mid - 1;
      } else { // time > keys[mid + 1].time
         low = mid + 1;
      }
   }

   assert_msg(false, "fuck you");
   // Should never reach here with proper input, but just in case
   return count - 2;
}

static isz inline find_keyframe_interval_linear(
   const typeof(((Joint_Animation *)0)->translation_keyframes.items) keys,
   u32 count,
   double time
) {
   assert_msg(count >= 2, "The caller should have checked this");

   for (u32 i = 0; i < count - 1; i++) {
      double t0 = keys[i].time;
      double t1 = keys[i + 1].time;

      if (time >= t0 && time <= t1) {
         return (int)i;
      }
   }

   return count-2; // time outside keyframe range
}


// TODO: Sketchy function, idk, i don't like it, is it fine? Can we do something about it?
static inline void interpolate_from_keyframes(const typeof(((Joint_Animation *)0)->translation_keyframes) keyframes, double time, Vector3 *out_vector3, Quaternion* out_quat) {
   assert_msg(out_quat && !out_vector3 || !out_quat && out_vector3, "Dont love this, there's a wrong way of calling this function");

   // Whatever the caller think this is good (don't change shit)
   if (keyframes.count == 0) {
      return;
   }

   // Just use the first one
   if (keyframes.count == 1) {
      if (out_vector3) {
         *out_vector3 = keyframes.items[0].vec3;
      } else {
         *out_quat    = keyframes.items[0].quat;
      }
      return;
   }

   assert_msg(keyframes.count > 1, "We just checked this bruh, but a smart person know code evolves into shit that seems retarded a week later, but it wasn't at first.");

   //
   // TODO: Eventually we'd like to remember where we were last time (index) to make a binary serach with low
   //       starting with this cached value, and also we'd like to know if we're going backswards or forwards in the animation.
   //       These animation interpolation add up quick, if we're on last keyframe and our search is linear you drop from 400 to 40 fps
   //       Binary serach is slower than instantly finding the index, that is if we're on the first frame using linear will beat binary serach by a 100 fps or less.
   //       That tells me that this function interpolation function is worth gettint it right.
   //       We don't have full knowledge yet because there's still the ideia of smooth transistion from one animation to another
   //       That means that the architectur isn't yet clear, but it will be. Might need a cached index per animation per joint.
   //       Also we might want to multi-thread this I think it's pretty doable, but there's compute skinning that I'll read about it to get more intuition of the best way to approach this 
   //       speding as little engineering hours with the best performance.
   //       TLDR: This can get slow, but binary search is fine for now.
   //
   isz index = 1;
#ifdef RENDERER_DEBUG
   if (is_button_pressed(BUTTON_F5)) {
      index = find_keyframe_interval_linear(keyframes.items, keyframes.count, time);
   } else if (is_button_pressed(BUTTON_F6)) {
      index = find_keyframe_interval(keyframes.items, keyframes.count, time);
   } else if (is_button_pressed(BUTTON_F7)) {
      index = find_keyframe_interval2(keyframes.items, keyframes.count, time);
   } else if (is_button_pressed(BUTTON_F8)) {
      index = find_keyframe_interval_old(keyframes.items, keyframes.count, time);
   } else {

      index = find_keyframe_interval(keyframes.items, keyframes.count, time);
      isz index2 = find_keyframe_interval2(keyframes.items, keyframes.count, time);
      // assert_msg(index2 == index, "find_keyframe_interval2 %lld", index2);

      isz old_index = find_keyframe_interval_old(keyframes.items, keyframes.count, time);

      if (is_debugging() && (200 == old_index &&  1999  == index) && 2001 == keyframes.count) {
         debug_break();
      }
      bool fucked_up = false;

      if (old_index != index) {
         auto a = keyframes.items[index];
         auto b = keyframes.items[index + 1];

         auto c = keyframes.items[old_index];
         auto d = keyframes.items[old_index + 1];
         if (
            // If they're *exactly* equal then it doesnt matter
            a.time != time && b.time != time && c.time != time && d.time != time
         ) {
            trace_error(
                  "%s: They should match index=%lld old_index=%lld keyframes.count = %lld\n"
                  "time = %f, ([%d]a=%f [%d]b=%f), ([%d]c=%f [%d]d=%f)",
                  __func__, index, old_index, (isz)keyframes.count,
                  time, index, a.time, index + 1, b.time, old_index, c.time, old_index + 1, d.time
            );
            fucked_up = true;

            debug_break();
            old_index = find_keyframe_interval_old(keyframes.items, keyframes.count, time);
            index = find_keyframe_interval(keyframes.items, keyframes.count, time);
            index2 = find_keyframe_interval2(keyframes.items, keyframes.count, time);
         }
      }

      if (index2 != index) {
         auto a = keyframes.items[index];
         auto b = keyframes.items[index + 1];

         auto c = keyframes.items[index2];
         auto d = keyframes.items[index2 + 1];
         if (
            // If they're *exactly* equal then it doesnt matter
            a.time != time && b.time != time && c.time != time && d.time != time
         ) {
            trace_error(
                  "%s: They should match index=%lld index2=%lld keyframes.count = %lld\n"
                  "time = %f, ([%d]a=%f [%d]b=%f), ([%d]c=%f [%d]d=%f)",
                  __func__, index, index2, (isz)keyframes.count,
                  time, index, a.time, index + 1, b.time, index2, c.time, index2 + 1, d.time
            );
            debug_break();
            index2 = find_keyframe_interval2(keyframes.items, keyframes.count, time);
            index = find_keyframe_interval(keyframes.items, keyframes.count, time);
            old_index = find_keyframe_interval_old(keyframes.items, keyframes.count, time);
            fucked_up = true;
         }
      }
      if (fucked_up) {
         debug_break();
      }
   }
#else
   index = find_keyframe_interval(keyframes.items, keyframes.count, time);
#endif


   assert_msg(
          (index      >= 0 &&  index      < keyframes.count)
      && ((index + 1) >= 0 && (index + 1) < keyframes.count),
      "At this point every index should be within bounds (index = %lld) (index + 1 = %lld)",
      index, index + 1
   );

   auto a = keyframes.items[index];
   auto b = keyframes.items[index + 1];

   // NOTE: It's possible for 'alpha' to be more than 1, because the this last keyframe might
   //       end earlier than the actual animation end time.
   //       Ex: Animation Mari end at 17.9499 but the key frames for a joint might only for as far as 17.933.
   //
   //       In that case, we can choose to simply let the interpolation with an overshot or clamp to 1.0
   //       to effectively only use the 'b', that would be the last keyframe in the list. Clamping or only using the b.vec3 or b.quat might be more correct.
   //       But not clamping would allow more movement extrapolated from previous which might be desired.
   //
   float alpha = (float)((time - a.time) / (b.time - a.time));
   // float alpha = clamp((time - a.time) / (b.time - a.time), 0.0, 1.0);
   if (out_vector3) {
      *out_vector3 = Vector3Lerp(a.vec3, b.vec3, alpha);
   } else {
      *out_quat    = QuaternionSlerp(a.quat, b.quat, alpha);
      // *out_quat    = QuaternionNlerp(a.quat, b.quat, alpha);
      // *out_quat    = QuaternionLerp(a.quat, b.quat, alpha);

   }
}

void update_joints_transforms(Joint_List *joint_list, const Animation *animation, double time) {
   for (u32 i = 0; i < joint_list->count; i += 1) {
      const Joint_Animation *ja = &animation->joints_animation.items[i];

      Vector3    T = {0.0, 0.0, 0.0};
      Quaternion R = {0.0, 0.0, 0.0, 1.0};
      Vector3    S = {1.0, 1.0, 1.0};

      // These should have a better api, like passing pointers to decide which function to interpolate with is kinda gross, no?
      // Returning a tuple would be ugly as well. Whatever for now (prolly ever).
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
   union {
      float16 *matrices;
      float16 *items;
   };
   u32 count;
} Geometry_To_World_List;

Joint_List create_joint_list_from_ufbx_scene(const ufbx_scene *scene);
Geometry_To_World_List joint_matrices_using_scene(Joint_List *joints, const Animation *const animation, const double time) {
   static Geometry_To_World_List list = {0};
   ufbx_scene *orig_scene = animation->scene;
   ufbx_error error; // Optional, pass nullptr if you don't care about errors
   const ufbx_evaluate_opts eval_opts =  {0};

   // Forward time a bit to see the differentes
   ufbx_scene *scene = ufbx_evaluate_scene(orig_scene, orig_scene->anim, time, &eval_opts, &error);

   if (!scene || UFBX_ERROR_NONE != error.type) {
      char err_buf[512];
      ufbx_format_error(err_buf, size_of(err_buf), &error);
      trace_error("%s failed: %s %s", __func__, error.info, err_buf);
      return list;
   }


   if (!list.matrices) {
      list.matrices = malloc(size_of(list.matrices[0]) * scene->bones.count);
   }

   list.count = 0;
   auto joint_list = create_joint_list_from_ufbx_scene(scene);
   for (usz index = 0; index < joint_list.count; index += 1) {
      auto joint = &joint_list.joints[index];
      Matrix node_to_world     = joint_calculate_node_to_world_matrix(&joint_list, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);
      trace_debug("%d joint (%s):\n\tgeometry_to_node", index, joint_list.names[index]);
      list.matrices[list.count++] = MatrixToFloatV(geometry_to_world);
   }
   destroy_joint_list(&joint_list);
   // ufbx_free_scene(scene);
   return list;
}

// You are supposed to call this function once to update the GPU buffers and then forget it. The next time you call this function, the old matrices are invalidated.
// the middle of the frame just for this, so instead, we are going to reuse the same buffer over and over.
// The workflow is: auto matrices = joint_matrices_from_animation(); upload/update GPU buffers from matrices, and that's it. There is no need to free or allocate memory in 
Geometry_To_World_List joint_matrices_from_animation(Joint_List *joints, const Animation *const animation, const double time) {
   static Geometry_To_World_List list = {0};
   static isz capacity = 0;
   if (!list.matrices || joints->count > capacity) {
      capacity = joints->count*2;
      list.matrices = realloc(list.matrices, size_of(list.matrices[0]) * capacity); // @Leak
   }
   list.count = joints->count;

   assert(capacity >= joints->count);
   assert(list.count == joints->count);
   assert(animation->joints_animation.items);

   update_joints_transforms(joints, animation, time);
   for (usz index = 0; index < joints->count; index += 1) {
      auto joint = &joints->joints[index];
      Matrix node_to_world = joint_calculate_node_to_world_matrix(joints, joint);
      Matrix geometry_to_world = mul(node_to_world, joint->matrices.geometry_to_node);

      const bool debug = false;
      if (debug) {
         trace_info("%d time = %f joint (%s):\n\tgeometry_to_node", index, time, joints->names[index]);
         trace_struct(joint->matrices.geometry_to_node);
      }

      Matrix scene_hierarchy_matrix = MatrixCompose(joints->hierarchy_transform);
      geometry_to_world = mul(scene_hierarchy_matrix, geometry_to_world);
      list.matrices[index] = MatrixToFloatV(geometry_to_world);
   }
   return list;
}

Matrix raylib_matrix_from_ufbx_matrix(const ufbx_matrix ufbxmat);

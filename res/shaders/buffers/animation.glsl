layout(std430, binding = 12) buffer Animation_Matrices {
   mat4 geometry_to_model[];
};

struct Joint_Data {
   ivec4 joint_idxs;     // index into geometry_to_model
   vec4  joint_weights;  // \sum_over_(i=4){joint_weights[i] * bone_idxs[i]}
};

layout(std430, binding = 9) buffer Animation_Bones {
   Joint_Data joint_data[];
  // for (int i = 0; i < 4; ++i) {
  //       mat4 bone_transform = geometry_to_model[bone_idxs[i]];
  //       position += bone_weights[i] * (bone_transform * vec4(position, 1.0));
  //   }
  //
  //   gl_Position = uModelViewProjection * vec4(position, 1.0);
};

Varying {
   //
   // Insn't it weird that for uniform and storage buffers we need to do all that padding align to vec4 and here it just works?
   // I guess the compiler has context that can be used to pad when compiling similarly to C compilers.
   //

   // Is there a limit in varying/flat variables?
   vec3 position;
   vec3 normal;
   vec2 TextureCoordinate;
   vec3 tangent;
   vec3 bitangent;
   float tangent_w_sign;

   vec3 ViewSpaceNormal; vec3 ViewSpacePosition;

   vec3 TangentLightPosition; // this is for calculating shadown in tanget space during the relaxed_cone_relief_map, never got around to do it though
   vec3 TangentViewPosition;
   vec3 TangentFragPosition;

   flat uint render_state;
   flat uint material_index;
   flat uint has_tangents;
   flat vec4 color_tint;
   flat uint instance_rendering_mode;
   flat vec4 custom_1;
   flat vec4 custom_2;
}

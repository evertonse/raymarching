Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TextureCoordinate;
   vec3 Tangent;
   vec3 Bitangent;
   float tangent_w_sign;

   vec3 TangentLightPosition;
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

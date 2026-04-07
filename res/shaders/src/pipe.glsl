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

   flat uint material_index;
   flat uint has_tangents;
}

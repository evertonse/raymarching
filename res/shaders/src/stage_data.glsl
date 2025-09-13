layout (location = 0) Varying {
   vec3 Position;
   vec3 Normal;
   vec2 TextureCoordinate;
   vec3 Tangent;
   vec3 Bitangent;
   float tangent_w_sign;
};

layout (location = 8) out Flat {
   flat uint material_index;
   flat uint has_tangents;
};

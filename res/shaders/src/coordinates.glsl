
vec3 spherical_to_cartesian(float theta, float phi) {
   // float x = sin(theta) * cos(phi);
   // float y = sin(theta) * sin(phi);
   // float z = cos(theta);
   // return vec3(x, y, z);

   // float x = sin(phi) * cos(theta);
   // float y = cos(phi);
   // float z = sin(phi) * sin(theta);

   float x =  sin(phi) * cos(theta);
   float y = -sin(phi) * sin(theta);
   float z =  cos(phi);
   return vec3(x, y, z);
}

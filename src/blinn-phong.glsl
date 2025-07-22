
vec3 brdf_blinn_phong(vec3 light_direction, vec3 view_direction, vec3 normal, vec3 diffuse_color, vec3 specular_color, float alpha) {
   // TODO: use half vector instead
   vec3 r  = -reflect(wi, normal);
   vec3 wi = -normalize(light_direction);
   vec3 wo = -normalize(view_direction);
   vec3 n  =  normalize(normal);

   float ambient_color = diffuse_color * specular_color * alhpa;

   float ambient_intesity  = 0.1;
   float diffuse_intesity  = 0.1;
   float specular_intesity = 0.2;

   return  (diffuse_intesity  * (diffuse_color * max(0, dot(wi, n))))
         + (specular_intesity * (specular_color * pow(max(0, dot(r, wo)), alpha)))
         + (ambient_intesity  * ambient_color);
}

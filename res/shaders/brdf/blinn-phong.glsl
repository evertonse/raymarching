vec3 brdf_blinn_phong(
      in vec3 light_direction,     in vec3 view_direction, in vec3 normal,
      in vec3 diffuse_color,       in vec3 specular_color,
      in vec3 light_diffuse_color, in vec3 light_specular_color, in vec3 light_ambient_color,
      float specular_exponent
) {

   vec3 wi = normalize(light_direction);
   vec3 wo = normalize(view_direction);
   vec3 n  = normalize(normal);

   // vec3 ambient_color = diffuse_color * specular_color;
   vec3 ambient_color =  0.55160 * diffuse_color  + 0.082671 * specular_color;
   // vec3 ambient_color = vec3(0.212671*diffuse_color.r, 0.715160*diffuse_color.g, 0.072169*diffuse_color.b);


   // Table of materials and constants for ambient: http://devernay.free.fr/cours/opengl/materials.html
   float ambient_intesity  = 0.12 * (0.212671*ambient_color.r + 0.715160*ambient_color.g + 0.072169*ambient_color.b)/(0.1 + (0.212671*diffuse_color.r + 0.715160*diffuse_color.r + 0.072169*diffuse_color.r));
   float diffuse_intesity  = 0.45;
   float specular_intesity = 0.25;

   const bool use_half_vector = true;
   float specular_term = 0;
   if (use_half_vector) {
      vec3 h = normalize(wo + wi);
      specular_term = dot(n, h);
   } else {
      vec3 r = -reflect(wi, normal);
      specular_term = dot(r, wo);
   }

   vec3 diffuse  = light_diffuse_color  * diffuse_color  * max(0.0, dot(wi, n));
   vec3 specular = light_specular_color * specular_color * pow(max(0, specular_term), specular_exponent);
   vec3 ambient  = light_ambient_color  * ambient_color;
   // vec3 specular = vec3(0.8) * specular_color.x * pow(max(0, specular_term), specular_exponent);

   return  vec3(0.)
         + (diffuse_intesity  * diffuse)
         + (specular_intesity * specular)
         + (ambient_intesity  * ambient)
   ;
}


float n = 10; // 1 100
float ior = 1.5; // 1 2.5
bool include_Fresnel = false;
bool divide_by_NdotL = true;

vec3 BRDF( vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y )
{
    vec3 H = normalize(L+V);

    float NdotH = dot(N, H);
    float VdotH = dot(V, H);
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);

    float x = acos(NdotH) * n;
    float D = exp( -x*x);
    float G = (NdotV < NdotL) ?
        ((2*NdotV*NdotH < VdotH) ?
         2*NdotH / VdotH :
         1.0 / NdotV)
        :
        ((2*NdotL*NdotH < VdotH) ?
         2*NdotH*NdotL / (VdotH*NdotV) :
         1.0 / NdotV);

    // fresnel
    float c = VdotH;
    float g = sqrt(ior*ior + c*c - 1);
    float F = 0.5 * pow(g-c,2) / pow(g+c,2) * (1 + pow(c*(g+c)-1,2) / pow(c*(g-c)+1,2));

    float val = NdotH < 0 ? 0.0 : D * G * (include_Fresnel ? F : 1.0);

    if (divide_by_NdotL)
        val = val / dot(N,L);
    return vec3(val);
}

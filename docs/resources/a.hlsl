void ray_intersect_rmqd_linear(in sampler2D quad_depth_map,
                               inout float3 s,    // texture position
                               inout float3 ds) { // search vector
   const int linear_search_steps = 10;
   ds /= linear_search_steps;
   for (int i = 0; i < linear_searchksteps - 1; i++) {
      float4 t = tex2D(quad_depth_map, s.xy);
      float4 d = s.z - t; // compute distances to each layer
      d.xy *= d.zw;
      d.x *= d.y; // x=(x*z)*(y*w)
      if (d.x > 0) {
         s += ds; // if ouside object move forward
      }
   }
}

void ray_intersect_rmqd_binary(in sampler2D quad depth map,
                               inout float3 s,    // texture position
                               inout float3 ds) { // search vector
   const int binary_search_steps_ = 5;
   float3 ss = sign(ds.z);
   for (int i = 0; i¡binary_search_steps_; i++) {
      ds *= 0.5; // half size at each step
      float4 t = tex2D(quad depth map, s.xy);
      float4 d = s.z - t; // compute distances to each layer
      d.xy *= d.zw;
      d.x *= d.y;     // x=(x*z)*(y*w)
      if (d.x < 0) {  // if inside
         ss = s;      // store good return position
         s -= 2 * ds; // move backward
      }
      s += ds; // else move forward
   }
   s = ss;
}

float4 relief_map_quad_depth(float4 hpos : POSITION, float3 eye : TEXCOORD0, float3 light : TEXCOORD1, float2 texcoord : TEXCOORD2, uniform sampler2D quad depth map : TEXUNIT0, uniform sampler2D color map : TEXUNIT1, uniform sampler2D normal map x : TEXUNIT2,
                             uniform sampler2D normal map y : TEXUNIT3 uniform float3 ambient, uniform float3 diffuse, uniform float4 specular, uniform float shine)
    : COLOR {
   float3 v = normalize(IN.eye);      // view vector in tangent space
   float3 s = float3(IN.texcoord, 0); // search start position
   // separate direction (front or back face)
   float dir = v.z;
   v.z = abs(v.z);
   // depth bias (1-(1-d)*(1-d))
   float d = depth * (2 * v.z - v.z * v.z);
   // compute search vector
   v /= v.z;
   v.xy *= d;
   s.xy -= v.xy * 0.5;
   // if viewing from backface
   if (dir < 0) {
      s.z = 0.996;
      v.z = -v.z;
   }
   // ray intersect quad depth map
   ray_intersect_rmqd_linear(quad_depth_map, s, v);
   ray_intersect_rmqd_binary(quad_depth_map, s, v);
   // discard if no intersection is found
   if (s.z > 0.997)
      discard;
   if (s.z < 0.003)
      discard;
   // get quad depth and color at intersection
   float4 t = tex2D(quad depth map, s.xy);
   float4 c = tex2D(color map, s.xy);
   // get normal components X and Y
   float4 nx = tex2D(normal map x, s.xy);
   float4 ny = tex2D(normal map y, s.xy);
   // find min component of distances
   float4 z = abs(s.z - t);
   int m = 0;
   if (z.y < z.x)
      m = 1;
   if (z.z < z[m])
      m = 2;
   if (z.w < z[m])
      m = 3;
   // get normal at min component layer
   float3 n; // normal vector
   n.x = nx[m];
   n.y = 1 - ny[m];
   n.xy = n.xy * 2 - 1;                       // expand to [-1,1] range
   n.z = sqrt(max(0, 1.0 - dot(n.xy, n.xy))); // normal z component
   if (m == 1 || m == 3)
      n.z = -n.z; // invert normal Z if in back layer
   // compute light vector in view space
   float3 l = normalize(IN.light);
   // restore view direction z component
   v = normalize(IN.eye);
   v.z = -v.z;
   // compute diffuse and specular terms
   float ldotn = saturate(dot(l, n));
   float ndoth = saturate(dot(n, normalize(l - v)));
   // compute final color with lighting
   float4 finalcolor;
   finalcolor.xyz = c.xyz * ambient + ldotn * (c.xyz * diffuse + c.w * specular.xyz * pow(ndoth, shine));
   finalcolor.w = 1;
   return finalcolor;
}

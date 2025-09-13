
void gen_basis_tb(out vec3 vT, out vec3 vB, vec2 texST, vec3 position, vec3 nrmBaseNormal, mat4 g_mScrToView, mat4 g_mViewToWorld) {

   // Position in camera space
   vec4 v4ScrPos = vec4(position, 1.0);
   vec4 v4ViewPos = v4ScrPos * g_mScrToView;
   vec3 surfPosInView = v4ViewPos.xyz / v4ViewPos.w;

   // Actual world space position
   vec3 surfPosInWorld = (vec4(surfPosInView, 1.0) * g_mViewToWorld).xyz;

   // Relative world space
   vec3 relSurfPos = surfPosInView * mat3(g_mViewToWorld);

   vec3 dPdx = dFdx(relSurfPos);
   vec3 dPdy = dFdy(relSurfPos);

   vec3 sigmaX = dPdx - dot(dPdx, nrmBaseNormal) * nrmBaseNormal;
   vec3 sigmaY = dPdy - dot(dPdy, nrmBaseNormal) * nrmBaseNormal;

   float flip_sign = dot(dPdy, cross(nrmBaseNormal, dPdx)) < 0.0 ? -1.0 : 1.0;

   // Generate tangent and bitangent
   vec2 dSTdx = dFdx(texST);
   vec2 dSTdy = dFdy(texST);
   float det = dot(dSTdx, vec2(dSTdy.y, -dSTdy.x));
   float sign_det = det < 0.0 ? -1.0 : 1.0;

   // invC0 represents (dXds, dYds), but we don't divide
   // by the determinant. Instead, we scale by the sign.
   vec2 invC0 = sign_det * vec2(dSTdy.y, -dSTdx.y);
   vT = sigmaX * invC0.x + sigmaY * invC0.y;

   if (abs(det) > 0.0) {
      vT = normalize(vT);
   }
   vB = (sign_det * flip_sign) * cross(nrmBaseNormal, vT);
}

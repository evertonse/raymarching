
#ifndef TBN_HEADER
#define TBN_HEADER

#define dFdx dFdxFine
#define dFdy dFdyFine

//
// From: http://www.thetenthplanet.de/archives/1180
// Implementation of the same algorithm by others: https://github.com/leetvr/hotham/blob/6a372eb0adbb6f968e71827c3db171cfdf9e4635/hotham/src/shaders/pbr.frag#L41
//
mat3 cotangent_frame(vec3 position, vec3 normal, vec2 uv) {
   vec3 p = -position; // Negating becuase we look into +z instead in this renderer instead -z.
   vec3 N = normal;

   // get edge vectors of the pixel triangle
   vec3 dp1  = dFdx(p);
   vec3 dp2  = dFdy(p);
   vec2 duv1 = dFdx(uv);
   vec2 duv2 = dFdy(uv);

   // solve the linear system
   vec3 dp2perp = cross(dp2, N);
   vec3 dp1perp = cross(N, dp1);
   vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
   vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
   // construct a scale-invariant frame
   float invmax = inversesqrt(max(dot(T, T), dot(B, B)));

#if 1 // Original: this invmax normalization is better for skew transfomations
      //           if no normalization is performed then large scale objects would perceive less normal perturbation because T and B scale inversely proporcional to it.
   return mat3(T * invmax, B * invmax, N);
#else
   return mat3(normalize(T), normalize(B), normalize(N));
#endif
}

//
//
// Based upon Mikkelsen's reference tangent basis construction from Surface Gradient Based Bump Mapping Framework
// More specifically bump demo: https://www.dropbox.com/scl/fi/fmmq4ufqk1upzlkkv2uqw/bumpdemo_src.zip?dl=0&e=1&rlkey=3bhqg3mxpfn4py1w3u26emqnz
// Also available here: https://mmikkelsen3d.blogspot.com/2012/02/parallaxpoc-mapping-and-no-tangent.html
//
// Theres even a way to avoid not need st/uv parametization see: Bump Mapping Unparametrized Surfaces on the GPU)
// https://www.dropbox.com/scl/fi/194bproxfa8rjgwgpfklo/mm_sfgrad_bump.pdf?rlkey=61gewtygbqusre9zqfc0ml64z&e=1&dl=0
//
// Produces an orthonormal TBN matrix from screen-space derivatives only.
// Supposed to match MikkTSpace convention.
//
//
mat3 tangent_frame_mikkelsen(in const vec3 position, in const vec3 normal, in const vec2 uv) {
   const vec3 N = normalize(normal);

   // Screen-space derivatives of position
   const vec3 dpdx = dFdx(position);
   const vec3 dpdy = dFdy(position);

   // Project onto tangent plane remove the normal component from each derivative.
   // Because the surface is curved, raw dpdx/dpdy point partly along the surface and partly along the normal.
   // Subtracting the normal projection leaves only the tangential component.
   const vec3 sigma_x = dpdx - dot(dpdx, N) * N;
   const vec3 sigma_y = dpdy - dot(dpdy, N) * N;

   // Screen-space derivatives of uv coordinates
   const vec2 duvdx = dFdx(uv);
   const vec2 duvdy = dFdy(uv);

   // Sign of the uv Jacobian determinant.
   // det > 0: uv space has the same orientation as screen space (standard)
   // det < 0: uv space is mirrored
   // J = [ du/dx  du/dy ]
   //     [ dv/dx  dv/dy ]
   // The author said to use sign only and division by det is unnecessary since we normalize T.
   const float sign_det = (duvdx.x * duvdy.y - duvdx.y * duvdy.x) < 0.0 ? -1.0 : 1.0;

   // inv_c0 represents (dxdu, dydu) how much position changes per unit of u
   // which direction u increases on the surface.
   const vec2 inv_c0 = sign_det * vec2(duvdy.y, -duvdx.y);

   // Tangent points to the direction of increasing u on the surface
   // Weighted combination of projected position derivatives using uv inverse
   const vec3 T = normalize(sigma_x * inv_c0.x + sigma_y * inv_c0.y);

   // Bitangent is just cross(normal, tangent)
   //    sign_det    corrects for mirrored uv space
   //    orientation corrects for handedness of the actual derivative frame
   //                Tests whether sigma_y is on the correct side of cross(n, sigma_x)
   const float orientation = dot(sigma_y, cross(N, sigma_x)) < 0.0 ? -1.0 : 1.0;
   const vec3 B = sign_det * orientation * cross(N, T);

   return mat3(T, B, N);
}

#endif // TBN_HEADER

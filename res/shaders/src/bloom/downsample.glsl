// #pragma compute
#version 460 core
layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D src_texture;
layout(rgba32f, binding = 1) writeonly uniform image2D dst_image;

#include "./bloom.h"

//
// Taken from https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/
//

float brightness(vec3 c) { return max(c.r, max(c.g, c.b)); };

vec3 pow3(vec3 v, float p) {
   return vec3(pow(v.x, p), pow(v.y, p), pow(v.z, p));
}

float luminance(vec3 linear_rgb) { return dot(linear_rgb, vec3(0.2126729, 0.7151522, 0.0721750)); }

// Intents to eliminate NaN propagation. max between NaN and 0 is defined to be 0.
vec3 apply_clamp_max(vec3 c) {
   // c = mix(vec3(0.0), c, vec3(equal(c, c))); // replace NaN with 0 (NaN != NaN)
   if (any(isnan(c)) || any(isinf(c))) {
      return vec3(0.0);
   }
   return min(c, vec3(bloom.prefilter_clamp_max));
}


// https://www.desmos.com/calculator/rauntuxt9o
vec3 apply_threshold(vec3 c) {
   float b = brightness(c);

   const float t = bloom.prefilter_threshold;
   const float k = bloom.prefilter_knee + 1e-5f;

   float softness   = clamp(b - t + k, 0.0, 2.0 *k);
         softness   = (softness * softness) / (4.0 * k + 1e-4);

   float multiplier  = max(b - t, softness);
         multiplier /= max(b, 1e-4);

   return c * multiplier;
}


vec3 sample_box(vec2 uv, vec2 texel) {
   vec3 c = vec3(0.0);

   c += texture(src_texture, uv + texel * vec2(-1, -1)).rgb;
   c += texture(src_texture, uv + texel * vec2(0,  -1)).rgb;
   c += texture(src_texture, uv + texel * vec2(1,  -1)).rgb;

   c += texture(src_texture, uv + texel * vec2(-1, 0)).rgb;
   c += texture(src_texture, uv).rgb;
   c += texture(src_texture, uv + texel * vec2(1, 0)).rgb;

   c += texture(src_texture, uv + texel * vec2(-1, 1)).rgb;
   c += texture(src_texture, uv + texel * vec2( 0, 1)).rgb;
   c += texture(src_texture, uv + texel * vec2( 1, 1)).rgb;

   return c / 9.0;
}


float karis_weight(vec3 color) {
   // TODO: Confirm: Maybe this 'luminance' function is working under sRGB
   return 1.0 / (1.0 + luminance(color));
   // NOTE: Could we bright ness instead
   //       https://github.com/github-linguist/linguist/blob/e535c9adf5306132e9df0b75ffe1ce2679873fe8/samples/HLSL/bloom.cginc#L46
   // return 1.0 / (1.0 + brightness(color));
}


vec3 partial_average(vec3 c0, vec3 c1, vec3 c2, vec3 c3, float w0, float w1, float w2, float w3) {
   return (c0*w0 + c1*w1 + c2*w2 + c3*w3) / (w0 + w1 + w2 + w3);
}


vec3 sample_texture(sampler2D in_texture, vec2 uv) {
   vec4 c = texture(in_texture, uv);

   // When alpha is enabled, regions with zero alpha should not generate any bloom / glow. Therefore we pre-multipy the color with the alpha channel here and the rest
   // of the computations remain float3. Still, when bloom is applied to the final image, bloom will still be spread on regions with zero alpha (see UberPost.compute)
   // Note that the alpha channel in the color target could be greater than 1.0 or NaN or negative. The alpha here is opacity so we clamp it to handle an unexpected input.
   c.rgb *= clamp(c.a, 0.0, 1.0);
   return c.rgb;
}

// Unity HQ 13 tap pattern (URP FragPrefilter _BLOOM_HQ).
// Half-pixel inner offsets exploit bilinear for free extra coverage.
vec3 sample_unity(vec2 uv, vec2 texel) {
   const float scale = 2.0;
   vec3 A = texture(src_texture, uv + texel * (vec2(-1.0, -1.0) * scale)).rgb;
   vec3 B = texture(src_texture, uv + texel * (vec2( 0.0, -1.0) * scale)).rgb;
   vec3 C = texture(src_texture, uv + texel * (vec2( 1.0, -1.0) * scale)).rgb;
   vec3 D = texture(src_texture, uv + texel * (vec2(-0.5, -0.5) * scale)).rgb;
   vec3 E = texture(src_texture, uv + texel * (vec2( 0.5, -0.5) * scale)).rgb;
   vec3 F = texture(src_texture, uv + texel * (vec2(-1.0,  0.0) * scale)).rgb;
   vec3 G = texture(src_texture, uv         * (vec2(0)          * scale)).rgb;
   vec3 H = texture(src_texture, uv + texel * (vec2( 1.0,  0.0) * scale)).rgb;
   vec3 I = texture(src_texture, uv + texel * (vec2(-0.5,  0.5) * scale)).rgb;
   vec3 J = texture(src_texture, uv + texel * (vec2( 0.5,  0.5) * scale)).rgb;
   vec3 K = texture(src_texture, uv + texel * (vec2(-1.0,  1.0) * scale)).rgb;
   vec3 L = texture(src_texture, uv + texel * (vec2( 0.0,  1.0) * scale)).rgb;
   vec3 M = texture(src_texture, uv + texel * (vec2( 1.0,  1.0) * scale)).rgb;
   
   vec2 div   = (1.0 / 4.0) * vec2(0.5, 0.125);
   vec3 color = (D + E + I + J) * div.x;
   color     += (A + B + G + F) * div.y;
   color     += (B + C + H + G) * div.y;
   color     += (F + G + L + K) * div.y;
   color     += (G + H + M + L) * div.y;
   return color;
}


vec3 sample_13_bilinear(vec2 uv, vec2 texel) {
   vec3 color = vec3(0.0);
   //
   // Take 13 samples around current texel:
   //
   // a - b - c
   // - j - k -
   // d - e - f
   // - l - m -
   // g - h - i
   //
   //  ('e' is the current texel)
   //
   vec3 a = sample_texture(src_texture, uv + texel * vec2(-2.0, 2.0)).rgb;
   vec3 b = sample_texture(src_texture, uv + texel * vec2( 0.0, 2.0)).rgb;
   vec3 c = sample_texture(src_texture, uv + texel * vec2( 2.0, 2.0)).rgb;


   vec3 d = sample_texture(src_texture, uv + texel * vec2(-2.0, 0.0)).rgb;
   vec3 e = sample_texture(src_texture, uv + texel * vec2( 0.0, 0.0)).rgb; // center
   vec3 f = sample_texture(src_texture, uv + texel * vec2( 2.0, 0.0)).rgb;

   vec3 g = sample_texture(src_texture, uv + texel * vec2(-2.0, -2.0)).rgb;
   vec3 h = sample_texture(src_texture, uv + texel * vec2( 0.0, -2.0)).rgb;
   vec3 i = sample_texture(src_texture, uv + texel * vec2( 2.0, -2.0)).rgb;


   vec3 j = sample_texture(src_texture, uv + texel * vec2(-1.0,  1.0)).rgb;
   vec3 k = sample_texture(src_texture, uv + texel * vec2( 1.0,  1.0)).rgb;
   vec3 l = sample_texture(src_texture, uv + texel * vec2(-1.0, -1.0)).rgb;
   vec3 m = sample_texture(src_texture, uv + texel * vec2( 1.0, -1.0)).rgb;

   //
   // Unity's prefilter:
   //    https://github.com/Unity-Technologies/Graphics/blob/3e99c0d1e996f856b618cb33e390ee6c0e4867ea/Packages/com.unity.render-pipelines.universal/Shaders/PostProcessing/Bloom.shader#L55
   //
   if (true && 0 == bloom.mip_level) {
      // Karis weighted groups to kill fireflies from slide 167
      // Following unity's instead of opengl tutorial we have
      // One karis weight per INDIVIDUAL sample, on raw unscaled values
      float wj = karis_weight(j),  wk = karis_weight(k);
      float wl = karis_weight(l),  wm = karis_weight(m);
      float wa = karis_weight(a),  wb = karis_weight(b),  wc = karis_weight(c);
      float wd = karis_weight(d),  we = karis_weight(e),  wf = karis_weight(f);
      float wg = karis_weight(g),  wh = karis_weight(h),  wi = karis_weight(i);
      // normalized weighted average per group (partial_average divides by weight sum)
      // spatial weight applied AFTER normalization
      color  = partial_average(j, k, l, m,  wj, wk, wl, wm) * 0.500;
      color += partial_average(a, b, d, e,  wa, wb, wd, we) * 0.125;
      color += partial_average(b, c, e, f,  wb, wc, we, wf) * 0.125;
      color += partial_average(d, e, g, h,  wd, we, wg, wh) * 0.125;
      color += partial_average(e, f, h, i,  we, wf, wh, wi) * 0.125;
   } else {
      color =               e  * 0.125   ;
      color += (a + c + g + i) * 0.03125 ;
      color += (b + d + f + h) * 0.0625  ;
      color += (j + k + l + m) * 0.125   ;
   }

   return color;
}


void main() {
   ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
   ivec2 dst_size = imageSize(dst_image);
   if (dst_coord.x >= dst_size.x || dst_coord.y >= dst_size.y) {
      return;
   }
   unpack_constants(push_constants);

   vec2 uv = (vec2(dst_coord) + 0.5) / vec2(dst_size);
   vec2 texel = 1.0 / vec2(bloom.src_width, bloom.src_height);

   vec3 color;
   if (bloom.mip_level == 0) {
      // Prefilter path
      // HQ kernel then clamp, then threshold.
      // color = sample_box(uv, texel);
      color = sample_13_bilinear(uv, texel);
      // color = sample_unity(uv, texel);
      color = apply_clamp_max(color);
      color = apply_threshold(color);
   } else {
      // Normal downsample path
      // color = sample_unity(uv, texel);
      color = sample_13_bilinear(uv, texel);
      // color = sample_box(uv, texel);
   }

   imageStore(dst_image, dst_coord, vec4(color, 1.0));
};

#ifndef COMMON_HEADER
#define COMMON_HEADER

// I refuse to call this mix smh.
#ifndef lerp
#   define lerp mix
#endif

#define BT709_OETF

#if defined(PURE_GAMMA)
vec3 to_linear(vec3 sRGB) { return pow(sRGB, vec3(2.2)); }

vec3 from_linear(vec3 linearRGB) { return pow(linearRGB, vec3(1.0 / 2.2)); }

#elif defined(BT709_OETF)
vec3 to_linear(vec3 sRGB) {
   bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
   vec3 higher = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
   vec3 lower = sRGB / vec3(12.92);

   return mix(higher, lower, cutoff);
}

vec3 from_linear(vec3 linearRGB) {
   bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
   vec3 higher = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
   vec3 lower = linearRGB * vec3(12.92);

   return mix(higher, lower, cutoff);
}

#endif

vec3 saturate(vec3 v) { return clamp(v, 0.0, 1.0); }

// Taken from https://observablehq.com/@rreusser/bicubic-texture-interpolation-using-linear-filtering
// Also present in https://stackoverflow.com/questions/13501081/efficient-bicubic-filtering-code-in-glsl#42179924

vec4 cubic(float v) {
   vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;
   vec4 s = n * n * n;
   float x = s.x;
   float y = s.y - 4.0 * s.x;
   float z = s.z - 4.0 * s.y + 6.0 * s.x;
   float w = 6.0 - x - y - z;
   return vec4(x, y, z, w) * (1.0 / 6.0);
}

vec4 sample_texture_bicubic(sampler2D sampler, vec2 uv) {
   vec2 texture_resolution = textureSize(sampler, 0);
   vec2 inverse_texture_resolution = 1.0 / texture_resolution;
   uv = uv * texture_resolution - 0.5;
   vec2 fxy = fract(uv);
   uv -= fxy;

   vec4 xcubic = cubic(fxy.x);
   vec4 ycubic = cubic(fxy.y);

   vec4 c = uv.xxyy + vec2(-0.5, 1.5).xyxy;

   vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);
   vec4 offset = c + vec4(xcubic.yw, ycubic.yw) / s;

   offset *= inverse_texture_resolution.xxyy;

   vec4 sample0 = texture2D(sampler, offset.xz);
   vec4 sample1 = texture2D(sampler, offset.yz);
   vec4 sample2 = texture2D(sampler, offset.xw);
   vec4 sample3 = texture2D(sampler, offset.yw);

   float sx = s.x / (s.x + s.y);
   float sy = s.z / (s.z + s.w);

   return lerp(lerp(sample3, sample2, sx), lerp(sample1, sample0, sx), sy);
}
#endif //COMMON_HEADER

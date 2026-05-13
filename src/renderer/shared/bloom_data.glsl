#define SIZE_OF_BLOOM_DATA (2*(4 * 4))
struct Bloom_Data {
   float strength, filter_radius,  mip_level, prefilter_clamp_max;
   float prefilter_threshold  /* 0.0 (full PBR) or 1.0 (classic)*/, prefilter_knee /* 0.5 */; int src_width, src_height;
};

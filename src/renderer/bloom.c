// #define BLOOM_MIP_COUNT 5
#define BLOOM_MIP_COUNT 7 // for 1080p

#include "./shared/bloom_data.glsl"
typedef struct Bloom_Data Bloom_Data;

static_assert(size_of(Bloom_Data) == 32);

typedef struct {
   bool        loaded;
   Shader      downsample_shader;
   Shader      upsample_shader;
   Shader      composite_shader;
   Texture     previous_bloom;
   Texture     mips[BLOOM_MIP_COUNT];
   Framebuffer output_framebuffer;
   int         width;
   int         height;
} Bloom_State;


Framebuffer apply_bloom(Framebuffer src_fb) {
   static Countdown shader_countdown_to_reload = {0};
   static Bloom_State s = {0};
   static float filter_radius = 0.005f;
   static float bloom_strength = 0.055f;

   if (is_button_pressed(BUTTON_F2)) {
      bloom_strength += 0.02f;
      if (bloom_strength > 1.f)
         bloom_strength = 0.f;
      trace_info("%s: strength = %f", __func__, bloom_strength);
   }

   if (is_button_pressed(BUTTON_F3)) {
      bloom_strength -= 0.02f;
      if (bloom_strength < 0.f)
         bloom_strength = 1.f;
      trace_info("%s: strength = %f", __func__, bloom_strength);
   }

   if (is_button_pressed(BUTTON_F4)) {
      filter_radius += 0.01f;
      if (filter_radius > 1.f)
         filter_radius = 0.f;
      trace_info("%s: filter_radius = %f", __func__, filter_radius);
   }

   if (is_button_pressed(BUTTON_F5)) {
      filter_radius = 0.005f;
      trace_info("%s: filter_radius = %f", __func__, filter_radius);
   }

   if (!is_valid_framebuffer(src_fb)) {
      trace_error("%s: invalid framebuffer", __func__);
      return src_fb;
   }

   if (texture_multisamples(src_fb.color) > 1) {
      trace_warn("%s: framebuffer is multisampled. Resolve it first.", __func__);
      return src_fb;
   }

   const int w = src_fb.color.width;
   const int h = src_fb.color.height;

   bool need_init = !s.loaded || s.width != w || s.height != h;

   // Init or Resize
   if (need_init) {
      // Free memory and zero init
      {
         for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
            destroy_texture(&s.mips[i]);
         }
         destroy_texture(&s.previous_bloom);

         destroy_framebuffer(&s.output_framebuffer);
         memset(&s, 0, size_of(s));
      }

      shader_countdown_to_reload = create_countdown(0.39, true);

      // Initlizated and allocated gpu memory for textures
      s.width  = w;
      s.height = h;


      // Build shaders
      s.downsample_shader = create_shader("res/shaders/src/bloom/downsample.glsl", COMPUTE_SHADER);
      s.upsample_shader   = create_shader("res/shaders/src/bloom/upsample.glsl",   COMPUTE_SHADER);
      s.composite_shader  = create_shader("res/shaders/src/bloom/composite.glsl",  COMPUTE_SHADER);
      if (!is_valid_shader(s.downsample_shader) || !is_valid_shader(s.upsample_shader) || !is_valid_shader(s.composite_shader)) {
         trace_error("%s: Failed to create shaders. Returning old framebuffer.", __func__);
         s.loaded = false;
         return src_fb;
      }

      // Build mips
      int mips_width  = s.width;
      int mips_height = s.height;

      for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
         mips_width  = max(1, mips_width / 2);
         mips_height = max(1, mips_height / 2);
         s.mips[i] = create_texture(mips_width, mips_height, nullptr, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, TEXTURE_FILTER_BILINEAR, TEXTURE_WRAP_CLAMP_EDGE);
      }


      // Output framebuffer
      Texture out_texture = create_texture(w, h, nullptr, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, TEXTURE_FILTER_NONE, TEXTURE_WRAP_CLAMP_EDGE);
      s.output_framebuffer = create_framebuffer_from_texture(out_texture);

      if (!is_valid_framebuffer(s.output_framebuffer)) {
         trace_error("%s: Failed to create bloom framebuffer. Returning old framebuffer.", __func__);
         // NOTE: We're gonna clean gpu memory next time this function is called.
         s.loaded = false;
         return src_fb;
      }

      s.loaded = true;
      trace_info("Bloom initialized (%dx%d)", w, h);
   }

   update_countdown(&shader_countdown_to_reload, {
      reload_shader_if_needed(&s.downsample_shader);
      reload_shader_if_needed(&s.upsample_shader);
      reload_shader_if_needed(&s.composite_shader);
   });

   // Pass 1: downsample
   Texture src_current = src_fb.color;
   int src_width  = w;
   int src_height = h;

   Bloom_Data bloom_data = {

      // Prefilter params shader reads them only on mip 0
      .prefilter_clamp_max = 500.0f,
      .prefilter_threshold = 1.f,
      .prefilter_knee      = 0.5f,
      .strength            = bloom_strength ,
      .filter_radius       = filter_radius,
   };


   bind_shader(s.downsample_shader);
   upload_push_constants(&bloom_data, size_of(bloom_data));

   for (int i = 0; i < BLOOM_MIP_COUNT; i++) {
      Texture mip = s.mips[i];
      bind_texture(src_current, 0);
      bind_texture_as_image(mip, 1, TEXTURE_ACCESS_WRITE);

      bloom_data.mip_level  = i;
      bloom_data.src_width  = src_width;
      bloom_data.src_height = src_height;
      upload_push_constants(&bloom_data, size_of(bloom_data));

      dispatch_compute_shader_2d(s.downsample_shader, mip.width, mip.height);

      // Ensure all writes to the image are complete
      shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);

      src_current = mip;
      src_width   = mip.width;
      src_height  = mip.height;
   }

   // Pass 2: upsample
   bind_shader(s.upsample_shader);
   upload_push_constants(&bloom_data, size_of(bloom_data));
   for (int i = BLOOM_MIP_COUNT - 1; i > 0; i--) {
      auto *src_mip = &s.mips[i];
      auto *dst_mip = &s.mips[i - 1];

      bind_texture(*src_mip, 0);
      bind_texture_as_image(*dst_mip, 1, TEXTURE_ACCESS_READ | TEXTURE_ACCESS_WRITE);

      dispatch_compute_shader_2d(s.upsample_shader, dst_mip->width, dst_mip->height);

      shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);
   }

   // Pass 3: composite
   bind_shader(s.composite_shader);
   upload_push_constants(&bloom_data, size_of(bloom_data));

   if (!is_valid_texture(s.previous_bloom) || s.previous_bloom.width != w || s.previous_bloom.height != h) {
      destroy_texture(&s.previous_bloom);
      s.previous_bloom = create_texture(s.mips[0].width, s.mips[0].height, nullptr, TEXTURE_FORMAT_RGBA32F, TEXTURE_TYPE_2D, TEXTURE_FILTER_BILINEAR, TEXTURE_WRAP_CLAMP_EDGE);
      copy_texture(s.previous_bloom, s.mips[0]); // initialize on first frame
   }

   bind_texture(src_fb.color, 0);
   bind_texture(s.mips[0], 1);
   bind_texture(s.previous_bloom, 2);
   bind_texture_as_image(s.output_framebuffer.color, 2, TEXTURE_ACCESS_WRITE);


   dispatch_compute_shader_2d(s.composite_shader, w, h);

   shader_memory_barrier(SHADER_BARRIER_IMAGE_ACCESS | SHADER_BARRIER_TEXTURE_FETCH);

   copy_texture(s.previous_bloom, s.mips[0]);

   return s.output_framebuffer;
}

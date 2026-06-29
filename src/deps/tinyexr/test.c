// gcc test.c -I src -Iinclude && ./a.out

#include <stdint.h>

#include "exr.h"
#include "exr_implementation.c"


float *exr_load_image(const char *filename, int *w, int *h, int *channels) {
   exr_image img = {0};
   if (exr_load_from_file(filename, NULL, &img) != EXR_SUCCESS) {
      return NULL;
   }

   //
   // TODO: error reporting.
   //

   float *out = NULL;
   if (1 != img.num_parts) {
      goto fail;
   }

   exr_part *part = &img.parts[0];
   if (part->is_deep) {
      goto fail;
   }

   int ch     = part->header.num_channels;
   int width  = part->width;
   int height = part->height;
   size_t pixels_count = width * height;
   out = malloc(pixels_count * ch * sizeof(float));

   for (int c = 0; c < ch; c++) {
      exr_pixel_type pt = part->header.channels[c].pixel_type;
      void *src = part->images[c];
      for (size_t i = 0; i < pixels_count; i++) {
         float val;

         if (pt == EXR_PIXEL_HALF) exr_half_to_float(((uint16_t *)src), &val, 1);
         else if (pt == EXR_PIXEL_UINT) val = (float)((uint32_t *)src)[i];
         else val = ((float *)src)[i];

         out[i * ch + c] = val;
      }
   }

   *w = width;
   *h = height;
   *channels = ch;
fail:
   exr_image_free(&img);
   return out;
}

int exr_write_image(const char *filename, const float *pixels, int width, int height, int channels) {
   // The EXR format mandates alphabetical channel order when writting and loading
   static const char *channel_names[] = {
      "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "L"
   }; // Usually would mean this {"R", "G", "B", "A"}; but that would invert some orders when writing

   constexpr int channels_max = sizeof(channel_names) / sizeof(channel_names[0]);
   if (channels > channels_max) {
      printf("%s: channels %d exceeds max %d aborting.\n",__func__, channels, channels_max);
      return 0;
   }

   size_t pixels_count = width * height;
   float *planes = malloc(pixels_count * channels * sizeof(float));

   // deinterleave into planar layout
   for (int c = 0; c < channels; c++) {
      for (size_t i = 0; i < pixels_count; i++) {
         planes[c * pixels_count + i] = pixels[i * channels + c];
      }
   }

   exr_channel chan[channels_max] = {0};
   void *image_ptr[channels_max];
   for (int c = 0; c < channels; c++) {
      strcpy(chan[c].name, channel_names[c]);
      chan[c].pixel_type = EXR_PIXEL_FLOAT;
      chan[c].x_sampling = 1;
      chan[c].y_sampling = 1;
      image_ptr[c] = planes + c * pixels_count;
   }

   exr_part part = {
      .header            = {
         .part_type      = EXR_PART_SCANLINE,
         .compression    = EXR_COMPRESSION_ZIP,
         .data_window    = {0, 0, width - 1, height - 1},
         .display_window = {0, 0, width - 1, height - 1},
         .num_channels   = channels,
         .channels       = chan,
      },
      .width  = width,
      .height = height,
      .images = (void **)image_ptr,
   };
   exr_image img = {.num_parts = 1, .parts = &part};

   int ok = exr_save_to_file(filename, &img, EXR_COMPRESSION_ZIP) == EXR_SUCCESS;
   free(planes);
   return ok;
}


int main(void) {
   const char *our_file = "pica.exr";
   float data[] = {
      0.69420, 420.0, 3, 4, 5 ,6, 7,  8, 9, 10,
      1 + 0.69420, 1 + 420.0, 1 + 3, 1 + 4, 1 + 5 ,1 + 6, 1 + 7,  1 + 8, 1 + 9, 1 + 10,
   };
   int ok = exr_write_image(our_file, data, 1, 2, 10);

   int w, h; int channels;
   float *data2 = exr_load_image(our_file, &w, &h, &channels);
   printf("w = %d, h = %d channels = %d\n", w, h, channels);
   for (int j = 0; j < h; j++) {
      for (int i = 0; i < w; i++) {
         for (int ch = 0; ch < channels; ch++) {
            printf("%f ", data2[(j * w + i) * channels + ch]);
         }
         printf("| ");
      }
      printf("\n");
   }
   printf("\n");
}


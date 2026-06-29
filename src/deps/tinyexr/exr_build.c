//
// File added by the vendoring to make compiling easier
//
// Compile as: gcc -std=c11 your_file.c -I include/
// Use like this:
//      #include "exr.h"
//      #include "exr_build.c"
// exr.h can be included everywhere whereas exr_build.c only in one transaltion unit
//
// To learn about exr: https://openexr.com/en/latest/TechnicalIntroduction.html
//

#include "./src/exr_attr.c"
#include "./src/exr_b44.c"
#include "./src/exr_codec.c"
#include "./src/exr_color.c"
#include "./src/exr_convert.c"
#include "./src/exr_core.c"
#include "./src/exr_cpu.c"
#include "./src/exr_deep.c"
#include "./src/exr_deflate.c"
#include "./src/exr_fpnge.c"
#include "./src/exr_freestanding.c"
#include "./src/exr_gpu_cuda.c"
#include "./src/exr_half.c"
#include "./src/exr_jph.c"
#include "./src/exr_jph_simd.c"
#include "./src/exr_jph_simd_neon.c"
#include "./src/exr_libdeflate.c"
#include "./src/exr_mip.c"
#include "./src/exr_piz.c"
#include "./src/exr_pxr24.c"
#include "./src/exr_reader.c"
#include "./src/exr_resize.c"
#include "./src/exr_rle.c"
#include "./src/exr_simd_neon.c"
#include "./src/exr_simd_x86.c"
#include "./src/exr_spectral.c"
#include "./src/exr_stdio.c"
#include "./src/exr_thread.c"
#include "./src/exr_tonemap.c"
#include "./src/exr_util_simd_neon.c"
#include "./src/exr_util_simd_x86.c"
#include "./src/exr_vk_vulkan.c"
#include "./src/exr_writer.c"
#include "./src/exr_zip.c"

// Included here to avoid having an extra #include "tinyexr_zstd.h"
#ifndef EXR_NO_ZSTD
#include "./deps/zstd/tinyexr_zstd.h"
#include "./deps/zstd/tinyexr_zstd.c"


#define EXR_ZSTD_LEVEL 3

exr_result exr_zstd_decompress(const exr_allocator *a, const uint8_t *src,
                               size_t src_size, uint8_t *dst, size_t dst_size) {
    size_t n;
    (void)a;

    n = tinyexr_zstd_decompress(dst, dst_size, src, src_size);
    if (tinyexr_zstd_is_error(n) || n != dst_size) return EXR_ERROR_CORRUPT;
    return EXR_SUCCESS;
}

exr_result exr_zstd_compress(const exr_allocator *a, const uint8_t *src,
                             size_t n, uint8_t **out_data, size_t *out_size) {
    size_t bound, clen;
    uint8_t *comp;

    *out_data = NULL;
    *out_size = 0;
    if (n == 0) {
        *out_data = (uint8_t *)exr_malloc(a, 1);
        if (!*out_data) return EXR_ERROR_OUT_OF_MEMORY;
        return EXR_SUCCESS;
    }

    bound = tinyexr_zstd_compress_bound(n);
    if (tinyexr_zstd_is_error(bound) || bound == 0) return EXR_ERROR_CORRUPT;

    comp = (uint8_t *)exr_malloc(a, bound);
    if (!comp) return EXR_ERROR_OUT_OF_MEMORY;

    clen = tinyexr_zstd_compress(comp, bound, src, n, EXR_ZSTD_LEVEL);
    if (tinyexr_zstd_is_error(clen) || clen >= n) {
        exr_free(a, comp);
        *out_data = (uint8_t *)exr_malloc(a, n);
        if (!*out_data) return EXR_ERROR_OUT_OF_MEMORY;
        memcpy(*out_data, src, n);
        *out_size = n;
        return EXR_SUCCESS;
    }

    *out_data = comp;
    *out_size = clen;
    return EXR_SUCCESS;
}
#endif


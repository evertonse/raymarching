#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define MAX_TEXT_CACHE 512

typedef struct {
    GLuint texture;
    stbtt_bakedchar chars[96]; // ASCII 32..126
    float font_size;
} FontAtlas;

static FontAtlas default_font;
static unsigned char font_bitmap[512 * 512]; // ~256 KB, large enough
static GLuint default_font_texture = 0;
static stbtt_bakedchar default_chars[96]; // ASCII 32..126

void init_default_font(const char *ttf_path, float font_size) {
    if (default_font_texture) return;

    FILE *f = fopen(ttf_path, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    int size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *ttf_buffer = malloc(size);
    fread(ttf_buffer, 1, size, f);
    fclose(f);

    stbtt_BakeFontBitmap(ttf_buffer, 0, font_size, font_bitmap, 512, 512, 32, 96, default_chars);
    glGenTextures(1, &default_font_texture);
    glBindTexture(GL_TEXTURE_2D, default_font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, font_bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    default_font.texture = default_font_texture;
    memcpy(default_font.chars, default_chars, sizeof(default_chars));
    default_font.font_size = font_size;

    free(ttf_buffer);
}

void draw_text_simple(const char *text, int pos_x, int pos_y, int fontSize, Color color) {
    init_default_font("res/fonts/Alegreya-Regular.ttf", fontSize); // Make sure font is loaded

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, default_font.texture);

    float x = (float)pos_x;
    float y = (float)pos_y;
    for (const char *c = text; *c; ++c) {
        if (*c < 32 || *c >= 128) continue;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(default_font.chars, 512, 512, *c - 32, &x, &y, &q, 1);

        draw_quad_textured(q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0,
                           q.s0, q.t0, q.s1, q.t1, color);
    }
}


// THIS whole file is gonna get deleted once we have sdf text rendering ok?


#define MAX_FONTS 16
#define MAX_FONT_SIZES 8
#define ATLAS_SIZE 4048
#define MAX_GLYPHS 256
#define MAX_TEXT_LENGTH 1024
#define OVERSAMPLE_X 2
#define OVERSAMPLE_Y 2

// For text positioning
typedef struct {
   float x, y, width, height;
} Rectangle_Float;

// Glyph info for rect packing
typedef struct {
   int codepoint;
   float advance;
   int x0, y0, x1, y1; // bitmap bounds
   float xoff, yoff;   // offset when drawing
   int bitmap_index;   // index into packed bitmap
} GlyphInfo;

// Cached font data
typedef struct {
   stbtt_fontinfo font_info;
   unsigned char *font_data;
   bool loaded;
   char path[256];
} FontData;

// Font atlas with rect packing
typedef struct {
   GLuint texture;
   GlyphInfo glyphs[MAX_GLYPHS];
   float font_size;
   float scale;
   int ascent, descent, line_gap;
   bool packed;
} FontAtlas;

// Global text renderer state
static struct {
   FontData fonts[MAX_FONTS];
   FontAtlas atlases[MAX_FONTS][MAX_FONT_SIZES];
   int current_font_count;

   GLuint shader_program;
   GLint u_texture, u_projection, u_text_color, u_char_data, u_char_count;
   GLuint dummy_vao;

   GLuint char_data_buffer;
   GLuint char_data_texture;

   bool initialized;
} g_text_renderer = {0};

static const char *vertex_shader_source = "#version 330 core\n"
                                          "uniform mat4 projection;\n"
                                          "uniform samplerBuffer char_data;\n"
                                          "uniform int char_count;\n"
                                          "\n"
                                          "out vec2 uv;\n"
                                          "\n"
                                          "void main() {\n"
                                          "    int char_id = gl_VertexID / 6;\n"
                                          "    int vertex_id = gl_VertexID % 6;\n"
                                          "    \n"
                                          "    if (char_id >= char_count) {\n"
                                          "        gl_Position = vec4(0,0,0,1);\n"
                                          "        uv = vec2(0);\n"
                                          "        return;\n"
                                          "    }\n"
                                          "    \n"
                                          "    vec4 rect = texelFetch(char_data, char_id * 2 + 0);\n"
                                          "    vec4 uv_rect = texelFetch(char_data, char_id * 2 + 1);\n"
                                          "    \n"
                                          "    vec2 positions[6] = vec2[](\n"
                                          "        vec2(rect.x, rect.y),\n"
                                          "        vec2(rect.x + rect.z, rect.y),\n"
                                          "        vec2(rect.x, rect.y + rect.w),\n"
                                          "        vec2(rect.x + rect.z, rect.y),\n"
                                          "        vec2(rect.x + rect.z, rect.y + rect.w),\n"
                                          "        vec2(rect.x, rect.y + rect.w)\n"
                                          "    );\n"
                                          "    \n"
                                          "    vec2 uvs[6] = vec2[](\n"
                                          "        vec2(uv_rect.x, uv_rect.w),\n"
                                          "        vec2(uv_rect.z, uv_rect.w),\n"
                                          "        vec2(uv_rect.x, uv_rect.y),\n"
                                          "        vec2(uv_rect.z, uv_rect.w),\n"
                                          "        vec2(uv_rect.z, uv_rect.y),\n"
                                          "        vec2(uv_rect.x, uv_rect.y)\n"
                                          "    );\n"
                                          "    \n"
                                          "    gl_Position = projection * vec4(positions[vertex_id], 0.0, 1.0);\n"
                                          "    uv = uvs[vertex_id];\n"
                                          "}\n";

// accidently auto formatted
static const char *fragment_shader_source = "#version 330 core\n"
                                            "in vec2 uv;\n"
                                            "uniform sampler2D font_texture;\n"
                                            "uniform vec4 text_color;\n"
                                            "out vec4 FragColor;\n"
                                            "\n"
                                            "void main() {\n"
                                            "    float alpha = texture(font_texture, uv).r;\n"
                                            "    FragColor = vec4(text_color.rgb, text_color.a * alpha);\n"
                                            "}\n";

static GLuint compile_shader(GLenum type, const char *source) {
   GLuint shader = glCreateShader(type);
   glShaderSource(shader, 1, &source, NULL);
   glCompileShader(shader);

   GLint success;
   glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetShaderInfoLog(shader, 512, NULL, info_log);
      printf("Shader compilation failed: %s\n", info_log);
      glDeleteShader(shader);
      return 0;
   }
   return shader;
}

// DONT use out apis yet
static bool create_shader_program() {
   GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
   GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

   if (!vertex_shader || !fragment_shader) {
      return false;
   }

   g_text_renderer.shader_program = glCreateProgram();
   glAttachShader(g_text_renderer.shader_program, vertex_shader);
   glAttachShader(g_text_renderer.shader_program, fragment_shader);
   glLinkProgram(g_text_renderer.shader_program);

   GLint success;
   glGetProgramiv(g_text_renderer.shader_program, GL_LINK_STATUS, &success);
   if (!success) {
      char info_log[512];
      glGetProgramInfoLog(g_text_renderer.shader_program, 512, NULL, info_log);
      printf("Shader linking failed: %s\n", info_log);
      glDeleteProgram(g_text_renderer.shader_program);
      return false;
   }

   glDeleteShader(vertex_shader);
   glDeleteShader(fragment_shader);

   g_text_renderer.u_texture = glGetUniformLocation(g_text_renderer.shader_program, "font_texture");
   g_text_renderer.u_projection = glGetUniformLocation(g_text_renderer.shader_program, "projection");
   g_text_renderer.u_text_color = glGetUniformLocation(g_text_renderer.shader_program, "text_color");
   g_text_renderer.u_char_data = glGetUniformLocation(g_text_renderer.shader_program, "char_data");
   g_text_renderer.u_char_count = glGetUniformLocation(g_text_renderer.shader_program, "char_count");


   return true;
}

static int load_font(const char *font_path) {
   if (g_text_renderer.current_font_count >= MAX_FONTS) {
      return -1;
   }

   // Check if font already loaded
   for (int i = 0; i < g_text_renderer.current_font_count; i++) {
      if (g_text_renderer.fonts[i].loaded && strcmp(g_text_renderer.fonts[i].path, font_path) == 0) {
         return i;
      }
   }

   FILE *file = fopen(font_path, "rb");
   if (!file) {
      printf("Failed to open font file: %s\n", font_path);
      return -1;
   }

   fseek(file, 0, SEEK_END);
   size_t size = ftell(file);
   fseek(file, 0, SEEK_SET);

   FontData *font = &g_text_renderer.fonts[g_text_renderer.current_font_count];
   font->font_data = malloc(size);
   if (!font->font_data) {
      printf("Failed to allocate memory for font data\n");
      fclose(file);
      return -1;
   }

   if (fread(font->font_data, 1, size, file) != size) {
      printf("Failed to read font file\n");
      free(font->font_data);
      fclose(file);
      return -1;
   }
   fclose(file);

   if (!stbtt_InitFont(&font->font_info, font->font_data, 0)) {
      printf("Failed to initialize font\n");
      free(font->font_data);
      return -1;
   }

   font->loaded = true;
   strncpy(font->path, font_path, sizeof(font->path) - 1);
   font->path[sizeof(font->path) - 1] = '\0';

   return g_text_renderer.current_font_count++;
}

// Pack font atlas using stb_rect_pack with oversampling
static FontAtlas *pack_font_atlas(int font_id, float font_size) {
   if (font_id < 0 || font_id >= g_text_renderer.current_font_count) {
      return NULL;
   }

   // Find existing atlas for this size
   for (int i = 0; i < MAX_FONT_SIZES; i++) {
      FontAtlas *atlas = &g_text_renderer.atlases[font_id][i];
      if (atlas->packed && fabs(atlas->font_size - font_size) < 0.1f) {
         return atlas;
      }
   }

   // Find empty slot
   FontAtlas *atlas = NULL;
   for (int i = 0; i < MAX_FONT_SIZES; i++) {
      if (!g_text_renderer.atlases[font_id][i].packed) {
         atlas = &g_text_renderer.atlases[font_id][i];
         break;
      }
   }

   if (!atlas) {
      printf("Too many font sizes cached\n");
      return NULL;
   }

   FontData *font = &g_text_renderer.fonts[font_id];
   atlas->scale = stbtt_ScaleForPixelHeight(&font->font_info, font_size);
   atlas->font_size = font_size;

   stbtt_GetFontVMetrics(&font->font_info, &atlas->ascent, &atlas->descent, &atlas->line_gap);

   // Prepare rectangle packing
   stbrp_context pack_context;
   stbrp_node pack_nodes[MAX_GLYPHS];
   stbrp_init_target(&pack_context, ATLAS_SIZE, ATLAS_SIZE, pack_nodes, MAX_GLYPHS);

   stbrp_rect pack_rects[MAX_GLYPHS];
   unsigned char *glyph_bitmaps[MAX_GLYPHS];
   int glyph_count = 0;

   // Generate glyph data for ASCII 32-127
   for (int i = 32; i < 128 && glyph_count < MAX_GLYPHS; i++) {
      int glyph_index = stbtt_FindGlyphIndex(&font->font_info, i);
      if (glyph_index == 0 && i != 32) continue; // Skip missing glyphs except space

      GlyphInfo *glyph = &atlas->glyphs[glyph_count];
      glyph->codepoint = i;

      int advance, lsb;
      stbtt_GetGlyphHMetrics(&font->font_info, glyph_index, &advance, &lsb);
      glyph->advance = advance * atlas->scale;

      int x0, y0, x1, y1; stbtt_GetGlyphBitmapBoxSubpixel(&font->font_info, glyph_index, atlas->scale * OVERSAMPLE_X, 
                                      atlas->scale * OVERSAMPLE_Y, 0, 0, &x0, &y0, &x1, &y1);

      int width = x1 - x0;
      int height = y1 - y0;

      if (width > 0 && height > 0) {
         // Store glyph bounds
         glyph->x0 = x0; glyph->y0 = y0;
         glyph->x1 = x1; glyph->y1 = y1;
         glyph->xoff = x0 / (float)OVERSAMPLE_X;
         glyph->yoff = y0 / (float)OVERSAMPLE_Y;

         // Generate oversampled bitmap
         glyph_bitmaps[glyph_count] = malloc(width * height);
         if (glyph_bitmaps[glyph_count]) {
            stbtt_MakeGlyphBitmapSubpixel(&font->font_info, glyph_bitmaps[glyph_count], 
                                         width, height, width, atlas->scale * OVERSAMPLE_X, 
                                         atlas->scale * OVERSAMPLE_Y, 0, 0, glyph_index);

            // Setup rect for packing
            pack_rects[glyph_count].w = width;
            pack_rects[glyph_count].h = height;
            pack_rects[glyph_count].id = glyph_count;
         }
      } else {
         glyph_bitmaps[glyph_count] = NULL;
         pack_rects[glyph_count].w = 0;
         pack_rects[glyph_count].h = 0;
         pack_rects[glyph_count].id = glyph_count;
      }

      glyph->bitmap_index = glyph_count;
      glyph_count++;
   }

   // Packing rectangles
   if (!stbrp_pack_rects(&pack_context, pack_rects, glyph_count)) {
      printf("Failed to pack font atlas\n");
      for (int i = 0; i < glyph_count; i++) {
         if (glyph_bitmaps[i]) free(glyph_bitmaps[i]);
      }
      return NULL;
   }

   unsigned char *atlas_bitmap = calloc(ATLAS_SIZE * ATLAS_SIZE, 1);
   if (!atlas_bitmap) {
      printf("Failed to allocate atlas bitmap\n");
      for (int i = 0; i < glyph_count; i++) {
         if (glyph_bitmaps[i]) free(glyph_bitmaps[i]);
      }
      return NULL;
   }

   // Copy glyphs to atlas and downsample
   for (int i = 0; i < glyph_count; i++) {
      if (!glyph_bitmaps[i] || !pack_rects[i].was_packed) continue;

      GlyphInfo *glyph = &atlas->glyphs[i];
      int src_w = glyph->x1 - glyph->x0;
      int src_h = glyph->y1 - glyph->y0;
      int dst_x = pack_rects[i].x;
      int dst_y = pack_rects[i].y;

      // Downsample from oversampled bitmap
      for (int y = 0; y < src_h / OVERSAMPLE_Y; y++) {
         for (int x = 0; x < src_w / OVERSAMPLE_X; x++) {
            int sum = 0;
            for (int oy = 0; oy < OVERSAMPLE_Y; oy++) {
               for (int ox = 0; ox < OVERSAMPLE_X; ox++) {
                  int sx = x * OVERSAMPLE_X + ox;
                  int sy = y * OVERSAMPLE_Y + oy;
                  if (sx < src_w && sy < src_h) {
                     sum += glyph_bitmaps[i][sy * src_w + sx];
                  }
               }
            }
            atlas_bitmap[(dst_y + y) * ATLAS_SIZE + (dst_x + x)] = sum / (OVERSAMPLE_X * OVERSAMPLE_Y);
         }
      }

      // Store UV coordinates (downsampled size)
      glyph->x0 = dst_x;
      glyph->y0 = dst_y;
      glyph->x1 = dst_x + src_w / OVERSAMPLE_X;
      glyph->y1 = dst_y + src_h / OVERSAMPLE_Y;

      free(glyph_bitmaps[i]);
   }

   // Create OpenGL texture
   glGenTextures(1, &atlas->texture);
   glBindTexture(GL_TEXTURE_2D, atlas->texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, atlas_bitmap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   free(atlas_bitmap);
   atlas->packed = true;

   return atlas;
}

// NOTE: Not using api yet, still experimenting A lot
static void init_text_renderer() {
   if (g_text_renderer.initialized)
      return;

   if (!create_shader_program()) {
      printf("Failed to create text shader program\n");
      return;
   }

   glGenVertexArrays(1, &g_text_renderer.dummy_vao);
   glGenBuffers(1, &g_text_renderer.char_data_buffer);
   glGenTextures(1, &g_text_renderer.char_data_texture);

   glBindBuffer(GL_TEXTURE_BUFFER, g_text_renderer.char_data_buffer);
   glBufferData(GL_TEXTURE_BUFFER, MAX_TEXT_LENGTH * 2 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

   glBindTexture(GL_TEXTURE_BUFFER, g_text_renderer.char_data_texture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, g_text_renderer.char_data_buffer);

   g_text_renderer.initialized = true;
}

static GlyphInfo *find_glyph(FontAtlas *atlas, int codepoint) {
   for (int i = 0; i < MAX_GLYPHS; i++) {
      if (atlas->glyphs[i].codepoint == codepoint) {
         return &atlas->glyphs[i];
      }
   }
   return NULL;
}

// FIX: Gross.
float screen_width = 600.0f, screen_height = 400.0f;

void draw_text_extended(const char *text, float font_size, Rectangle_Float rect) {
   if (!g_text_renderer.initialized) {
      init_text_renderer();
   }

   if (!text || strlen(text) == 0)
      return;

   int font_id = load_font("res/fonts/Alegreya-Regular.ttf");
   if (font_id == -1)
      return;

   FontAtlas *atlas = pack_font_atlas(font_id, font_size);
   if (!atlas)
      return;

   int text_len = strlen(text);
   if (text_len > MAX_TEXT_LENGTH)
      text_len = MAX_TEXT_LENGTH;

   float char_data[MAX_TEXT_LENGTH * 8];
   int visible_chars = 0;

   float x = rect.x;
   float y = rect.y + atlas->ascent * atlas->scale;

   for (int i = 0; i < text_len; i++) {
      char c = text[i];

      if (c == '\n') {
         x = rect.x;
         y += (atlas->ascent - atlas->descent + atlas->line_gap) * atlas->scale;
         continue;
      }

      GlyphInfo *glyph = find_glyph(atlas, c);
      if (!glyph) continue;

      if (glyph->x1 > glyph->x0 && glyph->y1 > glyph->y0) {
         int base = visible_chars * 8;

         float gx = x + glyph->xoff;
         float gy = y + glyph->yoff;
         float gw = (glyph->x1 - glyph->x0);
         float gh = (glyph->y1 - glyph->y0);

         // Character rectangle
         char_data[base + 0] = gx;
         char_data[base + 1] = screen_height - gy - gh;
         char_data[base + 2] = gw;
         char_data[base + 3] = gh;

         // UV coordinates
         char_data[base + 4] = glyph->x0 / (float)ATLAS_SIZE;
         char_data[base + 5] = glyph->y0 / (float)ATLAS_SIZE;
         char_data[base + 6] = glyph->x1 / (float)ATLAS_SIZE;
         char_data[base + 7] = glyph->y1 / (float)ATLAS_SIZE;

         visible_chars++;
      }

      x += glyph->advance;
   }

   if (visible_chars == 0)
      return;

   glBindBuffer(GL_TEXTURE_BUFFER, g_text_renderer.char_data_buffer);
   glBufferSubData(GL_TEXTURE_BUFFER, 0, visible_chars * 8 * sizeof(float), char_data);

   glUseProgram(g_text_renderer.shader_program);
   glBindVertexArray(g_text_renderer.dummy_vao);

   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, atlas->texture);
   glUniform1i(g_text_renderer.u_texture, 0);

   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_BUFFER, g_text_renderer.char_data_texture);
   glUniform1i(g_text_renderer.u_char_data, 1);

   glUniform1i(g_text_renderer.u_char_count, visible_chars);
   glUniform4f(g_text_renderer.u_text_color, 1.0f, 1.0f, 1.0f, 1.0f);

   float projection[16] = {2.0f / screen_width, 0, 0, 0, 0, 2.0f / screen_height, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1};
   glUniformMatrix4fv(g_text_renderer.u_projection, 1, GL_FALSE, projection);

   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   glDrawArrays(GL_TRIANGLES, 0, visible_chars * 6);

   glDisable(GL_BLEND);
}

void draw_text(const char *text) {
   float font_size = 24.;
   draw_text_extended(text, font_size, (Rectangle_Float){50, 50, 150, font_size*2});
}

void shutdown_text_renderer() {
   for (int i = 0; i < g_text_renderer.current_font_count; i++) {
      if (g_text_renderer.fonts[i].loaded) {
         free(g_text_renderer.fonts[i].font_data);
      }

      for (int j = 0; j < MAX_FONT_SIZES; j++) {
         if (g_text_renderer.atlases[i][j].packed) {
            glDeleteTextures(1, &g_text_renderer.atlases[i][j].texture);
         }
      }
   }

   if (g_text_renderer.char_data_buffer) {
      glDeleteBuffers(1, &g_text_renderer.char_data_buffer);
   }

   if (g_text_renderer.char_data_texture) {
      glDeleteTextures(1, &g_text_renderer.char_data_texture);
   }

   if (g_text_renderer.shader_program) {
      glDeleteProgram(g_text_renderer.shader_program);
   }

   if (g_text_renderer.dummy_vao) {
      glDeleteVertexArrays(1, &g_text_renderer.dummy_vao);
   }

   memset(&g_text_renderer, 0, sizeof(g_text_renderer));
}

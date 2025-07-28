#define MAX_FONTS 16
#define MAX_FONT_SIZES 8
#define ATLAS_SIZE 1024
#define MAX_GLYPHS 256
#define MAX_TEXT_LENGTH 1024

// For text positioning
typedef struct {
   float x, y, width, height;
} Rectangle_Float;

typedef struct {
   stbtt_fontinfo font_info;
   unsigned char *font_data;
   bool loaded;
   char path[256]; // Store path for comparison
} FontData;

// Font atlas for specific size
typedef struct {
   GLuint texture;
   stbtt_bakedchar char_data[MAX_GLYPHS];
   float font_size;
   bool baked;
} FontAtlas;

// Global text renderer state
static struct {
   FontData fonts[MAX_FONTS];
   FontAtlas atlases[MAX_FONTS][MAX_FONT_SIZES];
   int current_font_count;

   GLuint shader_program;
   GLint u_texture, u_projection, u_text_color, u_char_data, u_char_count;
   GLuint dummy_vao;

   // Texture buffer for character data
   GLuint char_data_buffer;
   GLuint char_data_texture;

   bool initialized;
} g_text_renderer = {0};

// Uses texture buffer instead of uniform arrays
static const char *vertex_shader_source = "#version 330 core\n"
                                          "uniform mat4 projection;\n"
                                          "uniform samplerBuffer char_data;\n" // Texture buffer with character data
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
                                          "    // Fetch character data from texture buffer\n"
                                          "    // Each character uses 2 texels: [0]=rect(x,y,w,h), [1]=uv(u0,v0,u1,v1)\n"
                                          "    vec4 rect = texelFetch(char_data, char_id * 2 + 0);\n"    // x, y, width, height\n"
                                          "    vec4 uv_rect = texelFetch(char_data, char_id * 2 + 1);\n" // u0, v0, u1, v1\n"
                                          "    \n"
                                          "    // Quad vertices: 0,1,2 = bottom-left triangle, 3,4,5 = top-right triangle\n"
                                          "    vec2 positions[6] = vec2[](\n"
                                          "        vec2(rect.x, rect.y),                    // 0: bottom-left\n"
                                          "        vec2(rect.x + rect.z, rect.y),           // 1: bottom-right\n"
                                          "        vec2(rect.x, rect.y + rect.w),           // 2: top-left\n"
                                          "        vec2(rect.x + rect.z, rect.y),           // 3: bottom-right\n"
                                          "        vec2(rect.x + rect.z, rect.y + rect.w),  // 4: top-right\n"
                                          "        vec2(rect.x, rect.y + rect.w)            // 5: top-left\n"
                                          "    );\n"
                                          "    \n"
                                          "    vec2 uvs[6] = vec2[](\n"
                                          "        vec2(uv_rect.x, uv_rect.w),              // 0: u0, v1\n"
                                          "        vec2(uv_rect.z, uv_rect.w),              // 1: u1, v1\n"
                                          "        vec2(uv_rect.x, uv_rect.y),              // 2: u0, v0\n"
                                          "        vec2(uv_rect.z, uv_rect.w),              // 3: u1, v1\n"
                                          "        vec2(uv_rect.z, uv_rect.y),              // 4: u1, v0\n"
                                          "        vec2(uv_rect.x, uv_rect.y)               // 5: u0, v0\n"
                                          "    );\n"
                                          "    \n"
                                          "    gl_Position = projection * vec4(positions[vertex_id], 0.0, 1.0);\n"
                                          "    uv = uvs[vertex_id];\n"
                                          "}\n";

// Fragment shader
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

   // Get uniform locations
   g_text_renderer.u_texture = glGetUniformLocation(g_text_renderer.shader_program, "font_texture");
   g_text_renderer.u_projection = glGetUniformLocation(g_text_renderer.shader_program, "projection");
   g_text_renderer.u_text_color = glGetUniformLocation(g_text_renderer.shader_program, "text_color");
   g_text_renderer.u_char_data = glGetUniformLocation(g_text_renderer.shader_program, "char_data");
   g_text_renderer.u_char_count = glGetUniformLocation(g_text_renderer.shader_program, "char_count");

   return true;
}

// Load font from file
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
   usz size = ftell(file);
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

// Get or create font atlas for specific size
static FontAtlas *get_font_atlas(int font_id, float font_size) {
   if (font_id < 0 || font_id >= g_text_renderer.current_font_count) {
      return NULL;
   }

   // Find existing atlas for this size
   for (int i = 0; i < MAX_FONT_SIZES; i++) {
      FontAtlas *atlas = &g_text_renderer.atlases[font_id][i];
      if (atlas->baked && fabs(atlas->font_size - font_size) < 0.1f) {
         return atlas;
      }
   }

   // Find empty slot
   FontAtlas *atlas = NULL;
   for (int i = 0; i < MAX_FONT_SIZES; i++) {
      if (!g_text_renderer.atlases[font_id][i].baked) {
         atlas = &g_text_renderer.atlases[font_id][i];
         break;
      }
   }

   if (!atlas) {
      printf("Too many font sizes cached\n");
      return NULL;
   }

   // Create atlas texture
   unsigned char *bitmap = malloc(ATLAS_SIZE * ATLAS_SIZE);
   if (!bitmap) {
      printf("Failed to allocate bitmap memory\n");
      return NULL;
   }

   // Initialize bitmap to zero
   memset(bitmap, 0, ATLAS_SIZE * ATLAS_SIZE);

   // Bake font
   int result = stbtt_BakeFontBitmap(g_text_renderer.fonts[font_id].font_data, 0, font_size, bitmap, ATLAS_SIZE, ATLAS_SIZE, 32, MAX_GLYPHS - 32, // ASCII 32-255
                                     atlas->char_data);

   if (result <= 0) {
      printf("Failed to bake font bitmap\n");
      free(bitmap);
      return NULL;
   }

   // Create OpenGL texture
   glGenTextures(1, &atlas->texture);
   glBindTexture(GL_TEXTURE_2D, atlas->texture);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

   free(bitmap);

   atlas->font_size = font_size;
   atlas->baked = true;

   return atlas;
}

// Initialize text renderer
static void init_text_renderer() {
   if (g_text_renderer.initialized)
      return;

   // Create shader program
   if (!create_shader_program()) {
      printf("Failed to create text shader program\n");
      return;
   }

   // Create a dummy VAO (required by OpenGL core profile)
   glGenVertexArrays(1, &g_text_renderer.dummy_vao);

   // Create texture buffer for character data
   glGenBuffers(1, &g_text_renderer.char_data_buffer);
   glGenTextures(1, &g_text_renderer.char_data_texture);

   // Setup texture buffer (each char needs 2 vec4s: rect + uvs)
   glBindBuffer(GL_TEXTURE_BUFFER, g_text_renderer.char_data_buffer);
   glBufferData(GL_TEXTURE_BUFFER, MAX_TEXT_LENGTH * 2 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

   glBindTexture(GL_TEXTURE_BUFFER, g_text_renderer.char_data_texture);
   glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, g_text_renderer.char_data_buffer);

   g_text_renderer.initialized = true;
}

// Main text rendering function
void draw_text_extended(const char *text, float font_size, Rectangle_Float rect) {
   float screen_width = (float)800, screen_height = 600;
   if (!g_text_renderer.initialized) {
      init_text_renderer();
   }

   if (!text || strlen(text) == 0)
      return;

   // Load font
   int font_id = load_font("res/fonts/Alegreya-Regular.ttf");
   if (font_id == -1)
      return;

   // Get font atlas
   FontAtlas *atlas = get_font_atlas(font_id, font_size);
   if (!atlas)
      return;

   // Calculate character positions and UVs
   int text_len = strlen(text);
   if (text_len > MAX_TEXT_LENGTH)
      text_len = MAX_TEXT_LENGTH;

   float char_data[MAX_TEXT_LENGTH * 8]; // Each char: rect(4) + uvs(4) = 8 floats
   int visible_chars = 0;

   float x = rect.x;
   float y = rect.y + font_size; // Baseline adjustment

   for (int i = 0; i < text_len; i++) {
      char c = text[i];

      // Handle newlines
      if (c == '\n') {
         x = rect.x;
         y += font_size * 1.2f; // Line spacing
         continue;
      }

      if (c < 32 || c >= 127)
         continue; // Skip non-printable chars

      stbtt_aligned_quad q;
      stbtt_GetBakedQuad(atlas->char_data, ATLAS_SIZE, ATLAS_SIZE, c - 32, &x, &y, &q, 1);

      // Each character uses 8 floats: rect(4) + uvs(4)
      int base = visible_chars * 8;

      // Character rectangle (flip Y coordinate for screen space)
      char_data[base + 0] = q.x0;                 // x
      char_data[base + 1] = screen_height - q.y1; // y (flipped for screen coordinates)
      char_data[base + 2] = q.x1 - q.x0;          // width
      char_data[base + 3] = q.y1 - q.y0;          // height

      // UV coordinates
      char_data[base + 4] = q.s0; // u0
      char_data[base + 5] = q.t0; // v0
      char_data[base + 6] = q.s1; // u1
      char_data[base + 7] = q.t1; // v1

      visible_chars++;
   }

   if (visible_chars == 0)
      return;

   // Upload character data to texture buffer
   glBindBuffer(GL_TEXTURE_BUFFER, g_text_renderer.char_data_buffer);
   glBufferSubData(GL_TEXTURE_BUFFER, 0, visible_chars * 8 * sizeof(float), char_data);

   // Set up rendering state
   glUseProgram(g_text_renderer.shader_program);
   glBindVertexArray(g_text_renderer.dummy_vao);

   // Bind font texture to unit 0
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, atlas->texture);
   glUniform1i(g_text_renderer.u_texture, 0);

   // Bind character data texture to unit 1
   glActiveTexture(GL_TEXTURE1);
   glBindTexture(GL_TEXTURE_BUFFER, g_text_renderer.char_data_texture);
   glUniform1i(g_text_renderer.u_char_data, 1);

   // Set other uniforms
   glUniform1i(g_text_renderer.u_char_count, visible_chars);
   glUniform4f(g_text_renderer.u_text_color, 1.0f, 1.0f, 1.0f, 1.0f); // White text

   // Set projection matrix (orthographic)
   float projection[16] = {2.0f / screen_width, 0, 0, 0, 0, 2.0f / screen_height, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1};
   glUniformMatrix4fv(g_text_renderer.u_projection, 1, GL_FALSE, projection);

   // Enable blending for text
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

   // Draw all characters in one call
   glDrawArrays(GL_TRIANGLES, 0, visible_chars * 6);

   glDisable(GL_BLEND);
}

void draw_text(const char *text) { draw_text_extended(text, 24., (Rectangle_Float){50, 50, 400, 60}); }

void cleanup_text_renderer() {
   for (int i = 0; i < g_text_renderer.current_font_count; i++) {
      if (g_text_renderer.fonts[i].loaded) {
         free(g_text_renderer.fonts[i].font_data);
      }

      for (int j = 0; j < MAX_FONT_SIZES; j++) {
         if (g_text_renderer.atlases[i][j].baked) {
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


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

uint32_t reload_compute_shader(uint32_t shader_handle, const char* path);
uint32_t create_compute_shader(const char* path);
uint32_t create_compute_shader_from_memory(const char* source);
uint32_t create_compute_shader_toy(const char* path);
uint8_t* read_entire_file_into_memory(const char* path);


#define BUFFER_SIZE 512
#ifdef PLATFORM_WINDOWS
char* read_stdin() {
    static char buffer[BUFFER_SIZE];
    DWORD bytes_read;

    // Prompt the user for input
    DWORD written;
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "Enter a string: ", 16, &written, NULL);

    // Read from standard input
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    if (!ReadFile(hStdin, buffer, BUFFER_SIZE - 1, &bytes_read, NULL)) {
        WriteConsole(GetStdHandle(STD_ERROR_HANDLE), "Error reading input\n", 20, &written, NULL);
        exit(1);
    }

    // Null-terminate the string
    buffer[bytes_read] = '\0';

    // Output the read string
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), "You entered: ", 13, &written, NULL);
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE), buffer, bytes_read, &written, NULL);

    return buffer;
}
#else

char* read_stdin() {
    static char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    write(STDOUT_FILENO, "Enter a string: ", 16);

    bytes_read = read(STDIN_FILENO, buffer, BUFFER_SIZE - 1);

    if (bytes_read < 0) {
        write(STDERR_FILENO, "Error reading input\n", 20);
        exit(1);
    }

    // Null-terminate the string
    buffer[bytes_read] = '\0';

    write(STDOUT_FILENO, "You entered: ", 13);
    write(STDOUT_FILENO, buffer, bytes_read);

    return buffer;
}
#endif

static bool pre_process_shader(const char *path, DString *ds);

#define INVALID_SHADER_HANDLE ((GLuint)-1)

#define read_file read_entire_file_into_memory

static bool pre_process_shader(const char *path, DString *ds) {
   char *source = read_file(path);
   if (!source) {
      return false;
   }

   stb_lexer lexer;
   char store[8192] = {0}; // Max possible path string in #include that we can read
   assert((sizeof store / sizeof store[0]) == 8192);
   stb_c_lexer_init(&lexer, source, source + strlen(source), store, (sizeof store / sizeof store[0]));

   char *start = lexer.parse_point;
   char *end   = lexer.parse_point;
   while (stb_c_lexer_get_token(&lexer)) {
      if (lexer.token == '#' && stb_c_lexer_get_token(&lexer)) {
         if (lexer.token == CLEX_id && strcmp(lexer.string, "include") == 0) {
            if (stb_c_lexer_get_token(&lexer) && lexer.token == CLEX_dqstring) {
               // On double quoted token the inside string (without quote) is stored at lexer.string
               const char *include_path = lexer.string;
               ds_write_buf(ds, start, end-start);
               start = lexer.parse_point;
               end   = lexer.parse_point;
               ds_write(ds, "\n"); // More readable in case of outputting to a file
               if (false == pre_process_shader(include_path, ds)) {
                  assert_msg(false, "TODO handle pre_process failure");
                  return false;
               }
            }
         }
      }
      end = lexer.parse_point;
   }
   ds_write_buf(ds, start, end-start);
   free(source);
   return true;
}

int write_entire_file_from_memory(const char *path, const uint8_t *data, size_t size) {
   // Create if doesn't exist, overwrite if it does
   int file = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   if (file < 0) {
      fprintf(stderr, "Failed to open file for writing: %s\n", path);
      return -1;
   }

   // Write data
   ssize_t bytes_written = write(file, data, size);
   if (bytes_written < 0 || (size_t)bytes_written != size) {
      fprintf(stderr, "Failed to write complete file: %s\n", path);
      close(file);
      return -1;
   }

   if (close(file) < 0) {
      fprintf(stderr, "Failed to close file: %s\n", path);
      return -1;
   }

   return 0; // Success
}

uint8_t* read_entire_file_into_memory(const char* path) {
    int file = open(path, O_RDONLY);
    if (file < 0) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    off_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    // Read the file content into a buffer
    char *data = (char *)malloc(file_size + 1);
    if (data == NULL) {
       fprintf(stderr, "Failed to allocate memory for shader source\n");
       close(file);
       return NULL;
    }

    read(file, data, file_size);
    data[file_size] = '\0'; // Null-terminate the string
    close(file);
    return data;
}



uint32_t create_compute_shader_toy(const char* path) {
    // Bad way of doing things, me no r'member size info xD
    char* prepend
        = read_entire_file_into_memory("./src/shaders/shadertoy/preamble.inc.glsl");

    char* shadertoy
        = read_entire_file_into_memory(path);

    size_t prepend_size   = strlen(prepend);
    size_t shadertoy_size = strlen(shadertoy);
    size_t size = prepend_size + shadertoy_size + 1;
    char  *source = malloc(size);

    memcpy(source, prepend, prepend_size);
    memcpy(source + prepend_size, shadertoy,  shadertoy_size);
    source[size] = '\0';

    DString ds = {0};
    if (false == pre_process_shader(source, &ds)) {
       return INVALID_SHADER_HANDLE;
    }
    ds_write_zero(&ds);
    uint32_t result = create_compute_shader_from_memory(source);
    // write_entire_file_from_memory("debug.glsl", source, size);

    free(source);
    free(shadertoy);
    free(prepend);
    return result;
}

// Create and preprocess and compile the shader
uint32_t create_compute_shader(const char* path) {
    DString ds = {0};
    // WARNING: We don't detect cyclic includes. #include "a" in b and #include "b" in a will halt the program
    if (false == pre_process_shader(path, &ds)) {
        write_entire_file_from_memory("src/shaders/output/failed-dump.glsl", ds.data, ds.size);
        return INVALID_SHADER_HANDLE;
    }
    write_entire_file_from_memory("src/shaders/output/dump.glsl", ds.data, ds.size);

    ds_write_zero(&ds);
    uint32_t result = create_compute_shader_from_memory(ds.data);
    ds_free(ds);
    return result;
}

// Returns INVALID_SHADER_HANDLE (-1) if error
// TODO: Allow passing size along with the string
uint32_t create_compute_shader_from_memory(const char* source) {
    GLuint shader_handle = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader_handle, 1, (const GLchar**)&source, NULL);
    glCompileShader(shader_handle);

    // Check for compilation errors
    GLint is_compiled = 0;
    glGetShaderiv(shader_handle, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        GLint max_length = 0;
        glGetShaderiv(shader_handle, GL_INFO_LOG_LENGTH, &max_length);

        // The info log is a string
        char* info_log = (char*)malloc(max_length);
        glGetShaderInfoLog(shader_handle, max_length, &max_length, info_log);

        fprintf(stderr, "%s\n", info_log);
        free(info_log);
        glDeleteShader(shader_handle);
        return INVALID_SHADER_HANDLE;
    }

    // Create and link the program
    GLuint program = glCreateProgram();
    glAttachShader(program, shader_handle);
    glLinkProgram(program);

    // Check for linking errors
    GLint is_linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (is_linked == GL_FALSE)
    {
        GLint max_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &max_length);

        char* info_log = (char*)malloc(max_length);
        glGetProgramInfoLog(program, max_length, &max_length, info_log);

        fprintf(stderr, "%s\n", info_log);
        free(info_log);
        glDeleteProgram(program);
        glDeleteShader(shader_handle);
        return INVALID_SHADER_HANDLE;
    }

    // Clean up
    glDetachShader(program, shader_handle);
    return program;
}

uint32_t reload_compute_shader(uint32_t shader_handle, const char *path) {
   uint32_t new_shader_handle = create_compute_shader(path);

   // Keep current shader while errors in new shader
   if (INVALID_SHADER_HANDLE == new_shader_handle) {
      return shader_handle;
   }

   // Only delete if shader was valid to begin with
   if (INVALID_SHADER_HANDLE != shader_handle) {
      glDeleteProgram(shader_handle);
   }
   return new_shader_handle;
}

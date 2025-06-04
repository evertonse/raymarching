#ifndef _CYE_H_
#define _CYE_H_


// Ideias
//
// - Errors:
//      Must be gracefully handled, the default is if things already exist, it's ok.
//      If some action is not permitted, trace log an error and let the user handdle (don't crash).
// - Conventions:
//      If the function returns `TString` or has a `t` prefix as in `tstrdup` then it's temporary allocated, it , then i
//
//

/*..................................................................................
 .                                                                                 .
 .                                BASIC                                            .
 .                                                                                 .
 ...................................................................................
*/
//----------------------------------------------------------------------------------
//  Basic Includes
//----------------------------------------------------------------------------------


#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <stdio.h>


#if defined(__MINGW32__)
#   define PLATFORM_MINGW
#   define PLATFORM_WINDOWS
#elif defined(_WIN32)
#   define PLATFORM_WINDOWS
#else
#   define PLATFORM_LINUX
#endif


#ifdef PLATFORM_WINDOWS
#    define WIN32_LEAN_AND_MEAN
#    include <windows.h>
#    include <direct.h>
#    include <shlobj.h>
#    include <oleidl.h>
#    include <shellapi.h>
#    include <io.h>
#ifndef FILE_SHARE_DELETE
#    define FILE_SHARE_DELETE 0x00000004 // MinGW compatibility
#endif
#    if defined(PLATFORM_MINGW)
#        include <fcntl.h>
#    else
#        define stat _stat
#        define utimbuf _utimbuf
#        define utime _utime
#    endif
#    define ENV_SEPARATOR ";"
#    define ENV_SEPARATOR_CHAR ';'
#    define PATH_SEPARATOR "\\"
#    define END_OF_LINE "\r\n"
#    define PATH_SEPARATOR_CHAR '\\'
#    if !defined(PATH_MAX)
#        define PATH_MAX MAX_PATH
#    endif
#else
#    include <sys/types.h>
#    include <sys/wait.h>
#    include <sys/stat.h>
#    include <unistd.h>
#    include <fcntl.h>
#    include <dirent.h>
#    include <pwd.h>
#    include <utime.h>
#    include <errno.h>
#    define ENV_SEPARATOR ":"
#    define ENV_SEPARATOR_CHAR ':'
#    define END_OF_LINE "\n"
#    define PATH_SEPARATOR "/"
#    define PATH_SEPARATOR_CHAR '/'
#endif


//----------------------------------------------------------------------------------
//  Basic Definitions with No Prefix
//----------------------------------------------------------------------------------

#ifndef as
#   define as(Type) (Type)
#endif

// Why would a signed sizeof be more useful?
#ifndef size_of
#   define size_of(x) (isize)(sizeof(x))
#endif

#ifndef count_of
#define count_of(x)                                                            \
  ((size_of(x) / size_of(x[0])) / ((isize)(!(size_of(x) % size_of(x[0])))))
#endif

#ifndef offset_of
#   define offset_of(Type, element) ((isize) & (((Type *)0)->element))
#endif

#ifndef fn_unused
#   if defined(_MSC_VER)
#       define fn_unused(x) (__pragma(warning(suppress : 4100))(x))
#   elif defined(__GCC__)
#       define fn_unused(x) __attribute__((__unused__)) (x)
#   else
#       define fn_unused(x)
#   endif
#endif

#define unused(x) ((void)(x))


#ifndef kilobytes
#   define kilobytes(x) ((x) * (i64)(1024))
#   define megabytes(x) (kilobytes(x) * (i64)(1024))
#   define gigabytes(x) (megabytes(x) * (i64)(1024))
#   define terabytes(x) (gigabytes(x) * (i64)(1024))
#endif

#define internal   static
#define local      static
#define file_scope static
#define fallthrough /* nothing */

#if defined(__GNUC__) || defined(__GNUG__)
#   define force_inline   inline __attribute__((always_inline))
#   define force_noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#   if _MSC_VER < 1300
#       define force_inline
#   else
#       define force_inline __forceinline
#       define force_noinline __declspec(noinline)
#   endif
#endif

#if !defined(__cplusplus)
#   if defined(_MSC_VER) && _MSC_VER <= 1800
#       define inline __inline
#   elif !defined(__STDC_VERSION__)
#       define inline __inline__
#   else
#       define inline
#   endif
#endif

#ifndef DEBUG_TRAP
#   if defined(_MSC_VER)
#      if _MSC_VER < 1300
#          define DEBUG_TRAP() __asm int 3
#      else
#          define DEBUG_TRAP() __debugbreak()
#      endif
#   else
#      define DEBUG_TRAP() __builtin_trap()
#   endif
#endif

#ifndef static_assert

// Create a maybe valid type
// Then, use the type by creating a variable, no unused typedef
// Then, Use the variable, no unused variable
#   define static_assert3(cond, msg)                              \
        typedef char static_assertion_##msg[(!!(cond))*2-1];      \
        static static_assertion_##msg static_assertion_use_##msg; \
        // unused(static_assertion_use_##msg);

#   define static_assert2(cond, line) static_assert3(cond, static_assertion_at_line_##line)
#   define static_assert1(cond, line) static_assert2(cond, line)
#   define static_assert(cond)        static_assert1(cond, __LINE__)
#endif


#if defined(__GNUC__) || defined(__GNUG__)
#   define force_restrict __restrict__
#elif defined(_MSC_VER)
#   define force_restrict __restrict
#endif

#if !defined(thread_local)
#   if defined(_MSC_VER) && _MSC_VER >= 1300
#       define thread_local __declspec(thread)
#   else
#       ifdef __cplusplus
#           define thread_local thread_local
#       else
#           define thread_local __thread // TODO: maybe check for GCC just to be sure
#       endif
#   endif
#endif

#ifndef null
#   if defined(__cplusplus)
#      if __cplusplus >= 201103L
#          define null nullptr
#      else
#          define null 0
#      endif
#   else
#      define null ((void *)0)
#   endif
#endif

#ifndef U8_MIN
#define U8_MIN 0u
#define U8_MAX 0xffu
#define I8_MIN (-0x7f - 1)
#define I8_MAX 0x7f

#define U16_MIN 0u
#define U16_MAX 0xffffu
#define I16_MIN (-0x7fff - 1)
#define I16_MAX 0x7fff

#define U32_MIN 0u
#define U32_MAX 0xffffffffu
#define I32_MIN (-0x7fffffff - 1)
#define I32_MAX 0x7fffffff

#define U64_MIN 0ull
#define U64_MAX 0xffffffffffffffffull
#define I64_MIN (-0x7fffffffffffffffll - 1)
#define I64_MAX 0x7fffffffffffffffll

#if defined(__i386__) || UINTPTR_MAX == 0xffFFffFF
#   define USIZE_MIX U32_MIN
#   define USIZE_MAX U32_MAX
#   define ISIZE_MIX I32_MIN
#   define ISIZE_MAX I32_MAX
#elif defined(__amd64__) || defined(__X86_64__) || UINTPTR_MAX == 0xffFFffFFffFFffFF
#   define USIZE_MIN  U64_MIN
#   define USIZE_MAX  U64_MAX
#   define ISIZE_MIN  I64_MIN
#   define ISIZE_MAX  I64_MAX
#else
#   //TODO: Portable warning is needed because of MSVC
#   warning "You might need to check for more CPU Architectures"
#endif

#define USZ_MIN  USIZE_MIN
#define USZ_MAX  USIZE_MAX
#define ISZ_MIX  ISIZE_MIN
#define ISZ_MAX  ISIZE_MAX

#define ESCAPE_CODE_HEADER    "\033[95m"
#define ESCAPE_CODE_OKBLUE    "\033[94m"
#define ESCAPE_CODE_OKCYAN    "\033[96m"
#define ESCAPE_CODE_OKGREEN   "\033[92m"
#define ESCAPE_CODE_WARNING   "\033[93m"
#define ESCAPE_CODE_FAIL      "\033[91m"
#define ESCAPE_CODE_UNDERLINE "\033[4m"
#define ESCAPE_CODE_LOG       "\x1b[30;1m";
#define ESCAPE_CODE_WARN      "\x1b[1m\x1b[33m";
#define ESCAPE_CODE_ERROR     "\x1b[1m\x1b[31m";
#define ESCAPE_CODE_BOLD      "\x1b[37m";
#define ESCAPE_CODE_RESET     "\033[0m"



#define F32_MIN 1.17549435e-38f
#define F32_MAX 3.40282347e+38f

#define F64_MIN 2.2250738585072014e-308
#define F64_MAX 1.7976931348623157e+308
#endif

// Adapted from Odin src code
#define set_bit(bitfield, pos)       ((bitfield)  |=  (1 << (pos)))
#define clear_bit(bitfield, pos)     ((bitfield)  &= ~(1 << (pos)))
#define toggle_bit(bitfield, pos)    ((bitfield)  ^=  (1 << (pos)))
#define read_bit(bitfield, pos)      (((bitfield) >>  (pos)) & 0x01)

#if !defined(PI)
#   define PI 3.14159265358979323846
#endif

#if !defined(EPSILON)
    #define EPSILON 0.000001f
#endif

#ifndef DEG2RAD
    #define DEG2RAD (PI/180.0f)
#endif

#ifndef RAD2DEG
    #define RAD2DEG (180.0f/PI)
#endif

//----------------------------------------------------------------------------------
//  Tweakable Constants
//----------------------------------------------------------------------------------
#ifndef cye_malloc
#   define cye_malloc  malloc
#endif

#ifndef cye_calloc
#   define cye_calloc  calloc
#endif

#ifndef cye_realloc
#   define cye_realloc realloc
#endif

#ifndef cye_free
#   define cye_free    free
#endif

#if !defined(cliteral) && defined(__cplusplus)
#   define cliteral(Type)      Type
#else
#   define cliteral(Type)      (Type)
#endif


#ifndef CYE_DARRAY_INIT_CAP
#   define CYE_DARRAY_INIT_CAP (2*PATH_MAX)
#endif

#ifndef CYE_DARRAY_CAP_MULTIPLIER
#   define CYE_DARRAY_CAP_MULTIPLIER 2
#endif


#ifndef CYE_TEMP_CAPACITY
#   define CYE_TEMP_CAPACITY megabytes(16)
#endif

#ifndef CYE_MAX_TRACE_LOG_MSG_LENGTH
#   define CYE_MAX_TRACE_LOG_MSG_LENGTH 1024
#endif

#ifndef CYE_PATH_MAX
#   define CYE_PATH_MAX (PATH_MAX*2)
#endif


/*..................................................................................
 .                                                                                 .
 .                                Types                                            .
 .                                                                                 .
 ...................................................................................
*/
//----------------------------------------------------------------------------------
//  Structures Definition without Prefix
//----------------------------------------------------------------------------------

// Boolean type
#if (defined(__STDC__) && __STDC_VERSION__ >= 199901L) || (defined(_MSC_VER) && _MSC_VER >= 1800)
#   include <stdbool.h>
#elif !defined(__cplusplus) && !defined(bool)
    typedef enum bool { false = 0, true = !false } bool;
#endif

// TODO: Do we need something like this  #ifndef CYE_NO_INT_TYPES ?
// I remember that linux kernel has some types like this maybe we need to prefix and optionally strip

// I saw this typing in rust and casey's stream, i really like it

// Floating Point
typedef float       f32;
typedef double      f64;

// Unsigned Integers
typedef __uint128_t u128;
typedef uint64_t    u64;
typedef uint32_t    u32;
typedef uint16_t    u16;
typedef uint8_t     u8;
typedef uint8_t     byte;

typedef void        u0;
typedef void*       rawptr;

//      Unsigned  Integers
typedef int64_t   i64;
typedef int32_t   i32;
typedef int16_t   i16;
typedef int8_t    i8;
typedef size_t    usz;
typedef ptrdiff_t isz;

typedef size_t    usize;
typedef ptrdiff_t isize;


typedef i32 rune;
typedef i8  b8;
typedef i16 b16;
typedef i32 b32;

#define RUNE_INVALID as(Rune)(0xfffd)
#define RUNE_MAX     as(Rune)(0x0010ffff)
#define RUNE_BOM     as(Rune)(0xfeff)
#define RUNE_EOF     as(Rune)(-1)


typedef const char* ZString;   // Static Zero Terminated String
typedef       char* TString;   // Temporary String
typedef       char* MutString; // Mutable String, might be temporary or not

//----------------------------------------------------------------------------------
//  Structures Definition with Prefix
//----------------------------------------------------------------------------------

// NOTE: Organized by priority level
typedef enum {
    CYE_LOG_ALL = 0,        // Display all logs
    CYE_LOG_TRACE,          // Trace logging, intended for internal use only
    CYE_LOG_DEBUG,          // Debug logging, used for internal debugging, it should be disabled on release builds
    CYE_LOG_INFO,           // Info logging, used for program execution info
    CYE_LOG_OKAY,             // Everthying Works, a bit more important and info but less important than error
    CYE_LOG_WARNING,        // Warning logging, used on recoverable failures
    CYE_TRACE_ERROR,          // Error logging, used on unrecoverable failures
    CYE_LOG_FATAL,          // Fatal logging, used to abort program: exit(EXIT_FAILURE)
    CYE_LOG_NONE            // Disable logging
} Cye_Log_Level;

#define Cye_DArray(Type) \
struct {                 \
    Type *items;         \
    usz count;           \
    usz capacity;        \
}

typedef struct {
    union {
        const char **items;
        const char **paths;
    };
    usz count;
    usz capacity;
} Cye_Path_DArray;

typedef bool (*Cye_File_Filter)(const char *path, void *user_data);

typedef enum {
    CYE_FILE_KIND_REGULAR = 0,
    CYE_FILE_KIND_DIRECTORY,
    CYE_FILE_KIND_SYMLINK,
    CYE_FILE_KIND_OTHER,
} Cye_File_Kind;

typedef enum {
    CYE_GLOB_SYNTAX_ERROR,
    CYE_GLOB_NO_MATCH,
    CYE_GLOB_MATCHED
} Cye_Match_Result; // Don't know if we'll keep it

typedef enum {
    CYE_PATTERN_NO_ESCAPE = (0x0001 << 0),
    CYE_PATTERN_PATH      = (0x0001 << 1),
    CYE_PATTERN_PERIOD    = (0x0001 << 2)
} Cye_Pattern_Flags; // Don't know if we'll keep it

typedef struct {
    time_t created_at;
    time_t accessed_at;
    time_t modified_at;
    usz size_bytes;
} Cye_File_Stats;


typedef struct {
    union {
        char* items;
        char* chars;
        u8*   data;
    };
    union {
        usz count;
        usz size;
    };
    usz capacity;
} Cye_DString;


#ifdef PLATFORM_WINDOWS
    #define CYE_INVALID_PROCESS     (INVALID_HANDLE_VALUE)
    #define CYE_INVALID_FILE_HANDLE (INVALID_HANDLE_VALUE)
    typedef HANDLE Cye_Process;
    typedef HANDLE Cye_File_Handle;
#else
    #define CYE_INVALID_PROCESS     (-1)
    #define CYE_INVALID_FILE_HANDLE (-1)
    typedef int Cye_Process;
    typedef int Cye_File_Handle;
#endif // PLATFORM_WINDOWS

typedef struct {
    Cye_Process *items;
    usz count;
    usz capacity;
} Cye_Process_DArray;

typedef struct {
    const char **items;
    usz count;
    usz capacity;
} Cye_Command;

typedef struct {
    Cye_File_Handle *in;
    Cye_File_Handle *out;
    Cye_File_Handle *err;
} Cye_Command_Redirect;

typedef struct {
    union {
        usz count;
        usz len;
        usz size;
    };
    const char *data;
} Cye_String_Slice;

typedef Cye_DArray(Cye_String_Slice) Cye_String_Slice_DArray;

typedef struct {
    void*  (*alloc)(usz size);
    void*  (*realloc) (void *ptr, usz size);
    void   (*free)(void* ptr);
    rawptr any;
} Cye_Context;

typedef struct {
    MutString pattern;
    MutString pattern_next;
    Cye_Path_DArray* matches;
} Cye_Glob_Filter_Data;

#ifndef PLATFORM_WINDOWS
    typedef int Cye_Pipe_Handle;
    #define CYE_INVALID_PIPE_HANDLE (-1)
#else
    typedef HANDLE Cye_Pipe_Handle;
    #define CYE_INVALID_PIPE_HANDLE INVALID_HANDLE_VALUE
#endif

typedef struct {
    Cye_Pipe_Handle read;
    Cye_Pipe_Handle write;
} Cye_Pipe;

#ifndef PLATFORM_WINDOWS
    #define CYE_INVALID_PIPE ((Cye_Pipe){-1, -1})
#else
    #define CYE_INVALID_PIPE ((Cye_Pipe){INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE})
#endif


typedef struct {
    Cye_DString out;
    Cye_DString err;
} Cye_Capture_Result;


/*..................................................................................
 .                                                                                 .
 .                               Declarations                                      .
 .                                                                                 .
 ...................................................................................
*/

//------------------------------------------------------------------------------------
//  Process and File Declarations
//------------------------------------------------------------------------------------

Cye_File_Handle cye_file_open_for_read(ZString path);
Cye_File_Handle cye_file_open_for_write(ZString path);
void cye_file_close(Cye_File_Handle fh);
bool cye_process_wait_all(Cye_Process_DArray procs);
bool cye_process_wait_all_and_reset(Cye_Process_DArray *procs);
bool cye_process_wait(Cye_Process proc); // Wait until the process has finished

//------------------------------------------------------------------------------------
//  Commands Declarations
//------------------------------------------------------------------------------------
#define cye_cmd_append(cmd, ...)              \
    cye_da_append_buf(                        \
        cmd, ((const char *[]){__VA_ARGS__}), \
        (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)))

#define cye_cmd_extend(cmd, other_cmd) \
    cye_da_append_buf(cmd, (other_cmd)->items, (other_cmd)->count)

// Free all the memory allocated by command arguments
#define cye_cmd_free(cmd) cye_da_free(cmd)


// Render a string representation of a command into a dynamic string.
void cye_ds_write_cmd(Cye_DString *ds, Cye_Command cmd);

// Run redirected command asynchronously
Cye_Process cye_cmd_run_async_and_reset(Cye_Command *cmd);

// Run redirected command asynchronously and reset count
Cye_Process cye_cmd_run_async_redirect(Cye_Command cmd, Cye_Command_Redirect redirect);

// Run redirected command asynchronously and set cmd.count to 0 and close all the opened files
Cye_Process cye_cmd_run_async_redirect_and_reset(Cye_Command *cmd, Cye_Command_Redirect redirect);

// Run command asynchronously
#define cye_cmd_run_async(cmd) cye_cmd_run_async_redirect(cmd, (Cye_Command_Redirect) {0})

// Run command synchronously
bool cye_cmd_run_sync(Cye_Command cmd);

bool cye_cmd_run_sync_and_reset(Cye_Command *cmd);

// Run redirected command synchronously
bool cye_cmd_run_sync_redirect(Cye_Command cmd, Cye_Command_Redirect redirect);

// Run redirected command synchronously and set cmd.count to 0 and close all the opened files
bool cye_cmd_run_sync_redirect_and_reset(Cye_Command *cmd, Cye_Command_Redirect redirect);
bool cye_cmd_run_sync_capture_and_reset(Cye_Command* cmd, Cye_Capture_Result* result);

#if !defined(cye_rebuild_command)
#  ifdef _WIN32
#    if defined(__GNUC__)
#       define cye_rebuild_command(binary_path, source_path) "gcc", "-o", binary_path, source_path, "-Wno-unused-parameter"
#    elif defined(__clang__)
#       define cye_rebuild_command(binary_path, source_path) "clang", "-o", binary_path, source_path
#    elif defined(_MSC_VER)
#       if defined(__clang__)
#           define cye_rebuild_command(binary_path, source_path) "clang-cl.exe", cye_tprintf("/Fe:%s", (binary_path)), source_path
#       else
#           define cye_rebuild_command(binary_path, source_path) "cl.exe", cye_tprintf("/Fe:%s", (binary_path)), source_path
#       endif
#    endif
#  else
#    define cye_rebuild_command(binary_path, source_path) "cc", "-o", binary_path, source_path
#  endif
#endif

// Sean Barret Style in-between `__`
// void cye__rebuild_ourselves(const char *source_path, int argc, char **argv);
void cye__rebuild_ourselves(ZString source_path, int argc, ZString *argv);
#define cye_rebuild_ourselves(argc, argv) cye__rebuild_ourselves(__FILE__, argc, argv)


//------------------------------------------------------------------------------------
//  Storage Declarations
//------------------------------------------------------------------------------------



Cye_Context cye_temp_context(void);
Cye_Context cye_default_context(void);

u0 cye_set_default_context(Cye_Context ctx);

char* cye_tstrdup(ZString cstr);
void* cye_talloc(usz size);
void* cye_trealloc(void *ptr, usz size);
void  cye_tfree(rawptr ptr);

TString cye_tprintf(ZString fmt, ...);

void cye_temp_reset(void);
usz  cye_temp_save(void);
void cye_temp_restore(usz checkpoint);

//------------------------------------------------------------------------------------
//  Path Declarations
//------------------------------------------------------------------------------------

bool cye_make_dir_if_not_exists(ZString path);
bool cye_copy_file(ZString src_path, ZString dst_path);
bool cye_copy_dir(ZString src_path, ZString dst_path);
bool cye_read_dir_filtered(
    ZString parent, Cye_Path_DArray *children,
    bool use_parent, Cye_File_Filter filter, void *user_data
);

#define cye_read_dir(parent, children) cye_read_dir_filtered(parent, children, false, NULL, NULL)

// Append data to the end of file
bool cye_append_file(const char* path, const void* data, usz count);

// Append zero terminated string to the end of file
bool cye_append_file_zstr(const char* path, const char* str);

// Write bytes to a file, creating if it doesnt exist
bool cye_write_file(ZString path, const void *data, usz size);
#define cye_write_file_zstr(path, zstring) cye_write_file(path, zstring, strlen(zstring))

// Read contents of file and return a malloc'ed pointer and zero terminates. You should free it with libc free. Returns NULL on error.
char* cye_read_file(const char* path);

// Read contents of file into a Dynamic String
bool cye_ds_read_file(ZString path, Cye_DString *ds);

// Get File Type
Cye_File_Kind cye_path_file_kind(ZString path);

// Normalize Path ex: ///oi/hello/././.txt -> /oi/hello/.txt
char* cye_path_temp_normalize(ZString path);

// Create from path parts and normalize
char* cye_path_create_from_array(ZString paths[], usz paths_count);

#define cye_path_create(...)                                            \
    cye_path_create_from_array(                                         \
        ((const char*[]){__VA_ARGS__}),                                 \
        (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)))

#define cye_path_temp_create(...)                                        \
({                                                                       \
    Cye_Context before = cye_context;                                    \
    cye_context = cye_temp_context();                                    \
    TString path = cye_path_create_from_array(                           \
        ((const char*[]){__VA_ARGS__}),                                  \
        (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *))); \
    cye_context = before;                                                \
    path;                                                                \
})


ZString cye_path_base_name(ZString path);
ZString cye_path_expand_user(ZString path);  // Expand ~ and ~user to full home directory path
ZString cye_path_expand_vars(ZString path);  // Expand environment variables

int  cye_needs_rebuild_from_buf(ZString output_path, ZString *input_paths, usz input_paths_count);

#define cye_needs_rebuild(output_path, ...)                    \
    cye_needs_rebuild_from_buf(                                \
        output_path, ((const char *[]){__VA_ARGS__}),          \
        (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)))



TString cye_path_temp_cwd(void);                                  // Get current working directory
bool cye_path_set_cwd(ZString path);                          // Change current working directory

b32  cye_file_exists(ZString file_path);
bool cye_file_stats(const char* path, Cye_File_Stats* stats);     // Get file stats, can use ctime() to get certain fields as strings
bool cye_is_absolute(ZString path);                               // Check if path  is absolute
bool cye_is_relative(ZString path);                               // Check if path  is relative
bool cye_is_file(ZString path);                                   // Check if path  is regular  file
bool cye_is_dir(ZString path);                                    // Check if path  is directory
bool cye_is_executable(ZString path);
bool cye_find_executable(ZString name, Cye_DString* out_path);    // Given a name, it finds the executable path
bool cye_is_period_dir(ZString path);                             // Check if path is the directory `./` of `..`
bool cye_is_link(ZString path);                                   // Check if path  is symbolic link
bool cye_is_mount(ZString path);                                  // Check if path  is mount    point
bool cye_is_same_path(ZString path1, ZString path2);              // Check if paths reference same file (one can be absolute and another relative or on be a hard link)

Cye_DString cye_path_join(ZString path, ZString* paths);          // Join paths intelligently

usz cye_path_size(ZString path);                                  // Size  in bytes

ZString cye_path_real(ZString path);                              // Returns real path (resolve symlinks)
ZString cye_path_absolute(ZString path);                          // Returns absolute path
ZString cye_path_relative(ZString from, ZString target);          // Returns relative path

ZString cye_path_home(void);                                      //  Return home
ZString cye_path_cwd(void);                                       //  Return current directory
ZString cye_path_parent(ZString path);                            //  Returns parent directory
ZString cye_path_owner(ZString path);                             //  Returns parent directory
TString cye_path_stem(ZString file_path);                         //  Return path without extension
ZString cye_path_dir_of(ZString file_path);                       //  Return directory where file is, if it's already an directory it return its self
ZString cye_path_ext(ZString path);                               //  Returns only the extension
bool    cye_path_touch(ZString path);                             //  Creates an empty file if not already exists

#define cye_make_dir  cye_make_dir_if_not_exists                  // Create directory
#define cye_make_dirs cye_make_dir_include_parents                // Create directory
bool cye_make_dir_include_parents(ZString path);                  // Create directories including parents as needed
bool cye_make_dir_include_parents_from_tstr(TString path);        // Create directories recursively

bool cye_remove_file(ZString path);                               // Remove file
bool cye_remove_dir(ZString path);                                // Remove directories recursively
bool cye_path_move(ZString src, ZString dst);                     // Move file or directory
bool cye_path_rename(ZString src, ZString dst);                   // Rename file or directory
bool cye_path_renames(ZString old_path, ZString new_path);        // Recursive directory or file renaming
bool cye_path_replace(ZString src, ZString dst);                  // Rename file or directory, replacing if exists

Cye_Path_DArray cye_path_scandir(ZString path);                   // Iterator of directory entries
Cye_Path_DArray cye_list_dir(ZString path);                       // Similar to LS or read_dir, but is return the darray

bool cye_path_glob(ZString pattern, Cye_Path_DArray *matches);     // Glob a path shell style * ? [] and [!]
Cye_Path_DArray cye_path_tglob(ZString pattern);               // Sames as Glob but Temporary Allocated strings

// bool cye_path_walk                                              // Generate directory tree

#define cye_file_stats_fmt "{.created_at=%s (%zu), .accessed_at=%s (%zu), .modified_at=%s (%zu), .size=%zu (bytes)}"
#define cye_file_stats_fmt_arg(stats)                              \
    strtok(ctime(&(stats).created_at), "\n"),  (stats).created_at, \
    strtok(ctime(&(stats).accessed_at), "\n"), (stats).accessed_at,\
    strtok(ctime(&(stats).modified_at), "\n"), (stats).modified_at,\
    (stats).size_bytes

//------------------------------------------------------------------------------------
//  Pipe Declarations
//------------------------------------------------------------------------------------
Cye_Pipe cye_pipe_open(void);
void cye_pipe_close(Cye_Pipe handle);
bool cye_is_pipe_valid(Cye_Pipe handle);
bool cye_pipe_read(Cye_Pipe_Handle pipe, char* buffer, usz buffer_size, usz* bytes_read);
bool cye_pipe_write(Cye_Pipe_Handle pipe, const char* buffer, usz buffer_size, usz* bytes_written);
void cye_pipe_close_handle(Cye_Pipe_Handle* pipe);

//------------------------------------------------------------------------------------
//  Dynamic Array Declarations
//------------------------------------------------------------------------------------


// Append an item to a dynamic array using thread_local Cye_Context cye_context
#define cye_da_append(da, item)                                                            \
    do {                                                                                   \
        if ((da)->count >= (da)->capacity) {                                               \
            (da)->capacity = (da)->capacity == 0 ?                                         \
                CYE_DARRAY_INIT_CAP :                                                      \
                (da)->capacity*CYE_DARRAY_CAP_MULTIPLIER;                                  \
                                                                                           \
            (da)->items = cye_context.realloc(                                             \
                (da)->items, (da)->capacity*sizeof(((da)->items)[0])                       \
            );                                                                             \
            cye_assert((da)->items != NULL && "Dynamic Array: OOM");                       \
        }                                                                                  \
                                                                                           \
        (da)->items[(da)->count++] = (item);                                               \
    } while (0)


//NOTE: Be aware that da_remove.* create a variable i_1 and i_2 that might
// if you're having unintuitive behaviour, remember, these are macros
// you might have been having variable overiding somewhere, specially if you're calling macros inside macros;

// Removes an item from a dynamic array at the specified index
#define cye_da_remove(da, index)                                                               \
    do {                                                                                       \
        cye_assert((da)->count > 0 && "Trying to delete from empty array");                    \
        cye_assert((((usz)(index)) < (da)->count) && "Index out of bounds");                   \
                                                                                               \
        /* Shift remaining elements to the left */                                             \
        for (usz i_1 = (index); i_1 < (da)->count - 1; ++i_1) {                                \
            (da)->items[i_1] = (da)->items[i_1 + 1];                                           \
        }                                                                                      \
                                                                                               \
        (da)->count--;                                                                         \
    } while (0)



// Alternative version that removes by matching value (first occurrence)
// NOTE: Requires a valid LValue for `item`
#define cye_da_remove_item(da, item)                                                         \
    do {                                                                                     \
        for (usz i_2 = 0; i_2 < (da)->count; ++i_2) {                                        \
            bool ok = memcmp(&((da)->items[i_2]), &(item), sizeof((da)->items[i_2])) == 0;   \
            if (ok) {                                                                        \
                cye_da_remove((da), i_2);                                                    \
                break;                                                                       \
            }                                                                                \
        }                                                                                    \
    } while (0)


#define cye_da_free(da) cye_context.free((da).items)

#define cye_da_append_buf(da, new_items, new_items_count)                                       \
    do {                                                                                        \
        if ((da)->count + (new_items_count) > (da)->capacity) {                                 \
            if ((da)->capacity == 0) {                                                          \
                (da)->capacity = CYE_DARRAY_INIT_CAP;                                           \
            }                                                                                   \
            while ((da)->count + (new_items_count) > (da)->capacity) {                          \
                (da)->capacity *= CYE_DARRAY_CAP_MULTIPLIER;                                    \
            }                                                                                   \
            (da)->items = cye_context.realloc((da)->items, (da)->capacity*sizeof(*(da)->items));\
            cye_assert((da)->items != NULL && "Dynamic Array: OOM");                            \
        }                                                                                       \
        memcpy((da)->items + (da)->count, (new_items), (new_items_count)*sizeof(*(da)->items)); \
        (da)->count += (new_items_count);                                                       \
    } while (0)

#define cye_da_fmt         "{.items=%p, .count=%zu, .capacity=%zu}"
#define cye_da_fmt_arg(da) ((void*)(da).items), (da).count, (da).capacity

//------------------------------------------------------------------------------------
//  Slices Declarations
//------------------------------------------------------------------------------------


// Generic slice structure
#define Cye_Slice(T) struct { T data; usz count; }

// Create a slice from an pointer and count
#define cye_slice_make(ptr, cnt) {.data = (ptr), .count = (cnt)}

// Create a slice from an array literal
#define cye_slice_from_arr(arr) cye_slice_make((arr), sizeof(arr)/sizeof((arr)[0]))

// Create an empty slice
#define cye_slice_empty(T) ((T)){.data = NULL, .count = 0}
#define CYE_STR_SLICE_EMPTY (const Cye_String_Slice){.data = NULL, .count = 0}

// Get subslice [start, end)
#define cye_slice_range(ptr, start, end) \
    cye_slice_make(ptr + (start), ((end) - (start)))

// Get first n elements
#define cye_slice_prefix(ptr, n) \
    cye_slice_make(ptr, (n))

// Get last n elements
#define cye_slice_suffix(slice, n) \
    cye_slice_make((slice).data + ((slice).count - (n)), (n))

// Compare two slices
#define cye_slice_equal(a, b)    \
    (((a).count == (b).count) && \
     (memcmp((a).data, (b).data, (a).count * sizeof(*(a).data)) == 0))

// Check if slice contains element
// @extensions, revise
#define cye_slice_contains(slice, elem) ({   \
    bool found = false;                      \
    for(usz i = 0; i < (slice).count; i++) { \
        if ((slice).data[i] == (elem)) {     \
            found = true;                    \
            break;                           \
        }                                    \
    }                                        \
    found;                                   \
})

// Check if slice is empty
#define cye_slice_is_empty(slice) ((slice).count == 0)

// Get element at index with bounds checking
#define cye_slice_at(slice, idx) \
    (((idx) < (slice).count) ? (slice).data[idx] : NULL)

// Copy slice to buffer
#define cye_slice_copy(dst, src) \
    memcpy((dst).data, (src).data, (src).count * sizeof(*(src).data))

// Find index of element
// @extensions
#define cye_slice_index_of(slice, elem) ({   \
    usz idx = (usz)-1;                       \
    for(usz i = 0; i < (slice).count; i++) { \
        if ((slice).data[i] == (elem)) {     \
            idx = i;                         \
            break;                           \
        }                                    \
    }                                        \
    idx;                                     \
})
#define cye_slice_fmt "{.data=%p, .count=%zu}"
#define cye_slice_fmt_arg(slice)  slice.data, slice.count

//------------------------------------------------------------------------------------
//  String Slice Declarations
//------------------------------------------------------------------------------------



Cye_String_Slice cye_str_slice_make(ZString str);
#define cye_str_slice_from_zstr cye_str_slice_make

// Trim whitespace from both ends
Cye_String_Slice cye_str_slice_trim(Cye_String_Slice s);

// String slice to null-terminated string (requires buffer)
void cye_str_slice_to_zstr(Cye_String_Slice s, char *buf, usz buf_size);

// Strip left whitespace
Cye_String_Slice cye_str_slice_strip_left(Cye_String_Slice s);

// Strip right whitespace
Cye_String_Slice cye_str_slice_strip_right(Cye_String_Slice s);

// Create string slice from string and explicit length
Cye_String_Slice cye_str_slice_make_len(ZString str, usz len);

// Compare two string slices
bool cye_str_slice_equals(Cye_String_Slice a, Cye_String_Slice b);

// Compare a slice with a zstring
bool cye_str_slice_equals_zstr(Cye_String_Slice a, const char* b);

// Check if string slice contains substring
bool cye_str_slice_contains(Cye_String_Slice haystack, Cye_String_Slice needle);

// Split string slice by delimiter into a Dynamic Array
Cye_String_Slice_DArray cye_str_slice_split(Cye_String_Slice s, Cye_String_Slice delim);
Cye_String_Slice_DArray cye_str_slice_split_zstr(Cye_String_Slice s, ZString delim);


// Split string slice at first occurrence of delimiter
void cye_str_slice_split_first(Cye_String_Slice s, char delim, Cye_String_Slice *before, Cye_String_Slice *after);

// Check if string slice starts with prefix
bool cye_str_slice_starts_with(Cye_String_Slice s, Cye_String_Slice prefix);

// Check if string slice ends with suffix
bool cye_str_slice_ends_with(Cye_String_Slice s, Cye_String_Slice suffix);

// Check if string slice ends with zero-terminated suffix
bool cye_str_slice_ends_with_zstr(Cye_String_Slice s, ZString suffix);

// Check if string slice starts with zero-terminated prefix
bool cye_str_slice_starts_with_zstr(Cye_String_Slice s, ZString prefix);

//------------------------------------------------------------------------------------
//  ZString Declarations
//------------------------------------------------------------------------------------

bool cye_zstr_ends_with(ZString src, ZString ending);
bool cye_zstr_starts_with(ZString src, ZString prefix);
bool cye_zstr_match_pattern(ZString pattern, ZString str);
ZString cye_zstr_ordinal(int n);

bool cye_pattern_match(ZString pattern, ZString string, int flags);
bool cye_is_pattern_well_formed(ZString pattern);
Cye_Match_Result cye_glob_match(ZString pattern, ZString text);


// TODO: Add String Slices Functions as we need

#define cye_ss_fmt "%.*s"
#define cye_ss_fmt_arg(sv) (int)(sv).count, (sv).data

//----------------------------------------------------------------------------------
//  Dynamic String Declarations
//----------------------------------------------------------------------------------

// Don't need to null terminate to see the dynamic string

#define cye_ds_write_buf(ds, buf, size) cye_da_append_buf(ds, buf, size)

#define cye_ds_write_zstr(ds, zstr)   \
    do {                              \
        ZString s = (zstr);       \
        usz n = strlen(s);            \
        cye_da_append_buf(ds, s, n);  \
    } while (0)

#define cye_ds_write(ds, ...)                                               \
    do {                                                                    \
        ZString cye_tmp_strs[] = {__VA_ARGS__};                         \
        for (usz idx = 0;                                                   \
             idx < sizeof(cye_tmp_strs) / sizeof(cye_tmp_strs[0]);          \
             idx++)                                                         \
        {                                                                   \
            ZString s = cye_tmp_strs[idx];                              \
            usz n = strlen(s);                                              \
            cye_da_append_buf(ds, s, n);                                    \
        }                                                                   \
    } while (0)

#define cye_ds_write_char(ds, ch) \
    cye_da_append(ds, ch)

// Write zero byte onto the Dynamic String
#define cye_ds_write_zero(ds) cye_ds_write_char(ds, '\0')

// Free the memory allocated by the Dynamic String
#define cye_ds_free(ds) cye_da_free(ds)

#define cye_ds_clear(ds) (ds)->count = 0

// Formated Print onto the Dynamic String
void cye_ds_printf(Cye_DString *ds, ZString fmt, ...);


#define cye_ds_fmt "{.items=%.*s(%p), .count=%zu, .capacity=%zu}"
#define cye_ds_fmt_arg(ds) (ds).count, (ds).items, (ds).items, (ds).count, (ds).capacity
//----------------------------------------------------------------------------------
//  Mathematics Declarations
//----------------------------------------------------------------------------------
#ifndef cye_max
#   define cye_max(value1, value2) ((value1) > (value2)) ? (value1) : (value2)
#endif

#ifndef cye_min
#   define cye_min(value1, value2) ((value1) < (value2)) ? (value1) : (value2)
#endif

// Clamp float value
f32 cye_clamp(f32 value, f32 min, f32 max);

// Calculate linear interpolation between two floats
f32 cye_lerp(f32 start, f32 end, f32 amount);

// Normalize input value within input range
f32 cye_normalize(f32 value, f32 start, f32 end);

// Remap input value within input range to output range
f32 cye_remap(f32 value, f32 inputStart, f32 inputEnd, f32 outputStart, f32 outputEnd);

// Wrap input value from min to max
f32 cye_wrap(f32 value, f32 min, f32 max);

// Check whether two given f32s are almost equal
int cye_float_equals(f32 x, f32 y);


//------------------------------------------------------------------------------------
//  Utils Declarations
//------------------------------------------------------------------------------------
void cye_set_trace_level(Cye_Log_Level level);
void cye_trace_log(Cye_Log_Level level, ZString fmt, ...);
#define cye_trace_info(...)  cye_trace_log(CYE_LOG_INFO,    __VA_ARGS__)
#define cye_trace_okay(...)  cye_trace_log(CYE_LOG_OKAY,    __VA_ARGS__)
#define cye_trace_error(...) cye_trace_log(CYE_TRACE_ERROR,   __VA_ARGS__)
#define cye_trace_warn(...)  cye_trace_log(CYE_LOG_WARNING, __VA_ARGS__)
#define cye_trace_fatal(...) cye_trace_log(CYE_LOG_FATAL,   __VA_ARGS__)
#define cye_trace_debug(fmt, ...) cye_trace_log(CYE_LOG_DEBUG, "`%s`: "fmt, __func__, __VA_ARGS__)

#define cye_return_defer(code) do { code; goto defer; } while(0)
#define cye_result_defer(value) do { result = (value); goto defer; } while(0)

// Consider using logging instead ? Maybe not
#define cye_todo(msg)        do { fprintf(stderr, "%s:%d: %s TODO: %s\n",       __FILE__, __LINE__,__func__,  msg); abort(); } while(0)
#define cye_unreachable(msg) do { fprintf(stderr, "%s:%d: %s UNREACHABLE: %s\n",__FILE__, __LINE__,__func__,  msg); abort(); } while(0)
#define cye_panic(msg)       do { fprintf(stderr, "%s:%d: %s PANIC: " msg "\n",__FILE__, __LINE__,__func__); abort(); } while(0)

#define cye_not_implemented(msg)    cye_assert_msg(false, msg)

#define cye_shift(items, items_sz)  (cye_assert_msg(((items_sz) > 0), "%s", "Shift WAY TOO MUCH"), (items_sz) -= 1, *(items)++)

void cye__assert_handler(char const *prefix, char const *condition, char const *file, int line, char const *msg, ...);

#ifndef cye_assert_msg
#define cye_assert_msg(cond, msg, ...)                             \
    ((void)((cond) ||                                              \
        (cye__assert_handler("Assertion Failure", #cond, __FILE__, \
                          (int)__LINE__, msg, ##__VA_ARGS__),      \
         DEBUG_TRAP(),                                             \
         0)))
#endif

#ifndef cye_assert
#   define cye_assert(cond) cye_assert_msg(cond, NULL)
#endif


#define cye_file_fmt "%s:%d:%s"
#define cye_file_fmt_arg __FILE__, __LINE__,__func__



// NOTE: C11 I think And this is garbage almost, can even use like normal fmt
// int a = 2;
// trace_info("some string before" cye_fmt(a), a); // does not work
#define cye_fmt(val)                        \
  _Generic((val),                           \
    Cye_String_Slice  : cye_ss_fmt,         \
    Cye_File_Stats    : cye_file_stats_fmt, \
    Cye_DString       : cye_ds_fmt,         \
    _Bool             : "%d",               \
    char              : "%c",               \
    signed char       : "%hhd",             \
    unsigned char     : "%hhu",             \
    short             : "%hd",              \
    int               : "%d",               \
    long              : "%ld",              \
    long long         : "%lld",             \
    unsigned short    : "%hu",              \
    unsigned int      : "%u",               \
    unsigned long     : "%lu",              \
    unsigned long long: "%llu",             \
    float             : "%f",               \
    double            : "%f",               \
    long double       : "%Lf",              \
    char*             : "%s",               \
    char const*       : "%s",               \
    wchar_t*          : "%ls",              \
    wchar_t const*    : "%ls",              \
    void*             : "%p",               \
    void const*       : "%p",               \
    default           : "%s"                \
  )


// This one can't be don't because _Generic only allows expressions
#if 0
// Does not work, ok? But might serve as inspiration of revising
// for another possible solution for now, just use tstring or
// .*fmt_arg directly
#define cye__fmt_arg(val)                                   \
  _Generic((val),                                           \
    Cye_String_Slice : cye_ss_fmt_arg,                      \
    Cye_File_Stats   : cye_file_stats_fmt_arg,              \
    Cye_DString      : cye_ds_fmt_arg,                      \
    default          : "(_Generic: unknown type)"           \
  )

#define cye_fmt_arg(val) cye__fmt_arg((val))((val))
#endif // 0


// These `tstring` functions should alwasy allocated new memory
// no matter if we already have some modifiable buffer like DString case
TString cye_file_stats_tstring(Cye_File_Stats stats);
TString cye_str_slice_tstring(Cye_String_Slice ss);
TString cye_ds_tstring(Cye_DString ds);

#define cye__tstring(val)                               \
  _Generic((val),                                       \
    Cye_File_Stats    : cye_file_stats_tstring,         \
    Cye_String_Slice  : cye_str_slice_tstring,          \
    Cye_DString       : cye_ds_tstring,                 \
    default           : "(_Generic: unknown type)"      \
  )

#define cye_tstring(val, ...) cye__tstring(val)(val)

#define cye_swap(T, a, b) do { \
    T tmp = (a);               \
    (a) = (b);                 \
    (b) = tmp;                 \
} while (0)

// Sort in reverse order
#define cye_sort_reverse(T, ptr, count, compare) \
    cye_sort(T, ptr, count, !(compare))

#define cye_sort cye_bubble_sort

#define cye_sort_q(T, ptr, count, compare) do {                          \
    T *arr = (ptr);                                                      \
    usz count = (count);                                                 \
    for (usz i = 1; i < count; i++) {                                    \
        T key = arr[i];                                                  \
        sizet j = i;                                                     \
        while (j > 0) {                                                  \
            T a = key;                                                   \
            T b = arr[j - 1];                                            \
            if (!(compare)) break;                                       \
            arr[j] = arr[j - 1];                                         \
            j--;                                                         \
        }                                                                \
        arr[j] = key;                                                    \
    }                                                                    \
} while (0)


// #define CYE_SORT CYE_QUICK_SORT
// Bubble sort macro - stable sort
#define cye_bubble_sort(T, ptr, count, compare) do {                    \
    T* arr = (ptr);                                                     \
    usz n = (count);                                                    \
    for (usz i = 0; i < n - 1; i++) {                                   \
        for (usz j = 0; j < n - i - 1; j++) {                           \
            T a = arr[j];                                               \
            T b = arr[j + 1];                                           \
            if (compare) {                                              \
                T temp = arr[j];                                        \
                arr[j] = arr[j + 1];                                    \
                arr[j + 1] = temp;                                      \
            }                                                           \
        }                                                               \
    }                                                                   \
} while (0)

// Quicksort macro - unstable but efficient sort
#define cye_quick_sort(T, ptr, count, compare) do {                     \
    T* arr = (ptr);                                                     \
    usz n = (count);                                                    \
    if (n <= 1) break;                                                  \
                                                                        \
    /* Stack for tracking partition ranges */                           \
    usz stack[64][2];                                                   \
    int top = 0;                                                        \
                                                                        \
    /* Initialize stack with full range */                              \
    stack[top][0] = 0;                                                  \
    stack[top][1] = n - 1;                                              \
    top++;                                                              \
                                                                        \
    while (top > 0) {                                                   \
        top--;                                                          \
        usz low = stack[top][0];                                        \
        usz high = stack[top][1];                                       \
                                                                        \
        /* Partition */                                                 \
        T pivot = arr[high];                                            \
        usz i = low;                                                    \
                                                                        \
        for (usz j = low; j < high; j++) {                              \
            T a = arr[j];                                               \
            T b = pivot;                                                \
            if (compare) {                                              \
                T temp = arr[i];                                        \
                arr[i] = arr[j];                                        \
                arr[j] = temp;                                          \
                i++;                                                    \
            }                                                           \
        }                                                               \
                                                                        \
        /* Put pivot in correct position */                             \
        T temp = arr[i];                                                \
        arr[i] = arr[high];                                             \
        arr[high] = temp;                                               \
                                                                        \
        /* Add sub-partitions to stack if they exist */                 \
        if (i > low + 1) {                                              \
            stack[top][0] = low;                                        \
            stack[top][1] = i - 1;                                      \
            top++;                                                      \
        }                                                               \
        if (i + 1 < high) {                                             \
            stack[top][0] = i + 1;                                      \
            stack[top][1] = high;                                       \
            top++;                                                      \
        }                                                               \
    }                                                                   \
} while (0)

ZString cye_cpu_architecture(void);

#endif // _CYE_H_



/*..................................................................................
 .                                                                                 .
 .                                IMPLEMENETATION                                  .
 .                                                                                 .
 ...................................................................................
*/
#if defined(CYE_IMPLEMENTATION)

//------------------------------------------------------------------------------------
//  Global Variables Implementation
//------------------------------------------------------------------------------------

static struct {
    usz size;
    rawptr last;  // Last pointer of a successful allocation, used in `trealloc`
    byte buffer[CYE_TEMP_CAPACITY];
} cye_temp_data = {0};

static Cye_Log_Level cye_threshold_log_level = CYE_LOG_INFO;

thread_local Cye_Context cye_context = {.alloc = cye_malloc, .realloc = cye_realloc, .free = cye_free, .any=null};

//------------------------------------------------------------------------------------
//  Process and File Implementation
//------------------------------------------------------------------------------------


char *win32_error_message(DWORD err);
#ifdef PLATFORM_WINDOWS
#   define CYE_GET_ERROR_STRING (win32_error_message(GetLastError()))
#else
#   define CYE_GET_ERROR_STRING (strerror(errno))
#endif


Cye_File_Handle cye_file_open_for_write(ZString path) {
#ifndef PLATFORM_WINDOWS
    Cye_File_Handle result = open(path,
        O_WRONLY | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
    );

    if (result < 0) {
        cye_trace_log(CYE_TRACE_ERROR, "Could not open file %s: %s", path, CYE_GET_ERROR_STRING);
        return CYE_INVALID_FILE_HANDLE;
    }
    return result;

#else
    SECURITY_ATTRIBUTES saAttr = {0};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;

    Cye_File_Handle result = CreateFile(
        path,                            // name of the write
        GENERIC_WRITE,                   // open for writing
        0,                               // do not share
        &saAttr,                         // default security
        OPEN_ALWAYS,                     // open always
        FILE_ATTRIBUTE_NORMAL,           // normal file
        NULL                             // no attr. template
    );

    if (result == INVALID_HANDLE_VALUE) {
        cye_trace_log(CYE_TRACE_ERROR, "Could not open file %s: %s", path, CYE_GET_ERROR_STRING);
        return CYE_INVALID_FILE_HANDLE;
    }

    return result;
#endif // PLATFORM_WINDOWS
}
void cye_file_close(Cye_File_Handle handle) {
#ifndef PLATFORM_WINDOWS
    close(handle);
#else
    CloseHandle(handle);
#endif // PLATFORM_WINDOWS
}

bool cye_process_wait_all(Cye_Process_DArray procs) {
    bool success = true;
    // TODO: Maybe sucess && fn() instead of fu() && sucess?
    for (usz i = 0; i < procs.count; ++i) {
        success = cye_process_wait(procs.items[i]) && success;
    }
    return success;
}

bool cye_process_wait_all_and_reset(Cye_Process_DArray *procs) {
    bool success = cye_process_wait_all(*procs);
    procs->count = 0;
    return success;
}

// Wait until the process has finished
bool cye_process_wait(Cye_Process proc) {

    if (proc == CYE_INVALID_PROCESS)  {
        return false;
    }

#ifndef PLATFORM_WINDOWS
    for (;;) {
        int wstatus = 0;
        if (waitpid(proc, &wstatus, 0) < 0) {
            cye_trace_log(CYE_TRACE_ERROR, "Could not wait on command (pid %d): %s", proc, CYE_GET_ERROR_STRING);
            return false;
        }

        if (WIFEXITED(wstatus)) {
            int exit_status = WEXITSTATUS(wstatus);
            if (exit_status != 0) {
                cye_trace_log(CYE_TRACE_ERROR, "Command exited with exit code %d", exit_status);
                return false;
            }
            break;
        }

        if (WIFSIGNALED(wstatus)) {
            cye_trace_log(CYE_TRACE_ERROR, "Command process was terminated by %s", strsignal(WTERMSIG(wstatus)));
            return false;
        }
    }
    return true;
#else
    DWORD result = WaitForSingleObject(
        proc,    // HANDLE hHandle,
        INFINITE // DWORD  dwMilliseconds
    );

    if (result == WAIT_FAILED) {
        cye_trace_log(CYE_TRACE_ERROR, "Could not wait on child process: %s", win32_error_message(GetLastError()));
        return false;
    }

    DWORD exit_status;
    if (!GetExitCodeProcess(proc, &exit_status)) {
        cye_trace_log(CYE_TRACE_ERROR, "Could not get process exit code: %s", win32_error_message(GetLastError()));
        return false;
    }

    if (exit_status != 0) {
        cye_trace_log(CYE_TRACE_ERROR, "Command exited with exit code %lu", exit_status);
        return false;
    }

    CloseHandle(proc);

    return true;
#endif
}

//------------------------------------------------------------------------------------
//  Commands Functions Implementation
//------------------------------------------------------------------------------------
void cye_ds_write_cmd(Cye_DString *ds, Cye_Command cmd) {
    for (usz i = 0; i < cmd.count; ++i) {
        ZString arg = cmd.items[i];
        if (arg == NULL) break;
        if (i > 0) cye_ds_write_zstr(ds, " ");
        if (!strchr(arg, ' ')) {
            cye_ds_write_zstr(ds, arg);
        } else {
            cye_da_append(ds, '\'');
            cye_ds_write_zstr(ds, arg);
            cye_da_append(ds, '\'');
        }
    }
}

// Run redirected command asynchronously
Cye_Process cye_cmd_run_async_and_reset(Cye_Command *cmd) {
    Cye_Process proc = cye_cmd_run_async(*cmd);
    cmd->count = 0;
    return proc;
}


// Run redirected command asynchronously and reset count
Cye_Process cye_cmd_run_async_redirect(Cye_Command cmd, Cye_Command_Redirect redirect) {
    if (cmd.count < 1) {
        cye_trace_error("Could not run empty command");
        return CYE_INVALID_PROCESS;
    }

    Cye_DString sb = {0};
    cye_ds_write_cmd(&sb, cmd);
    cye_ds_write_zero(&sb);
    cye_trace_info("CMD: %s", sb.items);
    cye_ds_free(sb);
    memset(&sb, 0, sizeof(sb));

#if !defined(PLATFORM_WINDOWS) // Unix
    pid_t cpid = fork();
    if (cpid < 0) {
        cye_trace_error("Could not fork child process: %s", CYE_GET_ERROR_STRING);
        return CYE_INVALID_PROCESS;
    }

    if (cpid == 0) {
        if (redirect.in) {
            if (dup2(*redirect.in, STDIN_FILENO) < 0) {
                cye_trace_error("Could not setup stdin for child process: %s", CYE_GET_ERROR_STRING);
                exit(1);
            }
        }

        if (redirect.out) {
            if (dup2(*redirect.out, STDOUT_FILENO) < 0) {
                cye_trace_error("Could not setup stdout for child process: %s", CYE_GET_ERROR_STRING);
                exit(1);
            }
        }

        if (redirect.err) {
            if (dup2(*redirect.err, STDERR_FILENO) < 0) {
                cye_trace_error("Could not setup stderr for child process: %s", CYE_GET_ERROR_STRING);
                exit(1);
            }
        }

        // NOTE: This leaks a bit of memory in the child process.
        // But do we actually care? It's a one off leak anyway...
        Cye_Command cmd_null = {0};
        cye_da_append_buf(&cmd_null, cmd.items, cmd.count);
        cye_cmd_append(&cmd_null, NULL);

        if (execvp(cmd.items[0], (char * const*) cmd_null.items) < 0) {
            cye_trace_error("Could not exec child process: %s", CYE_GET_ERROR_STRING);
            exit(1);
        }
        cye_unreachable("cmd_run_async_redirect");
    }

    return cpid;
#else
    // https://docs.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output

    STARTUPINFO siStartInfo;
    ZeroMemory(&siStartInfo, sizeof(siStartInfo));
    siStartInfo.cb = sizeof(STARTUPINFO);
    // NOTE: theoretically setting NULL to std handles should not be a problem
    // https://docs.microsoft.com/en-us/windows/console/getstdhandle?redirectedfrom=MSDN#attachdetach-behavior
    // TODO: check for errors in GetStdHandle
    siStartInfo.hStdError = redirect.err ? *redirect.err : GetStdHandle(STD_ERROR_HANDLE);
    siStartInfo.hStdOutput = redirect.out ? *redirect.out : GetStdHandle(STD_OUTPUT_HANDLE);
    siStartInfo.hStdInput = redirect.in ? *redirect.in : GetStdHandle(STD_INPUT_HANDLE);
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION piProcInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // TODO: use a more reliable rendering of the command instead of cmd_render
    // cmd_render is for logging primarily
    cye_ds_write_cmd(&sb, cmd);
    cye_ds_write_zero(&sb);
    BOOL bSuccess = CreateProcessA(NULL, sb.items, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);
    cye_ds_free(sb);

    if (!bSuccess) {
        cye_trace_error("Could not create child process: %s", win32_error_message(GetLastError()));
        return CYE_INVALID_PROCESS;
    }

    CloseHandle(piProcInfo.hThread);

    return piProcInfo.hProcess;
#endif
}

// Run redirected command asynchronously and set cmd.count to 0 and close all the opened files
Cye_Process cye_cmd_run_async_redirect_and_reset(Cye_Command *cmd, Cye_Command_Redirect redirect) {
    Cye_Process proc = cye_cmd_run_async_redirect(*cmd, redirect);
    cmd->count = 0;
    if (redirect.in) {
        cye_file_close(*redirect.in);
        *redirect.in = CYE_INVALID_FILE_HANDLE;
    }
    if (redirect.out) {
        cye_file_close(*redirect.out);
        *redirect.out = CYE_INVALID_FILE_HANDLE;
    }
    if (redirect.err) {
        cye_file_close(*redirect.err);
        *redirect.err = CYE_INVALID_FILE_HANDLE;
    }
    return proc;
}

// Run command synchronously
bool cye_cmd_run_sync(Cye_Command cmd) {
    Cye_Process proc = cye_cmd_run_async(cmd);
    if (proc == CYE_INVALID_PROCESS) {
        return false;
    }
    return cye_process_wait(proc);
}

bool cye_cmd_run_sync_and_reset(Cye_Command *cmd) {
    bool ok = cye_cmd_run_sync(*cmd);
    cmd->count = 0;
    return ok;
}

// Run redirected command synchronously
bool cye_cmd_run_sync_redirect(Cye_Command cmd, Cye_Command_Redirect redirect) {
    Cye_Process p = cye_cmd_run_async_redirect(cmd, redirect);
    if (p == CYE_INVALID_PROCESS) {
        return false;
    }
    return cye_process_wait(p);
}

// Run redirected command synchronously and set cmd.count to 0 and close all the opened files
bool cye_cmd_run_sync_redirect_and_reset(Cye_Command *cmd, Cye_Command_Redirect redirect) {

    bool ok = cye_cmd_run_sync_redirect(*cmd, redirect);
    cmd->count = 0;
    if (redirect.in) {
        cye_file_close(*redirect.in);
        *redirect.in = CYE_INVALID_FILE_HANDLE;
    }
    if (redirect.out) {
        cye_file_close(*redirect.out);
        *redirect.out = CYE_INVALID_FILE_HANDLE;
    }
    if (redirect.err) {
        cye_file_close(*redirect.err);
        *redirect.err = CYE_INVALID_FILE_HANDLE;
    }
    return ok;
}

// Run a command synchronously and capture its stdout and stderr output
// Returns true on success, false on failure
bool cye_cmd_run_sync_capture_and_reset(Cye_Command* cmd, Cye_Capture_Result* result) {
    bool success = false;
    Cye_Pipe pipe_out = CYE_INVALID_PIPE;
    Cye_Pipe pipe_err = CYE_INVALID_PIPE;

    // Create pipes for stdout and stderr
    pipe_out = cye_pipe_open();
    if (!cye_is_pipe_valid(pipe_out)) {
        goto cleanup;
    }

    pipe_err = cye_pipe_open();
    if (!cye_is_pipe_valid(pipe_err)) {
        goto cleanup;
    }

    // Run the command with redirected output
    Cye_Process p = cye_cmd_run_async_redirect_and_reset(
        cmd,
        (Cye_Command_Redirect){
            .out = &pipe_out.write,
            .err = &pipe_err.write
        }
    );

    if (p == CYE_INVALID_PROCESS) {
        goto cleanup;
    }

    // Close write ends after starting the process
    cye_pipe_close_handle(&pipe_out.write);
    cye_pipe_close_handle(&pipe_err.write);

    // Read from both pipes
    char buffer[1024];
    usz bytes_read;

    // Read from stderr
    while (cye_pipe_read(pipe_err.read, buffer, sizeof(buffer) - 1, &bytes_read) && bytes_read > 0) {
        cye_ds_write_buf(&result->err, buffer, bytes_read);
    }
    cye_ds_write_zero(&result->err);

    // Read from stdout
    while (cye_pipe_read(pipe_out.read, buffer, sizeof(buffer) - 1, &bytes_read) && bytes_read > 0) {
        cye_ds_write_buf(&result->out, buffer, bytes_read);
    }
    cye_ds_write_zero(&result->out);

    // Wait for process completion
    success = cye_process_wait(p);

cleanup:
    if (cye_is_pipe_valid(pipe_out)) {
        cye_pipe_close(pipe_out);
    }
    if (cye_is_pipe_valid(pipe_err)) {
        cye_pipe_close(pipe_err);
    }

    return success;
}


// The implementation idea is stolen from https://github.com/zhiayang/nabs
void cye__rebuild_ourselves(ZString source_path, int argc, ZString *argv) {
    ZString binary_path = cye_shift(argv, argc);
#ifdef PLATFORM_WINDOWS
    if (!cye_zstr_ends_with(binary_path, ".exe")) {
        binary_path = cye_tprintf("%s.exe", binary_path);
    }
#endif

    int rebuild_is_needed = cye_needs_rebuild(binary_path, source_path, __FILE__);
    if (rebuild_is_needed < 0) {
        exit(1);
    }
    if (!rebuild_is_needed) {
        return;
    }

    Cye_Command cmd = {0};

    ZString old_binary_path = cye_tprintf("%s.old", binary_path);

    if (!cye_path_rename(binary_path, old_binary_path)) {
        exit(1);
    }

    cye_cmd_append(&cmd, cye_rebuild_command(binary_path, source_path));
    if (!cye_cmd_run_sync_and_reset(&cmd)) {
        cye_path_rename(old_binary_path, binary_path);
        exit(1);
    }

    cye_cmd_append(&cmd, binary_path);
    cye_da_append_buf(&cmd, argv, argc);
    if (!cye_cmd_run_sync_and_reset(&cmd)) {
        exit(1);
    }

    exit(0);
}



//------------------------------------------------------------------------------------
//  Storage Functions Implementation
//------------------------------------------------------------------------------------

Cye_Context cye_temp_context(void) {
    // Gets whatever was in the cye_context.any
    return cliteral(Cye_Context){.alloc = cye_talloc, .realloc = cye_trealloc, .free = cye_tfree, cye_context.any};
}

Cye_Context cye_default_context(void) {
    return cliteral(Cye_Context){.alloc = cye_malloc, .realloc = cye_realloc, .free = cye_free, .any=null};
}

u0 cye_set_default_context(Cye_Context ctx) {
    unused(ctx);
    cye_panic("YAY");
}

TString cye_tstrdup(ZString cstr) {
    usz n = strlen(cstr);
    TString result = (TString)cye_talloc(n + 1);
    cye_assert_msg(result != NULL, "Please increase CYE_TEMP_CAPACITY (%zu bytes)", CYE_TEMP_CAPACITY);
    memcpy(result, cstr, n);
    result[n] = '\0';
    return result;
}


// TODO: Check out arena allocator
rawptr cye_talloc(usz size) {
    if (cye_temp_data.size + size > CYE_TEMP_CAPACITY) return NULL;
    rawptr result = &cye_temp_data.buffer[cye_temp_data.size];
    cye_temp_data.last  = result;
    cye_temp_data.size += size;
    return result;
}

void *cye_trealloc(void *ptr, usz size) {
    if (ptr == null) {
        // `talloc` already sets the last pointer
        return cye_talloc(size);
    }

    if (size == 0) {
        return null;
    }
    // If ptr is NULL or it's not from our temp buffer, just do a new allocation
    if ((byte*)ptr <   cye_temp_data.buffer
     || (byte*)ptr >= (cye_temp_data.buffer + CYE_TEMP_CAPACITY))
    {
        cye_trace_fatal(
            "Trying to realloc investigate this behaviour"
            "You might have allocate with one context than changed the context and reallocated with something else"
            "This might indicate that you need to either note realloc instead to the memcpy your self since you probably already"
            "know how much data the pointer points to. (temporary allocator does not)"
        );
        cye_unreachable("FATAL"); // Maybe it shouldn't be? But good for me to find bugs

        // `talloc` already sets the last pointer
        return cye_talloc(size);;
    }

    // Check if ptr is the last allocation by seeing if it points to
    // the position right after our previous allocations
    if (ptr == cye_temp_data.last) {
        cye_trace_log(CYE_LOG_TRACE, "Reallocation done on pointer from last allocation");
        // Make sure we don't exceed buffer capacity
        if ((byte*)ptr + size > cye_temp_data.buffer + CYE_TEMP_CAPACITY) {
            cye_trace_log(CYE_LOG_WARNING, "Reallocation returns exceeds CYE_TEMP_CAPACITY=%zu", CYE_TEMP_CAPACITY);
            return NULL;
        }

        // Adjust total size
        cye_temp_data.size = ((byte*)ptr - cye_temp_data.buffer) + size;
        return ptr;
    }

    // Otherwise, allocate new space. `talloc` already sets the last pointer
    void *new_ptr = cye_talloc(size);
    cye_trace_log(CYE_LOG_TRACE, "Reallocation done from different pointer from last allocation. (pointer=%p != last_pointer=%p)(new_ptr=%p)", ptr, cye_temp_data.last, new_ptr);
    if (new_ptr) {
        // Calculate how much data we can safely copy, techinacally should be all of it
        // Because talloc doesn't let us allocate partially, but whatever
        usz remaining_space = cye_temp_data.buffer + CYE_TEMP_CAPACITY - (byte*)ptr;
        usz copy_size = size;
        if (size > remaining_space) {
            copy_size = remaining_space;
            cye_trace_warn("Wanted %zu bytes but can only give %zu to not exceed %d CYE_TEMP_CAPACITY", size, remaining_space, CYE_TEMP_CAPACITY);
        }
        memcpy(new_ptr, ptr, copy_size);
    }

    return new_ptr;
}

void cye_tfree(rawptr ptr) {
    cye_trace_log(CYE_LOG_TRACE, "Temporary allocator freed");
}

// TODO: Function to generate default Wanings for each compiler maybe, and output binary path?
TString cye_tprintf(ZString fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);


    cye_assert(n >= 0);
    char *result = cye_talloc(n + 1);

    cye_assert(result != NULL && "Extend the size of the temporary allocator");

    // TODO: use proper arenas for the temporary allocator;
    va_start(args, fmt);
    vsnprintf(result, n + 1, fmt, args);
    va_end(args);

    return result;
}

void  cye_temp_reset(void) {
    cye_temp_data.size = 0;
}

usz cye_temp_save(void) {
    return cye_temp_data.size;
}

void cye_temp_restore(usz checkpoint) {
    cye_temp_data.size = checkpoint;
}


//------------------------------------------------------------------------------------
//  Path Functions Implementation
//------------------------------------------------------------------------------------

bool cye_make_dir_if_not_exists(ZString path) {
#ifdef PLATFORM_WINDOWS
    int result = mkdir(path);
#else
    int result = mkdir(path, 0755);
#endif
    if (result < 0) {
        if (errno == EEXIST) {
            cye_trace_info("directory `%s` already exists", path);
            return true;
        }
        cye_trace_error("could not create directory `%s`: %s", path, CYE_GET_ERROR_STRING);
        return false;
    }

    cye_trace_info("created directory `%s`", path);
    return true;
}

bool cye_copy_file(ZString src_path, ZString dst_path) {
    cye_trace_info("Copying %s -> %s", src_path, dst_path);
#ifndef PLATFORM_WINDOWS
    int src_fd = -1;
    int dst_fd = -1;
    usz buf_size = 32*1024;
    char *buf = cye_context.realloc(NULL, buf_size);
    cye_assert(buf != NULL && "RAM not enough");
    bool result = true;

    src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        cye_trace_error("Could not open file %s: %s", src_path, strerror(errno));
        cye_result_defer(false);
    }

    struct stat src_stat;
    if (fstat(src_fd, &src_stat) < 0) {
        cye_trace_error("Could not get mode of file %s: %s", src_path, strerror(errno));
        cye_result_defer(false);
    }

    dst_fd = open(dst_path, O_CREAT | O_TRUNC | O_WRONLY, src_stat.st_mode);
    if (dst_fd < 0) {
        cye_trace_error("Could not create file %s: %s", dst_path, strerror(errno));
        cye_result_defer(false);
    }

    for (;;) {
        isz n = read(src_fd, buf, buf_size);
        if (n == 0) break;
        if (n < 0) {
            cye_trace_error("Could not read from file %s: %s", src_path, strerror(errno));
            cye_result_defer(false);
        }
        char *buf2 = buf;
        while (n > 0) {
            isz m = write(dst_fd, buf2, n);
            if (m < 0) {
                cye_trace_error("Could not write to file %s: %s", dst_path, strerror(errno));
                cye_result_defer(false);
            }
            n    -= m;
            buf2 += m;
        }
    }

defer:
    free(buf);
    close(src_fd);
    close(dst_fd);
    return result;
#else

    if (!CopyFile(src_path, dst_path, FALSE)) {
        cye_trace_error("Could not copy file: %s", win32_error_message(GetLastError()));
        return false;
    }
    return true;
#endif
}


bool cye_copy_dir(ZString src_path, ZString dst_path) {
    static int depth = 0;

    depth += 1;

    bool result = true;
    Cye_Path_DArray children = {0};
    Cye_DString src_ds = {0};
    Cye_DString dst_ds = {0};
    usz temp_checkpoint = cye_temp_save();

    Cye_File_Kind type = cye_path_file_kind(src_path);
    if (type < 0) {
        depth -= 1;
        return false;
    }

    switch (type) {
        case CYE_FILE_KIND_DIRECTORY: {
            if (!cye_make_dirs(dst_path)) cye_result_defer(false);
            if (!cye_read_dir(src_path, &children)) cye_result_defer(false);

            for (usz i = 0; i < children.count; ++i) {
                if (strcmp(children.items[i], ".") == 0) continue;
                if (strcmp(children.items[i], "..") == 0) continue;

                src_ds.count = 0;
                cye_ds_write(&src_ds, src_path, PATH_SEPARATOR, children.items[i]);
                cye_ds_write_zero(&src_ds);

                dst_ds.count = 0;
                cye_ds_write(&dst_ds, dst_path, PATH_SEPARATOR, children.items[i]);
                cye_ds_write_zero(&dst_ds);
                if (!cye_copy_dir(src_ds.items, dst_ds.items)) {
                    cye_result_defer(false);
                }
            }
        } break;

        case CYE_FILE_KIND_REGULAR: {
            Cye_Log_Level old_level = cye_threshold_log_level;
            cye_set_trace_level(CYE_LOG_TRACE);
            bool copy_result = cye_copy_file(src_path, dst_path);
            cye_set_trace_level(old_level);

            if (!copy_result) {
                cye_set_trace_level(old_level);
                cye_result_defer(false);
            }
        } break;

        case CYE_FILE_KIND_SYMLINK: {
            cye_trace_warn("TODO: Copying symlinks is not supported yet");
        } break;

        case CYE_FILE_KIND_OTHER: {
            cye_trace_error("Unsupported type of file %s", src_path);
            cye_result_defer(false);
        } break;

        default: cye_unreachable("copy_directory_recursively");
    }

defer:
    cye_temp_restore(temp_checkpoint);
    cye_da_free(src_ds);
    cye_da_free(dst_ds);
    cye_da_free(children);
    depth -= 1;

    cye_trace_log(CYE_LOG_TRACE, "%s, depth=%d", __func__, depth);
    if (depth == 0) {
        if (result) {
            cye_trace_info("Copied directory `%s` into `%s` successfully.", src_path, dst_path);
        } else {
            cye_trace_info("Failed to copied directory `%s` into `%s`.", src_path, dst_path);
        }
    }
    return result;
}


bool cye_read_dir_filtered(
    ZString parent, Cye_Path_DArray *children,
    bool use_parent, Cye_File_Filter filter, void *user_data
) {
    cye_assert(parent);
    bool result = true;

    //
    // We Might blow the the stack with this (`char[PATH_MAX+1]`).
    // In case of recursive calls like glob functions
    // but sure, we can linearize those, although it'd be slower
    // because of two loops instead of one (in the way that I'm thinking),
    // One loop to get the `read_dir_filtered` then another loop to see
    // if any files inside children are directories to finally call
    // `read_dir_filtered` on those again
    //
    char full_path[PATH_MAX+1];

#ifndef PLATFORM_WINDOWS // On Unix
    DIR *dir = NULL;

    dir = opendir(parent);
    if (dir == NULL) {
        cye_result_defer(false);
        cye_trace_error("Could not open directory %s: %s", parent, CYE_GET_ERROR_STRING);
    }

    errno = 0;
    struct dirent *ent = readdir(dir);
    while (ent != NULL) {
        ZString path = ent->d_name;
        if (use_parent) {
            strcpy(full_path, parent);
            if(!cye_zstr_ends_with(full_path, PATH_SEPARATOR)) {
                strcat(full_path, PATH_SEPARATOR);
            }
            strcat(full_path, path);
            path = full_path;
        }
        // Apply only filter if provided, otherwise just append anyways
        if (filter == NULL || filter(path, user_data)) {
            cye_da_append(children, cye_tstrdup(path));
        }

        *full_path = '\0'; // Just to be sure
        ent = readdir(dir);
    }

    if (errno != 0) {
        cye_trace_error("Could not read directory %s: %s", parent, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

defer:
    if (dir) {
        closedir(dir);
    }
    return result;

#else // On Windows
    WIN32_FIND_DATAA find_data;
    HANDLE find_handle = INVALID_HANDLE_VALUE;
    char search_path[MAX_PATH];

    // Prepare search path with wildcard
    if (snprintf(search_path, sizeof(search_path), "%s\\*", parent) >= (signed int)sizeof(search_path)) {
        cye_trace_error("Path too long: %s", parent);
        cye_result_defer(false);
    }

    // Start file search
    find_handle = FindFirstFileA(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        cye_trace_error("Could not open directory %s: %s", parent, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

    // Read all entries
    do {
        ZString path = find_data.cFileName;
        if (use_parent) {
            strcpy(full_path, parent);
            if(!cye_zstr_ends_with(full_path, PATH_SEPARATOR)) {
                strcat(full_path, PATH_SEPARATOR);
            }
            strcat(full_path, path);
            path = full_path;
        }

        // Apply only filter if provided, otherwise just append anyways
        if (filter == NULL || filter(path, user_data)) {
            cye_da_append(children, cye_tstrdup(path));
        }
        // cye_da_append(children, cye_tstrdup(find_data.cFileName));
    } while (FindNextFileA(find_handle, &find_data));

    // Check if we stopped due to an error
    DWORD last_error = GetLastError();
    if (last_error != ERROR_NO_MORE_FILES) {
        cye_trace_error("Could not read directory %s: %s", parent, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

defer:
    if (find_handle != INVALID_HANDLE_VALUE) {
        FindClose(find_handle);
    }
    return result;
#endif
}


// Append data to the end of file
bool cye_append_file(const char* path, const void* data, usz count) {
    bool result = true;

#ifndef PLATFORM_WINDOWS
    int fd = -1;
    isz bytes_written = 0;

    // Open file for append
    fd = open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        cye_trace_error("Could not open file %s for append: %s", path, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

    // Write the data
    bytes_written = write(fd, data, count);
    if (bytes_written == -1 || (usz)bytes_written != count) {
        cye_trace_error("Could not write to file %s: %s", path, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

#else
    HANDLE file_handle = INVALID_HANDLE_VALUE;
    DWORD bytes_written = 0;

    // Open file for append
    file_handle = CreateFileA(
        path,                     // path
        FILE_APPEND_DATA,         // access mode (append only)
        FILE_SHARE_READ,          // share mode
        NULL,                     // security attributes
        OPEN_ALWAYS,              // create if not exists
        FILE_ATTRIBUTE_NORMAL,    // file attributes
        NULL                      // template file
    );

    if (file_handle == INVALID_HANDLE_VALUE) {
        cye_trace_error("Could not open file %s for append: %s", path, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

    // Move file pointer to end (should be redundant with FILE_APPEND_DATA, but being thorough)
    if (SetFilePointer(file_handle, 0, NULL, FILE_END) == INVALID_SET_FILE_POINTER) {
        cye_trace_error("Could not seek to end of file %s: %s", path, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

    // Write the data
    if (!WriteFile(file_handle, data, (DWORD)count, &bytes_written, NULL) || bytes_written != count) {
        cye_trace_error("Could not write to file %s: %s", path, CYE_GET_ERROR_STRING);
        cye_result_defer(false);
    }

#endif

defer:

#ifndef PLATFORM_WINDOWS
    if (fd != -1) {
        close(fd);
    }
#else
    if (file_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(file_handle);
    }
#endif

    return result;
}

// Convenience function for appending strings
bool cye_append_file_zstr(const char* path, const char* str) {
    return cye_append_file(path, str, strlen(str));
}


// TODO: Check this for windows
bool cye_write_file(ZString path, const void *data, usz size) {
    bool result = true;

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        cye_trace_error("Could not open file %s for writing: %s\n", path, strerror(errno));
        cye_result_defer(false);
    }

    //           len
    //           v
    // aaaaaaaaaa
    //     ^
    //     data

    ZString buf = data;
    while (size > 0) {
        usz n = fwrite(buf, 1, size, f);
        if (ferror(f)) {
            cye_trace_error("Could not write into file %s: %s\n", path, strerror(errno));
            cye_result_defer(false);
        }
        size -= n;
        buf  += n;
    }

defer:
    if (f) fclose(f);
    return result;
}

#if 0 && defined(TODO_kindly_why_does_this_work_with_mingw)
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
#endif



// TODO: Check this for cl.exe it
#if defined(PLATFORM_MINGW) || defined(PLATFORM_LINUX)
char* cye_read_file(const char* path) {
    int file = open(path, O_RDONLY);
    if (file < 0) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    off_t file_size = lseek(file, 0, SEEK_END);
    lseek(file, 0, SEEK_SET);

    // Read the file content into a buffer
    char* data = malloc(file_size + 1);
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

#elif defined(PLATFORM_WINDOWS)

u8* cye_read_file(const char* path) {
    HANDLE file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return NULL;
    }

    DWORD file_size = GetFileSize(file, NULL);
    if (file_size == INVALID_FILE_SIZE) {
        fprintf(stderr, "Failed to get file size: %s\n", path);
        CloseHandle(file);
        return NULL;
    }

    u8* data = (u8*)malloc(file_size + 1);
    if (data == NULL) {
        fprintf(stderr, "Failed to allocate memory for shader source\n");
        CloseHandle(file);
        return NULL;
    }

    DWORD bytes_read;
    if (!ReadFile(file, data, file_size, &bytes_read, NULL) || bytes_read != file_size) {
        fprintf(stderr, "Failed to read file: %s\n", path);
        free(data);
        CloseHandle(file);
        return NULL;
    }

    data[file_size] = '\0'; // Null-terminate the string
    CloseHandle(file);
    return data;
}
#endif

// TODO: Check this for windows
bool cye_ds_read_file(ZString path, Cye_DString *ds) {
    bool result = true;

    FILE *f = fopen(path, "rb");
    if (f == NULL)                 cye_result_defer(false);
    if (fseek(f, 0, SEEK_END) < 0) cye_result_defer(false);
    long m = ftell(f);
    if (m < 0)                     cye_result_defer(false);
    if (fseek(f, 0, SEEK_SET) < 0) cye_result_defer(false);

    usz new_count = ds->count + m;
    if (new_count > ds->capacity) {
        ds->items = cye_context.realloc(ds->items, new_count);
        cye_assert(ds->items != NULL && "Please, you'll need to acquire more random access memory ");
        ds->capacity = new_count;
    }

    fread(ds->items + ds->count, m, 1, f);
    // If no error has occurred on stream, ferror return 0
    int error_value = ferror(f);
    if (error_value != 0) {
        cye_trace_error("Could not read file %s: ferror error value is %d", path, error_value);
        result = false;
        goto close;
    }
    ds->count = new_count;

defer:
    if (!result) cye_trace_error("Could not read file %s: %s", path, strerror(errno));
close:
    if (f) fclose(f);
    return result;
}

Cye_File_Kind cye_path_file_kind(ZString path) {
#ifndef PLATFORM_WINDOWS
    struct stat statbuf;
    if (stat(path, &statbuf) < 0) {
        cye_trace_error("Could not get stat of %s: %s", path, strerror(errno));
        return -1;
    }

    switch (statbuf.st_mode & S_IFMT) {
        case S_IFDIR:  return CYE_FILE_KIND_DIRECTORY;
        case S_IFREG:  return CYE_FILE_KIND_REGULAR;
        case S_IFLNK:  return CYE_FILE_KIND_SYMLINK;
        default:       return CYE_FILE_KIND_OTHER;
    }
#else // PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        cye_trace_error("Could not get file attributes of %s: %lu", path, GetLastError());
        return -1;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) return CYE_FILE_KIND_DIRECTORY;
    // TODO: detect symlinks on Windows (whatever that means on Windows anyway)
    return CYE_FILE_KIND_REGULAR;
#endif // PLATFORM_WINDOWS
}

char* cye_path_temp_normalize(ZString path) {
    // 1 extra for the path separator in the end and another 1 byte for null terminator
    usz path_count  = strlen(path);
    usz total_count = path_count + 1 + 1;

    // Allocate memory for the final path
    Cye_DString ds = {
        .items = cye_talloc(total_count),
        .count = 0,
        .capacity = total_count
    };

    // Removing repeated separators
    for (usz idx = 0; idx < path_count; ++idx) {

        bool is_next_end = (idx + 1) == (path_count);
        bool is_prev_sep = ds.count > 0 && (ds.items[ds.count-1] == PATH_SEPARATOR_CHAR);
        bool is_next_sep = ((idx + 1) < path_count) && (path[idx + 1] == PATH_SEPARATOR_CHAR);
        bool is_curr_dot = path[idx] ==  '.';

        if (is_prev_sep && (is_next_end || is_next_sep) && is_curr_dot) {
            idx += 1;
            continue;
        }

        bool is_curr_sep = path[idx] == PATH_SEPARATOR_CHAR;
        if (!(is_curr_sep && is_prev_sep)) {
            cye_ds_write_char(&ds, path[idx]);
        }
    }

    // Special .. must end with trailing PATH_SEP, we must have
    if (ds.count >= 2
        && ('.' == ds.items[ds.count-1])
        && ('.' == ds.items[ds.count-2]))
    {
        // 2th case: Don't need to check for >= 3 and it fails in ds.count == 2
        if (ds.count == 2 || PATH_SEPARATOR_CHAR == ds.items[ds.count-3]) {
            cye_ds_write(&ds, PATH_SEPARATOR);
        }
    } else if (ds.count == 1 && '.' == ds.items[ds.count-1]) {
        cye_ds_write(&ds, PATH_SEPARATOR);
    }

    cye_ds_write_zero(&ds);

    // Should have been an upperbound on allocated memory, it should never have grown
    if (ds.capacity > total_count) {
        cye_trace_error(
            "Allocating memory for the dynamic string is an error path=%s total_count=%zu ds="cye_ds_fmt".\n"
            "All memory should have been talloc",
            path,
            total_count,
            cye_ds_fmt_arg(ds)
        );
        cye_panic();
    }

    return ds.items;
}

char* cye_path_create_from_array(ZString paths[], usz paths_count) {
    Cye_Context ctx = cye_context;

    usz total_count = 0;
    usz traling_empty_count = 0;
    for (usz i = 0; i < paths_count; i++) {
        usz len = strlen(paths[i]);
        total_count += len;
        if (len == 0) {
            traling_empty_count += 1;
        } else {
            traling_empty_count = 0;
        }
        cye_trace_log(CYE_LOG_TRACE, "path[%d/%d] = %s", i, paths_count-1, paths[i]);
    }

    paths_count = paths_count - traling_empty_count;

    // Allocate memory for the final path with context, so user can decide where to allocate this
    Cye_DString ds = {
        .items = ctx.alloc(total_count + paths_count + 1),
        .count = 0,
        .capacity = total_count + paths_count + 1
    };

    // Concatenate the paths
    for (usz i = 0; i < paths_count; i++) {
        cye_ds_write(&ds, paths[i]);
        if (i < (paths_count-1) && ds.count > 0 && (ds.items[ds.count-1] != PATH_SEPARATOR_CHAR)) {
            cye_ds_write(&ds, PATH_SEPARATOR);
        }
    }
    cye_ds_write_zero(&ds);

    {
        usz chk_point = cye_temp_save();
        TString tpath = cye_path_temp_normalize(ds.items);
        // `strncpy` doesn't consider '\0'. It'd be nice to consider both `n` and char `'\0'`.
        strcpy(ds.items, tpath);
        cye_temp_restore(chk_point);
    }

    return ds.items;
}

ZString cye_path_base_name(ZString path) {
#ifndef PLATFORM_WINDOWS
    ZString p = strrchr(path, '/');
    return p ? p + 1 : path;
#else
    ZString p1 = strrchr(path, '/');
    ZString p2 = strrchr(path, '\\');
    ZString p =
        (p1 > p2) ? p1
                  : p2;  // NULL is ignored if the other search is successful
    return p ? p + 1 : path;
#endif  // PLATFORM_WINDOWS
}

// Expand ~ and ~user to full home directory path
// @Leak: maybe make temp? or provide the DString to write to
// @Check: Sanity check every thing
ZString cye_path_expand_user(ZString path) {
    if (!path || path[0] != '~') return path;

    Cye_DString result = {0};
    usz path_len = strlen(path);

#ifdef PLATFORM_WINDOWS
    // On Windows, we'll only handle plain ~ (no ~user support)
    if (path[1] != '\0' && path[1] != '/' && path[1] != '\\') {
        return path;
    }

    char home_path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, home_path))) {
        cye_ds_write(&result, home_path);

        // Add the rest of the path (skip the ~)
        if (path[1] != '\0') {
            // If path uses forward slashes, convert home_path backslashes to forward slashes
            if (strchr(path, '/')) {
                for (char* p = result.items; *p; p++) {
                    if (*p == '\\') *p = '/';
                }
            }
            cye_ds_write(&result, path + 1);
        }

        return result.items;
    }

    // Fallback to USERPROFILE environment variable
    const char* user_profile = getenv("USERPROFILE");
    if (user_profile) {
        cye_ds_write(&result, user_profile);
        if (path[1] != '\0') {
            cye_ds_write(&result, path + 1);
        }
        return result.items;
    }
#else
    // Find the end of the username or ~ if no username
    const char* path_separator = strchr(path, '/');
    usz username_len = path_separator ? (usz)(path_separator - path - 1) :
                         (path_len > 1 ? path_len - 1 : 0);

    const char* home_dir = NULL;

    if (username_len == 0) {
        // Plain ~ - use current user's home
        home_dir = getenv("HOME");
        if (!home_dir) {
            // Fallback to password database
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                home_dir = pw->pw_dir;
            }
        }
    } else {
        // ~user - look up user in password database
        char username[256];  // Reasonable max username length
        if (username_len >= sizeof(username)) {
            return path;  // Username too long
        }
        memcpy(username, path + 1, username_len);
        username[username_len] = '\0';

        struct passwd* pw = getpwnam(username);
        if (pw) {
            home_dir = pw->pw_dir;
        }
    }

    if (home_dir) {
        cye_ds_write(&result, home_dir);
        if (path_separator) {
            cye_ds_write(&result, path_separator);
        }
        return result.items;
    }
#endif

    // If all expansion attempts failed, return original path
    return path;
}

ZString cye_path_expand_vars(ZString path) { cye_panic("TODO");}

int cye_needs_rebuild_from_buf(ZString output_path, ZString *input_paths, usz input_paths_count) {
#if !defined(PLATFORM_WINDOWS)
    struct stat statbuf = {0};

    if (stat(output_path, &statbuf) < 0) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (errno == ENOENT) return 1;
        cye_trace_log(CYE_TRACE_ERROR, "could not stat %s: %s", output_path, CYE_GET_ERROR_STRING);
        return -1;
    }
    int output_path_time = statbuf.st_mtime;

    for (usz i = 0; i < input_paths_count; ++i) {
        ZString input_path = input_paths[i];
        if (stat(input_path, &statbuf) < 0) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            cye_trace_log(CYE_TRACE_ERROR, "could not stat %s: %s", input_path, CYE_GET_ERROR_STRING);
            return -1;
        }
        int input_path_time = statbuf.st_mtime;
        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (input_path_time > output_path_time) return 1;
    }

    return 0;
#else

    BOOL bSuccess;
    HANDLE output_path_fd = CreateFile(output_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
    if (output_path_fd == INVALID_HANDLE_VALUE) {
        // NOTE: if output does not exist it 100% must be rebuilt
        if (GetLastError() == ERROR_FILE_NOT_FOUND) return 1;
        cye_trace_log(CYE_TRACE_ERROR, "Could not open file %s: %s", output_path, win32_error_message(GetLastError()));
        return -1;
    }
    FILETIME output_path_time;
    bSuccess = GetFileTime(output_path_fd, NULL, NULL, &output_path_time);
    CloseHandle(output_path_fd);
    if (!bSuccess) {
        cye_trace_log(CYE_TRACE_ERROR, "Could not get time of %s: %s", output_path, win32_error_message(GetLastError()));
        return -1;
    }

    for (usz i = 0; i < input_paths_count; ++i) {
        ZString input_path = input_paths[i];
        HANDLE input_path_fd = CreateFile(input_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
        if (input_path_fd == INVALID_HANDLE_VALUE) {
            // NOTE: non-existing input is an error cause it is needed for building in the first place
            cye_trace_log(CYE_TRACE_ERROR, "Could not open file %s: %s", input_path, win32_error_message(GetLastError()));
            return -1;
        }
        FILETIME input_path_time;
        bSuccess = GetFileTime(input_path_fd, NULL, NULL, &input_path_time);
        CloseHandle(input_path_fd);
        if (!bSuccess) {
            cye_trace_log(CYE_TRACE_ERROR, "Could not get time of %s: %s", input_path, win32_error_message(GetLastError()));
            return -1;
        }

        // NOTE: if even a single input_path is fresher than output_path that's 100% rebuild
        if (CompareFileTime(&input_path_time, &output_path_time) == 1) return 1;
    }

    return 0;
#endif
}


TString cye_path_temp_cwd(void) {
#ifndef PLATFORM_WINDOWS
    char *buffer = (char*) cye_talloc(PATH_MAX);
    if (getcwd(buffer, PATH_MAX) == NULL) {
        cye_trace_error("could not get current directory: %s", CYE_GET_ERROR_STRING);
        return NULL;
    }
    return buffer;
#else
    DWORD nBufferLength = GetCurrentDirectory(0, NULL);
    if (nBufferLength == 0) {
        cye_trace_error("could not get current directory: %s", win32_error_message(GetLastError()));
        return NULL;
    }

    char *buffer = (char*) cye_talloc(nBufferLength);
    if (GetCurrentDirectory(nBufferLength, buffer) == 0) {
        cye_trace_error("could not get current directory: %s", win32_error_message(GetLastError()));
        return NULL;
    }

    return buffer;
#endif // PLATFORM_WINDOWS
}

bool cye_path_set_cwd(ZString path) {
#ifndef PLATFORM_WINDOWS
    if (chdir(path) < 0) {
        cye_trace_error("could not set current directory to %s: %s", path, CYE_GET_ERROR_STRING);
        return false;
    }
    return true;
#else
    if (!SetCurrentDirectory(path)) {
        cye_trace_error("could not set current directory to %s: %s", path, win32_error_message(GetLastError()));
        return false;
    }
    return true;
#endif // PLATFORM_WINDOWS
}


b32 cye_file_exists(ZString file_path) {
#ifndef PLATFORM_WINDOWS
    struct stat statbuf;
    if (stat(file_path, &statbuf) < 0) {
        if (errno == ENOENT) return 0;
        cye_trace_error("Could not check if file %s exists: %s", file_path, strerror(errno));
        return -1;
    }
    return 1;
#else
    // TODO: distinguish between "does not exists" and other errors
    DWORD dwAttrib = GetFileAttributesA(file_path);
    return dwAttrib != INVALID_FILE_ATTRIBUTES;
#endif
}


bool cye_file_stats(const char* path, Cye_File_Stats* stats) {
#ifndef PLATFORM_WINDOWS
    struct stat st;
    if (stat(path, &st) != 0) {
        return false;
    }

    stats->created_at = st.st_ctime;
    stats->accessed_at = st.st_atime;
    stats->modified_at = st.st_mtime;
    stats->size_bytes = (usz)st.st_size;

    return true;
#else
    HANDLE file_handle = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILETIME created, accessed, modified;
    if (!GetFileTime(file_handle, &created, &accessed, &modified)) {
        CloseHandle(file_handle);
        return false;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(file_handle, &size)) {
        CloseHandle(file_handle);
        return false;
    }

    CloseHandle(file_handle);

    stats->created_at = ((ULARGE_INTEGER*)&created)->QuadPart / 10000000ULL - 11644473600ULL;
    stats->accessed_at = ((ULARGE_INTEGER*)&accessed)->QuadPart / 10000000ULL - 11644473600ULL;
    stats->modified_at = ((ULARGE_INTEGER*)&modified)->QuadPart / 10000000ULL - 11644473600ULL;
    stats->size_bytes = (usz)size.QuadPart;

    return true;
#endif
}

// Check if path is absolute
bool cye_is_absolute(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// Check if path is relative
bool cye_is_relative(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// Check if path  is regular  file
bool cye_is_file(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// Check if path  is directory
bool cye_is_dir(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// TODO: Move this into the library
// Check if a path exists and is executable
bool cye_is_executable(ZString path) {
#ifdef PLATFORM_WINDOWS
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return false;

    // Check if it's a directory
    if (attr & FILE_ATTRIBUTE_DIRECTORY) return false;

    return true;
#else
    struct stat st;
    if (stat(path, &st) != 0) return false;

    // Check if it's a regular file and has execute permission
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
#endif
}

#ifdef PLATFORM_WINDOWS
static const char* EXECUTABLE_EXTENSIONS[] = {".exe", ".com", ".bat", ".cmd"};
#endif


#ifdef PLATFORM_WINDOWS
// Windows-specific: check if string ends with any executable extension
static bool has_executable_extension(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return false;

    for (size_t i = 0; i < sizeof(EXECUTABLE_EXTENSIONS)/sizeof(EXECUTABLE_EXTENSIONS[0]); i++) {
        if (_stricmp(ext, EXECUTABLE_EXTENSIONS[i]) == 0) {
            return true;
        }
    }
    return false;
}
#endif

// Find executable in PATH or current directory
bool cye_find_executable(const char* name, Cye_DString* out_path) {
    if (!name || !out_path) return false;

    // If name contains any path separator, check it directly
    const char* path_sep =
#ifdef PLATFORM_WINDOWS
        strpbrk(name, "\\/");
#else
        strchr(name, '/');
#endif

    if (path_sep) {
        out_path->count = 0;
        cye_ds_write(out_path, name);
#ifdef PLATFORM_WINDOWS
        // On Windows, if no extension provided, try adding .exe
        if (!has_executable_extension(name)) {
            cye_ds_write(out_path, ".exe");
        }
#endif
        return cye_is_executable(out_path->items);
    }

    // Get PATH environment variable
    const char* path_env = getenv("PATH");
    // printf("$PATH=%s\n", path_env);
    if (!path_env) return false;

    Cye_DString path_copy = {0};
    cye_ds_write(&path_copy, path_env);

    // Try each directory in PATH
    char* dir = strtok(path_copy.items, ENV_SEPARATOR);
    while (dir) {
        cye_ds_clear(out_path);
        cye_ds_write(out_path, dir);
        if (out_path->count > 0 && out_path->items[out_path->count-1] != PATH_SEPARATOR_CHAR) {
            cye_ds_write(out_path, PATH_SEPARATOR);
        }

        cye_ds_write(out_path, name);
        cye_ds_write_zero(out_path);

#ifdef PLATFORM_WINDOWS
        // On Windows, try with and without .exe if no extension provided
        if (!has_executable_extension(name)) {
            // Try without extension first
            if (cye_is_executable(out_path->items)) {
                cye_ds_free(path_copy);
                return true;
            }
            // Try with .exe
            cye_ds_write(out_path, ".exe");
        }
#endif
        if (cye_is_executable(out_path->items)) {
            cye_ds_free(path_copy);
            return true;
        }

        dir = strtok(NULL, ENV_SEPARATOR);
    }

    cye_ds_free(path_copy);
    return false;
}

// Check if path is the directory `./` of `..`
bool cye_is_period_dir(ZString path) {
    if (path == NULL) {
        return false;
    }
    bool result = false;
    ZString path_suffix = strrchr(path,  PATH_SEPARATOR_CHAR);

    if (path_suffix == NULL) {
        path_suffix = path;
    } else {
        path_suffix += 1;
        if (*path_suffix == '\0')  {
            path_suffix -= 1;
            while (path != path_suffix && *path_suffix == PATH_SEPARATOR_CHAR) {
                path_suffix -= 1;
            }
            while (path != path_suffix && *path_suffix != PATH_SEPARATOR_CHAR) {
                path_suffix -= 1;
            }

            if (*path_suffix != '\0' && *path_suffix == PATH_SEPARATOR_CHAR)  {
                path_suffix += 1;
            }
        }
    }

    if (path_suffix[0] == '\0') {
        cye_result_defer(false);
    }

    if (path_suffix[0] ==  '.' ) {
        if (path_suffix[1] == '\0') {
            cye_result_defer(true);
        } else if (path_suffix[1] ==  PATH_SEPARATOR_CHAR) {
            cye_result_defer(true);
        } else if (path_suffix[1] ==  '.') {
            if (path_suffix[2] == '\0') {
                cye_result_defer(true);
            } else if (path_suffix[2] ==  PATH_SEPARATOR_CHAR) {
                cye_result_defer(true);
            } else {
                cye_result_defer(false);
            }
        } else {
            cye_result_defer(false);
        }
    }

defer:
    return result;
}

// Check if path  is symbolic link
bool cye_is_link(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// Check if path  is symbolic link
bool cye_is_mount(ZString path) {
    cye_todo("VAI TRABALHAR VAGABUNDO");
}

// Check if paths reference same file (one can be absolute and another relative or on be a hard link)
bool cye_is_same_path(ZString path1, ZString path2) {
    cye_todo("New Functions to Work on");
}

// Join paths intelligently
Cye_DString cye_path_join(ZString path, ZString* paths) {
    cye_todo("New Functions to Work on");
}

// Size  in bytes
usz cye_path_size(ZString path) {
    cye_todo("New Functions to Work on");
}

// Return real path (resolve symlinks)
ZString cye_path_real(ZString path) {
    cye_todo("New Functions to Work on");
}

// Convert a normalized path to absolute path
// Returns a newly allocated string containing the absolute path
// Returns NULL on error
// The returned string must be freed by the caller
// TODO: For these Path functions we really should go the
// Raylib TextFormat round of having 5~ buffers that cicles each time a functions is called
// so the it endures 5~ times and if the user really wants to live long, it should make a copy.
ZString cye_path_absolute(ZString path) {
    if (!path) return NULL;

    Cye_DString result = {0};

#ifdef PLATFORM_WINDOWS
    // Handle Windows UNC paths specially
    if (path[0] == '\\' && path[1] == '\\') {
        cye_ds_write(&result, path);
        return result.items;
    }

    char abs_path[MAX_PATH];
    DWORD len = GetFullPathNameA(path, MAX_PATH, abs_path, NULL);

    if (len == 0 || len >= MAX_PATH) {
        cye_ds_free(result);
        return NULL;
    }

    // Convert backslashes to forward slashes if the input used them
    if (strchr(path, '/')) {
        for (DWORD i = 0; i < len; i++) {
            if (abs_path[i] == '\\') abs_path[i] = '/';
        }
    }

    cye_ds_write(&result, abs_path);

#else
    char abs_path[PATH_MAX];

    if (path[0] == '/') {
        // Path is already absolute
        cye_ds_write(&result, path);
    } else {
        // Get current working directory first
        if (!getcwd(abs_path, sizeof(abs_path))) {
            return NULL;
        }

        cye_ds_write(&result, abs_path);

        // Add separator if needed
        if (result.count > 0 && result.items[result.count - 1] != '/') {
            cye_ds_write(&result, "/");
        }

        cye_ds_write(&result, path);
    }

    // Clean up any . or .. in the path
    char real_path[PATH_MAX];
    if (realpath(result.items, real_path)) {
        result.count = 0;
        cye_ds_write(&result, real_path);
    } else if (errno != ENOENT) {
        // If error is not "file not exists", return error
        // We allow non-existent paths as long as parent exists
        cye_ds_free(result);
        return NULL;
    }
#endif

    return result.items;
}

//  Return relative path
ZString cye_path_relative(ZString from, ZString target) {
    cye_todo("New Functions to Work on");
}

//  Return home
ZString cye_path_home(void) {
    cye_todo("New Functions to Work on");
}

//  Return current directory
ZString cye_path_cwd(void) {
    cye_todo("New Functions to Work on");
}

ZString cye_path_parent(ZString path) {
    cye_todo("New Functions to Work on");
}

ZString cye_path_owner(ZString path) {
    cye_todo("New Functions to Work on");
}

// Path without extension
TString cye_path_stem(ZString file_path) {
    static bool last = true; // use_last_dot
    static char path_buffer[PATH_MAX]; // TODO: May use this to not destruct the original buffer
    unused(path_buffer);
    if (file_path == NULL) return NULL;

    char* result_buffer = cye_tstrdup(file_path);

    // Handle special dots dir
    if (strcmp(result_buffer, ".") == 0 || strcmp(result_buffer, "..") == 0) {
        return result_buffer;
    }

    // TODO: Check to see if it fucks up
    char *first_dot = strchr(result_buffer, '.');
    char *last_dot = strrchr(result_buffer, '.');

    // No extension found
    if (first_dot == NULL) {
        return result_buffer;
    }

    // Handle hidden files starting with a dot
    if (first_dot == result_buffer) {
        if (last_dot == first_dot) {
            // Just a hidden file without extension
            return result_buffer;
        }
        // Hidden file with extension, move past the first dot
        first_dot = strchr(first_dot + 1, '.');
        last_dot = strrchr(first_dot, '.');
        if (first_dot == NULL) {
            return result_buffer;
        }
    }

    // Terminate string at appropriate dot position
    if (last) {
        *last_dot = '\0';
    } else {
        *first_dot = '\0';
    }

    return result_buffer;

}


//  NOTE: Its not just lexical dir_of, if a folder exists then we consider that
// But maybe we just want lexical?
// Return directory where file is, if it's already an directory it return its self
ZString cye_path_dir_of(ZString file_path) {
    if (!file_path) return NULL;

    // If it's already a directory, return thyself
    Cye_Log_Level old_level = cye_threshold_log_level;
    cye_set_trace_level(CYE_LOG_NONE);
    if (cye_path_file_kind(file_path) == CYE_FILE_KIND_DIRECTORY) {
        return file_path;
    }
    cye_set_trace_level(old_level);

    // Get last separator position
    ZString last_sep = NULL;
    for (const char* p = file_path; *p; p++) {
#ifdef PLATFORM_WINDOWS
        if (*p == '\\' || *p == '/') {
#else
        if (*p == '/') {
#endif
            last_sep = (ZString)p;
        }
    }

    if (!last_sep) {
        // No separator found, return "." for current directory
        return ".";
    }

    // Handle root directory cases
#ifdef PLATFORM_WINDOWS
    // Handle "C:\" case
    if (last_sep == file_path + 2 && file_path[1] == ':') {
        return file_path; // Return full path including root
    }
    // Handle "\\server\share\" case
    if (file_path[0] == '\\' && file_path[1] == '\\') {
        ZString p = file_path + 2;
        int separators = 0;
        while (*p) {
            if (*p == '\\' || *p == '/') {
                separators++;
                if (separators == 2 && p == last_sep) {
                    return file_path; // Return full UNC path
                }
            }
            p++;
        }
    }
#else
    // Handle "/" case
    if (last_sep == file_path) {
        return "/";
    }
#endif

    // Create a static buffer for the result
    static char dir_buffer[CYE_PATH_MAX];
    usz len = last_sep - file_path;

    // Handle the case where the separator is the last character
    if (last_sep[1] == '\0') {
        // Copy the path up to and including the last separator
        if (len >= CYE_PATH_MAX) len = CYE_PATH_MAX - 1;
        memcpy(dir_buffer, file_path, len);
        dir_buffer[len] = '\0';
        return dir_buffer;
    }

    // Copy the path up to (but not including) the last separator
    if (len >= CYE_PATH_MAX) len = CYE_PATH_MAX - 1;
    memcpy(dir_buffer, file_path, len);
    dir_buffer[len] = '\0';
    return dir_buffer;
}


// Get only extension
ZString cye_path_ext(ZString path) {
    ZString file_ext = strrchr(path, '.');
    // May be null;
    return file_ext;
}


#ifdef PLATFORM_WINDOWS
bool cye_path_touch(ZString path) {
    HANDLE h;
    FILETIME ft;
    SYSTEMTIME st;
    bool file_existed = false;
    (void)file_existed; // Maybe do something with this info
    
    // Try to open existing file first
    h = CreateFileA(
        path,
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (h != INVALID_HANDLE_VALUE) {
        file_existed = true;
    } else {
        // File doesn't exist, try to create it
        h = CreateFileA(
            path,
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        if (h == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) {
                // File was created by another process, try to open it
                h = CreateFileA(
                    path,
                    GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL
                );
                
                if (h == INVALID_HANDLE_VALUE) {
                    return false;
                }
                file_existed = true;
            } else {
                // Other error occurred
                return false;
            }
        }
    }
    
    // Get current system time
    GetSystemTime(&st);
    if (!SystemTimeToFileTime(&st, &ft)) {
        CloseHandle(h);
        return false;
    }
    
    // Set both access time and modification time to current time
    bool success = SetFileTime(h, NULL, &ft, &ft);
    
    CloseHandle(h);
    return success;
}

#else

bool cye_path_touch(ZString path) {
    struct stat st;
    bool file_exists = (stat(path, &st) == 0);
    
    if (!file_exists) {
        // Create the file if it doesn't exist
        int fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOCTTY, 0666);
        if (fd < 0) {
            if (errno == EEXIST) {
                // File was created by another process
                file_exists = true;
            } else {
                return false;
            }
        } else {
            close(fd);
        }
    }
    
    // Use utimensat for higher precision timestamps
    struct timespec times[2];
    
    // Get current time with nanosecond precision
    if (clock_gettime(CLOCK_REALTIME, &times[0]) != 0) {
        return false;
    }
    
    times[1] = times[0]; // Set both access and modification time to the same value
    
    return (utimensat(AT_FDCWD, path, times, 0) == 0);
}
#endif

// Utility function to get file modification time (cross-platform)
#ifdef PLATFORM_WINDOWS
time_t cye_path_get_mtime(ZString path) {
    HANDLE h = CreateFileA(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    
    if (h == INVALID_HANDLE_VALUE) {
        return (time_t)-1;
    }
    
    FILETIME ft;
    if (!GetFileTime(h, NULL, NULL, &ft)) {
        CloseHandle(h);
        return (time_t)-1;
    }
    
    CloseHandle(h);
    
    // Convert FILETIME to time_t
    ULARGE_INTEGER ull;
    ull.LowPart = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    
    // FILETIME is in 100-nanosecond intervals since January 1, 1601
    // time_t is seconds since January 1, 1970
    // Difference is 11644473600 seconds
    return (time_t)((ull.QuadPart / 10000000ULL) - 11644473600ULL);
}
#else
time_t cye_path_get_mtime(ZString path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return (time_t)-1;
    }
    return st.st_mtime;
}
#endif

bool cye_make_dir_include_parents_from_tstr(TString path) {
    if (path == NULL || *path == '\0') {
        return false;
    }

    Cye_Log_Level old_level = cye_threshold_log_level;
    cye_set_trace_level(CYE_LOG_NONE);
    bool created = false;

    // Remove trailing slashes
    usz len = strlen(path);
    while (len > 0 && (path[len - 1] == '/' || path[len - 1] == '\\')) {
        path[--len] = '\0';
    }

    // Handle absolute paths on Windows (e.g., "C:\foo")
#ifdef PLATFORM_WINDOWS
    if (len >= 2 && path[1] == ':') {
        if (len == 2) {  // Just a drive letter
            cye_set_trace_level(old_level);
            return true;
        }
        // Skip drive letter and first slash if present
        char *p = path + 3;
        if (*p == '/' || *p == '\\') p++;
        for (; *p; p++) {
            if (*p == '/' || *p == '\\') {
                *p = '\0';
                created |= cye_make_dir(path);
                *p = '\\';
            }
        }
        created |= cye_make_dir(path);
    }
#endif

    // Handle absolute paths on Unix and relative paths on both systems
    for (char *p = path + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            *p = '\0';
            created |= cye_make_dir(path);
            *p = '/';
        }
    }
    created |= cye_make_dir(path);

    cye_set_trace_level(old_level);
    if (!created) {
        cye_trace_error("could not create directories recursively `%s`: %s", path, CYE_GET_ERROR_STRING);
    } else {
        cye_trace_info("created all directories `%s`", path);
    }

    return created;
}

// Create directories recursively
bool cye_make_dir_include_parents(ZString path) {
    bool created  = false;
    usz chk_point = cye_temp_save();
    cye_context   = cye_temp_context();

    TString tpath = cye_path_create(path);
    created = cye_make_dir_include_parents_from_tstr(tpath);

    cye_context  = cye_default_context();
    cye_temp_restore(chk_point);
    return created;
}

//  Remove file
bool cye_remove_file(ZString path) {
    if (!cye_file_exists(path)) {
        cye_trace_info("file `%s` does not exist", path);
        return true;
    }

    Cye_File_Kind type = cye_path_file_kind(path);

    if (type != CYE_FILE_KIND_REGULAR) {
        cye_trace_error("`%s` exists but is not a regular file", path);
        return false;
    }

    if (type == CYE_FILE_KIND_DIRECTORY) {
        cye_trace_error("`%s` exists but is a directory, should we make a recursive remove function?", path);
        return false;
    }

#ifdef PLATFORM_WINDOWS
    int result = remove(path);
#else
    // https://www.man7.org/linux/man-pages/man2/unlink.2.html
    int result = unlink(path);
#endif

    if (result < 0) {
        cye_trace_error("could not remove file `%s`: %s", path, strerror(errno));
        return false;
    }

    cye_trace_info("Removed file `%s`", path);
    return true;

}


// Helper function to join paths
static void path_join(char *dest, ZString dir, ZString file) {
    usz dir_len = strlen(dir);
    strcpy(dest, dir);

    #ifdef PLATFORM_WINDOWS
        if (dir_len > 0 && dir[dir_len - 1] != '\\') {
            strcat(dest, "\\");
        }
    #else
        if (dir_len > 0 && dir[dir_len - 1] != '/') {
            strcat(dest, "/");
        }
    #endif

    strcat(dest, file);
}

// Remove directory recursively
bool cye_remove_dir(ZString path) {
    char full_path[PATH_MAX];
    bool success = true;

#ifndef PLATFORM_WINDOWS
    DIR *dir = opendir(path);
    if (!dir) {
        if (errno == ENOENT) {
            // Directory doesn't exist
            return true;
        }
        cye_trace_error("could not open directory `%s`: %s", path, strerror(errno));
        return false;
    }

    struct dirent *entry;
    while ((entry = readdir(dir))) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        path_join(full_path, path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) != 0) {
            cye_trace_error("could not stat `%s`: %s", full_path, strerror(errno));
            success = false;
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            // Recursively remove subdirectory
            if (!cye_remove_dir(full_path)) {
                success = false;
            }
        } else {
            // Remove file
            if (unlink(full_path) != 0) {
                cye_trace_error("could not delete file `%s`: %s", full_path, strerror(errno));
                success = false;
            } else {
                cye_trace_info("deleted file `%s`", full_path);
            }
        }
    }

    closedir(dir);

    // Remove the empty directory
    if (success && rmdir(path) != 0) {
        cye_trace_error("could not remove directory `%s`: %s", path, strerror(errno));
        success = false;
    } else if (success) {
        cye_trace_info("Removed directory `%s`", path);
    }
#else
    WIN32_FIND_DATA find_data;
    char search_path[PATH_MAX];

    // Prepare search path
    snprintf(search_path, sizeof(search_path), "%s\\*", path);

    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            // Directory is empty
            return RemoveDirectory(path);
        }
        cye_trace_error("could not open directory `%s`: %lu", path, GetLastError());
        return false;
    }

    do {
        // Skip "." and ".." directories
        if (strcmp(find_data.cFileName, ".") == 0 ||
            strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        path_join(full_path, path, find_data.cFileName);

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Recursively remove subdirectory
            if (!cye_remove_dir(full_path)) {
                success = false;
            }
        } else {
            // Remove file
            if (!DeleteFile(full_path)) {
                cye_trace_error("could not delete file `%s`: %lu", full_path, GetLastError());
                success = false;
            } else {
                cye_trace_info("deleted file `%s`", full_path);
            }
        }
    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);

    // Remove the empty directory
    if (success && !RemoveDirectory(path)) {
        cye_trace_error("could not remove directory `%s`: %lu", path, GetLastError());
        success = false;
    } else if (success) {
        cye_trace_info("Removed directory `%s`", path);
    }

#endif

    return success;
}

//  Remove directories recursively
bool cye_remove_dirs(ZString path) {
    cye_todo("New Functions to Work on");
}

//  Move file or directory
bool cye_path_move(ZString src, ZString dst) {

    cye_trace_info("Moving %s -> %s", src, dst);
#ifndef PLATFORM_WINDOWS // Unix
    // On Unix-like systems, rename() can move files across directories
    if (rename(src, dst) < 0) {
        cye_trace_error("Could not move %s to %s: %s", src, dst, CYE_GET_ERROR_STRING);
        return false;
    }
#else
    if (!MoveFileEx(src, dst, MOVEFILE_REPLACE_EXISTING)) {
        cye_trace_error("Could not move %s to %s: %s", src, dst, win32_error_message(GetLastError()));
        return false;
    }
#endif // PLATFORM_WINDOWS
    return true;
}

//  Rename file or directory
bool cye_path_rename(ZString src, ZString dst) {
    // TODO: use dir of to check they are on the same directory
    return cye_path_move(src, dst);
}

//  Recursive directory or file renaming
bool cye_path_renames(ZString old_path, ZString new_path) {
    cye_todo("New Functions to Work on");
}

//  Rename file or directory, replacing
bool cye_path_replace(ZString src, ZString dst) {
    cye_todo("New Functions to Work on");
}

// Paths valid for one func call much like TextFormat from Raylib
//  Iterator of directory entries
Cye_Path_DArray cye_path_scandir(ZString path) {
    cye_todo("New Functions to Work on");
}

Cye_Path_DArray cye_list_dir(ZString path) {
    cye_todo("New Functions to Work on");
}


internal bool cye_glob_filter(ZString path, void *user_data);
internal int  cye_path_glob_recursive_dirent(char *pattern, char path[PATH_MAX + 1], Cye_Path_DArray *matches);
internal bool cye_path_glob_recursive(char pattern[PATH_MAX + 1], char path[PATH_MAX + 1], Cye_Path_DArray *matches);



int cye_path_glob_recursive_dirent(char *pattern, char path[PATH_MAX + 1], Cye_Path_DArray *matches) {
#ifndef PLATFORM_WINDOWS
    // Find the first ocurrence of the path separator
    char *pattern_sep = strchr(pattern, PATH_SEPARATOR_CHAR);
    bool is_last_pattern = pattern_sep == NULL;
    char *pattern_next = NULL;
    if (pattern_sep != NULL) {
        *pattern_sep = '\0';
        pattern_next = pattern_sep + 1;
    }


    DIR *dir = opendir(path);
    struct dirent *dirent = NULL;
    while ((dirent = readdir(dir)) != NULL) {
        if (cye_pattern_match(pattern, dirent->d_name, 0)) {
            if (is_last_pattern) {
                char *match_path =
                    // +2 for the \0 and the slash
                cye_context.alloc(sizeof(char) * (strlen(path) + strlen(dirent->d_name) + 2));
                strcpy(match_path, path);
                strcat(match_path, PATH_SEPARATOR);
                strcat(match_path, dirent->d_name);
                cye_da_append(matches, match_path);
            } else if (dirent->d_type == DT_DIR
                && strcmp(dirent->d_name, ".") != 0
                && strcmp(dirent->d_name, "..") != 0
            ) {
                char *path_end = strchr(path, '\0');
                strcat(path, PATH_SEPARATOR);
                strcat(path, dirent->d_name);
                cye_path_glob_recursive_dirent(pattern_next, path, matches);
                *path_end = '\0';
            }
        }
    }
#else
    cye_trace_warn("Windows must use `cye_path_glob_recursive` instead");
#endif
    return 0;
}

internal bool cye_glob_filter(ZString path, void *user_data) {
    cye_assert(path != NULL);

    Cye_Glob_Filter_Data data = *(Cye_Glob_Filter_Data*)user_data;

    Cye_Path_DArray* matches   = data.matches;
    MutString pattern          = data.pattern;
    MutString pattern_next     = data.pattern_next;

    bool is_last_pattern = pattern_next == NULL;

    ZString path_suffix = strrchr(path,  PATH_SEPARATOR_CHAR);
    if (path_suffix == NULL) {
        path_suffix = path;
    } else {
        path_suffix += 1;
    }

    if (!cye_pattern_match(pattern, path_suffix, 0)) {
        return false;
    }

    // That includes directories
    if (is_last_pattern) {
        return true;
    }

    if (CYE_FILE_KIND_DIRECTORY == cye_path_file_kind(path)
        && !cye_is_period_dir(path))
    {

        // IMPORTANT: a COPY of the `pattern_next` MUST be given
        // if you change any byte on the `pattern` string.
        // right now we're reconstructing anything that meddles with
        // the pattern_next, but IDK if thats enough!
        cye_path_glob_recursive(pattern_next, (MutString) path, matches);
    }
    return false;
}

bool cye_path_glob_recursive(char pattern[PATH_MAX + 1], char path[PATH_MAX + 1], Cye_Path_DArray *matches) {
    // Find the first ocurrence of the path separator
    char *pattern_sep = strchr(pattern, PATH_SEPARATOR_CHAR);

    char *pattern_next = NULL;
    if (pattern_sep != NULL) {
        // That means that we started with '/'
        if (pattern_sep == pattern) {
            pattern += 1; // pattern is either an actual pattern or ""
            pattern_sep = strchr(pattern, PATH_SEPARATOR_CHAR);
        }
        if (pattern_sep != NULL) {
            *pattern_sep = '\0';
            pattern_next = pattern_sep + 1;
        }
    }

    Cye_Glob_Filter_Data data = {
        .pattern         = pattern,
        .pattern_next    = pattern_next,
        .matches         = matches
    };

    bool result = cye_read_dir_filtered(
        path, (Cye_Path_DArray*)matches,
        true, cye_glob_filter, &data
    );

    // We undo our changes, but only if we did change
    if (pattern_next != NULL) {
        *pattern_sep = PATH_SEPARATOR_CHAR;
    }

    return result;
}


Cye_Path_DArray cye_path_tglob(ZString pattern) {
    Cye_Path_DArray matches = {0};
    if (!cye_is_pattern_well_formed(pattern)) {
        return cliteral(Cye_Path_DArray){0};
    }

    Cye_Context old_ctx = cye_context;
    cye_context = cye_temp_context();
    {
        cye_path_glob(pattern, &matches);
    }
    cye_context = old_ctx;

    return matches;
}

bool cye_path_glob(ZString pattern, Cye_Path_DArray *matches) {
    if (!cye_is_pattern_well_formed(pattern)) {
        return false;
    }
    char path[PATH_MAX + 1] = {'\0'};
    bool absolute_path = pattern[0] == PATH_SEPARATOR_CHAR;
    if (absolute_path) {
        strcpy(path, PATH_SEPARATOR);
    } else {
        // Maybe @leaks
        getcwd(path, PATH_MAX);
    }

    static char mut_pattern[PATH_MAX+1];
    strcpy(mut_pattern, pattern);
    cye_path_glob_recursive(mut_pattern, path, matches);

    // NOTE: Not sure which is better or faster, using the filtered thing or raw dirent
    unused(cye_path_glob_recursive_dirent);
    return true;
}


//------------------------------------------------------------------------------------
//  Pipe Implementation
//------------------------------------------------------------------------------------
Cye_Pipe cye_pipe_open(void) {
#ifndef PLATFORM_WINDOWS
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return CYE_INVALID_PIPE;
    }
    return (Cye_Pipe){
        .read = pipefd[0],
        .write = pipefd[1]
    };

#else
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL
    };

    HANDLE read_handle, write_handle;
    if (!CreatePipe(&read_handle, &write_handle, &sa, 0)) {
        return CYE_INVALID_PIPE;
    }
    return (Cye_Pipe){
        .read = read_handle,
        .write = write_handle
    };
#endif
}

void cye_pipe_close(Cye_Pipe handle) {
#ifndef PLATFORM_WINDOWS
    if (handle.read != CYE_INVALID_PIPE_HANDLE) close(handle.read);
    if (handle.write != CYE_INVALID_PIPE_HANDLE) close(handle.write);
#else
    if (handle.read != CYE_INVALID_PIPE_HANDLE) CloseHandle(handle.read);
    if (handle.write != CYE_INVALID_PIPE_HANDLE) CloseHandle(handle.write);
#endif
}

bool cye_is_pipe_valid(Cye_Pipe handle) {
    return handle.read != CYE_INVALID_PIPE_HANDLE &&
           handle.write != CYE_INVALID_PIPE_HANDLE;
}

bool cye_pipe_read(Cye_Pipe_Handle pipe, char* buffer, usz buffer_size, usz * bytes_read) {
#ifndef PLATFORM_WINDOWS
    isz result = read(pipe, buffer, buffer_size);
    if (result < 0) {
        *bytes_read = 0;
        return false;
    }
    *bytes_read = (usz)result;
    return true;
#else
    DWORD bytes_read_win;
    if (!ReadFile(pipe, buffer, (DWORD)buffer_size, &bytes_read_win, NULL)) {
        *bytes_read = 0;
        return false;
    }
    *bytes_read = bytes_read_win;
    return true;
#endif
}

bool cye_pipe_write(Cye_Pipe_Handle pipe, const char* buffer, usz buffer_size, usz* bytes_written) {
#ifndef PLATFORM_WINDOWS
    isz result = write(pipe, buffer, buffer_size);
    if (result < 0) {
        *bytes_written = 0;
        return false;
    }
    *bytes_written = (usz)result;
    return true;
#else
    DWORD bytes_written_win;
    if (!WriteFile(pipe, buffer, (DWORD)buffer_size, &bytes_written_win, NULL)) {
        *bytes_written = 0;
        return false;
    }
    *bytes_written = bytes_written_win;
    return true;
#endif
}



void cye_pipe_close_handle(Cye_Pipe_Handle* pipe) {
    if (*pipe == CYE_INVALID_PIPE_HANDLE) return;
#ifndef PLATFORM_WINDOWS
    close(*pipe);
#else
    CloseHandle(*pipe);
#endif
    *pipe = CYE_INVALID_PIPE_HANDLE;
}

//------------------------------------------------------------------------------------
//  Dynamic Array Implementation
//------------------------------------------------------------------------------------

// All macros xD

//------------------------------------------------------------------------------------
//  Slices Implementation
//------------------------------------------------------------------------------------

// All macros xD



//------------------------------------------------------------------------------------
//  String Slice Implementation
//------------------------------------------------------------------------------------

Cye_String_Slice cye_str_slice_make(ZString str) {
    return (Cye_String_Slice)cye_slice_make(str, strlen(str));
}

// Trim whitespace from both ends
Cye_String_Slice cye_str_slice_trim(Cye_String_Slice s) {
    while (s.count > 0 && isspace(s.data[0])) {
        s.data++;
        s.count--;
    }
    while (s.count > 0 && isspace(s.data[s.count - 1])) {
        s.count--;
    }
    return s;
}

// String slice to null-terminated string (requires buffer)
void cye_str_slice_to_zstr(Cye_String_Slice s, char *buf, usz buf_size) {
    usz to_copy = s.count < buf_size - 1 ? s.count : buf_size - 1;
    memcpy(buf, s.data, to_copy);
    buf[to_copy] = '\0';
}


// New function: Strip left whitespace
Cye_String_Slice cye_str_slice_strip_left(Cye_String_Slice s) {
    while (s.count > 0 && isspace(s.data[0])) {
        s.data++;
        s.count--;
    }
    return s;
}

// New function: Strip right whitespace
Cye_String_Slice cye_str_slice_strip_right(Cye_String_Slice s) {
    while (s.count > 0 && isspace(s.data[s.count - 1])) {
        s.count--;
    }
    return s;
}
// Create string slice from string and explicit length
Cye_String_Slice cye_str_slice_make_len(ZString str, usz len) {
    return (Cye_String_Slice)cye_slice_make((char*)str, len);
}

// Compare two string slices
bool cye_str_slice_equals(Cye_String_Slice a, Cye_String_Slice b) {
    if (a.count != b.count) return false;
    return memcmp(a.data, b.data, a.count) == 0;
}

bool cye_str_slice_equals_zstr(Cye_String_Slice a, const char* b) {
    usz b_count = strlen(b);
    if (a.count != b_count) {
        return false;
    } else {
        return memcmp(a.data, b, a.count) == 0;
    }
}

// Check if string slice contains substring
bool cye_str_slice_contains(Cye_String_Slice haystack, Cye_String_Slice needle) {
    if (needle.count > haystack.count) return false;

    for (usz i = 0; i <= haystack.count - needle.count; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.count) == 0) {
            return true;
        }
    }
    return false;
}


// Split string slice by delimiter into a Dynamic Array
Cye_String_Slice_DArray cye_str_slice_split(Cye_String_Slice s, Cye_String_Slice delim) {
    Cye_String_Slice_DArray result = {0};

    char *start   = (char*)s.data;
    char *end     = (char*)s.data + s.count;
    char *current = (char*)s.data;

    while (current <= end - delim.count) {
        if (memcmp(current, delim.data, delim.count) == 0) {
            cye_da_append(&result, cye_str_slice_make_len(start, current - start));
            current += delim.count;
            start = current;
        } else {
            current++;
        }
    }

    // Add the last part
    if (start < end) {
        cye_da_append(&result, cye_str_slice_make_len(start, end - start));
    }

    return result;
}

Cye_String_Slice_DArray cye_str_slice_split_zstr(Cye_String_Slice s, ZString delim) {
    Cye_String_Slice ss_delim = cye_str_slice_make(delim);
    return cye_str_slice_split(s, ss_delim);
}

// Split string slice at first occurrence of delimiter
void cye_str_slice_split_first(Cye_String_Slice s, char delim, Cye_String_Slice *before, Cye_String_Slice *after) {
    for (usz i = 0; i < s.count; i++) {
        if (s.data[i] == delim) {
            if (before) *before = (Cye_String_Slice)cye_slice_make(s.data, i);
            if (after) *after = (Cye_String_Slice)cye_slice_make(s.data + i + 1, s.count - i - 1);
            return;
        }
    }
    if (before) *before = s;
    if (after) *after = CYE_STR_SLICE_EMPTY;
}

// Check if string slice starts with prefix
bool cye_str_slice_starts_with(Cye_String_Slice s, Cye_String_Slice prefix) {
    if (prefix.count > s.count) return false;
    return memcmp(s.data, prefix.data, prefix.count) == 0;
}

// Check if string slice ends with suffix
bool cye_str_slice_ends_with(Cye_String_Slice s, Cye_String_Slice suffix) {
    if (suffix.count > s.count) return false;
    return memcmp(s.data + s.count - suffix.count, suffix.data, suffix.count) == 0;
}

// Check if string slice starts with zero-terminated prefix
bool cye_str_slice_starts_with_zstr(Cye_String_Slice s, ZString prefix) {
    usz prefix_len = strlen(prefix);
    if (prefix_len > s.count) return false;
    return memcmp(s.data, prefix, prefix_len) == 0;
}

// Check if string slice ends with zero-terminated suffix
bool cye_str_slice_ends_with_zstr(Cye_String_Slice s, ZString suffix) {
    usz suffix_len = strlen(suffix);
    if (suffix_len > s.count) return false;
    return memcmp(s.data + s.count - suffix_len, suffix, suffix_len) == 0;
}

//------------------------------------------------------------------------------------
//  ZString Implementation
//------------------------------------------------------------------------------------

bool cye_zstr_ends_with(ZString src, ZString ending) {
    if (!src || !ending) return false;  // NULL check

    usz src_len    = strlen(src);
    usz ending_len = strlen(ending);

    // If ending is longer than src, it can't be a suffix
    if (ending_len > src_len) return false;

    // Compare the end of src with ending
    return memcmp(src + (src_len - ending_len), ending, ending_len) == 0;
}

bool cye_zstr_starts_with(ZString src, ZString prefix) {
    if (src == NULL || prefix == NULL) return false;
    if (!*prefix)               return true;  // Empty prefix always matches
    if (!*src)                  return false;    // Empty string only matches empty prefix

    usz prefix_len = strlen(prefix);
    usz src_len    = strlen(src);

    if (prefix_len > src_len) return false;

    return memcmp(src, prefix, prefix_len) == 0;
}

bool cye_zstr_match_pattern(ZString pattern, ZString str) {
    cye_trace_warn("`%s` deprecated in favor or `cye_pattern_match`", __func__);
    // End of pattern
    if (*pattern == '\0') return *str == '\0';

    // Handle '*' wildcard
    if (*pattern == '*') {
        // Skip consecutive '*'
        while (*(pattern + 1) == '*') pattern++;

        // Try matching the rest of the pattern with different positions in str
        for (usz i = 0; i <= strlen(str); i++) {
            if (cye_zstr_match_pattern(pattern + 1, str + i)) {
                return true;
            }
        }
        return false;
    }

    // Normal character matching
    if (*str != '\0' && (*pattern == *str || *pattern == '?')) {
        return cye_zstr_match_pattern(pattern + 1, str + 1);
    }

    return false;
}


ZString cye_zstr_ordinal(int n) {
    static ZString suffixes[]  = { "th", "st", "nd", "rd", "th"};
    if (11 <= (n % 100) && (n % 100) <= 13) {
        return "th";
    }
    return suffixes[cye_min(n % 10, 4)];
}



// NOTE: Based on this steal
// https://github.com/cacharle/globule/blob/d9ac95c55750dcb07dc41e87d4bc760a1ac3032e/src/fnmatch.c#L6C3-L6C4
bool cye_pattern_match(ZString pattern, ZString text, int flags) {
    // static int depth = 0;
    // if(!cye_is_pattern_well_formed(pattern) && 0 == depth) {
    //     cye_trace_warn("Malformed pattern (%s) you may check this prior with `cye_is_pattern_well_formed`", pattern);
    //     return false;
    // }

    if (flags & CYE_PATTERN_PERIOD && *text == '.' && *pattern != '.') {
        return false;
    }
    if (*pattern == '\0') {
        return *text == '\0';
    }
    if (*text == '\0') {
        return strcmp(pattern, "*") == 0;
    }

    if ( (flags & CYE_PATTERN_PATH && *text == '/')
      || (flags & CYE_PATTERN_PERIOD && *text == '.'))
    {
        if (*pattern == *text) {
            return cye_pattern_match(pattern + 1, text + 1, flags);
        } else if (*pattern == '*' || *pattern == '?'){
            return cye_pattern_match(pattern + 1, text, flags);
        }
        return false;
    }

    switch (*pattern) {
    // TODO:
    // case '\\':
    //     if (!(flags & CYE_PATTERN_NO_ESCAPE))
    //         pattern++;
    //     break;
    case '*':
        if (cye_pattern_match(pattern + 1, text, flags)) {
            return true;
        }
        if (cye_pattern_match(pattern, text + 1, flags)) {
            return true;
        }
        return cye_pattern_match(pattern + 1, text + 1, flags);
    case '?':
        return cye_pattern_match(pattern + 1, text + 1, flags);
    case '[':
        pattern++;
        bool complement = *pattern == '!';
        if (complement) {
            pattern++;
        }
        ZString closing = strchr(pattern + 1, ']') + 1;
        if (*pattern == *text) { // has to contain at least one character
            return !complement ? cye_pattern_match(closing, text + 1, flags) : false;
        }
        pattern++;
        for (; *pattern != ']'; pattern++) {
            if (pattern[0] == '-' && pattern + 2 != closing) {
                char range_start = pattern[-1];
                char range_end = pattern[1];
                if (*text >= range_start && *text <= range_end) {
                    return !complement ? cye_pattern_match(closing, text + 1, flags) : false;
                }
                pattern++;
            } else if (*pattern == *text) {
                return !complement ? cye_pattern_match(closing, text + 1, flags) : false;
            }
        }
        return !complement ? false : cye_pattern_match(closing, text + 1, flags);
    }
    if (*pattern == *text) {
        return cye_pattern_match(pattern + 1, text + 1, flags);
    }
    return false;
}

bool cye_is_pattern_well_formed(ZString pattern) {
    bool in_class = false;
    for (usz idx = 0; pattern[idx] != '\0'; idx++) {
        if (pattern[idx] == '[') {
            idx++;
            if (pattern[idx] == '\0') {
                return false;
            }
            idx++;
            if (pattern[idx] == '\0') {
                return false;
            }
            in_class = true;
        }
        if (pattern[idx] == ']') {
            in_class = false;
        }
    }
    if (in_class) {
        return false;
    }
    return true;
}

// Make the glob match accpet pattern?
// Advantages is that is less recursive than pattern_match
Cye_Match_Result cye_glob_match(ZString pattern, ZString text) {
    cye_trace_warn("`%s` untested, this one does't consider period `.` and slash `/` special.", __func__);
    while (*pattern != '\0' && *text != '\0') {
        switch (*pattern) {
        case '?': {
            pattern += 1;
            text += 1;
        } break;

        case '*': {
            while (*(pattern + 1) == '*') pattern++;
            Cye_Match_Result result = cye_glob_match(pattern + 1, text);
            if (result != CYE_GLOB_NO_MATCH) {
                return result;
            }
            text += 1;
        } break;

        case '[': {
            bool matched = false;
            bool negate = false;

            pattern += 1; // skipping [
            if (*pattern == '\0') {
                return CYE_GLOB_SYNTAX_ERROR; // unclosed [
            }

            if (*pattern == '!') {
                negate = true;
                pattern += 1;
                if (*pattern == '\0') {
                    return CYE_GLOB_SYNTAX_ERROR; // unclosed [
                }
            }

            char prev = *pattern;
            matched |= prev == *text;
            pattern += 1;

            while (*pattern != ']' && *pattern != '\0') {
                switch (*pattern) {
                case '-': {
                    pattern += 1;
                    switch (*pattern) {
                    case ']':
                        matched |= '-' == *text;
                        break;
                    case '\0':
                        return CYE_GLOB_SYNTAX_ERROR; // unclosed [
                    default: {
                        matched |= prev <= *text && *text <= *pattern;
                        prev = *pattern;
                        pattern += 1;
                    }
                    }
                } break;
                default: {
                    prev = *pattern;
                    matched |= prev == *text;
                    pattern += 1;
                }
                }
            }

            if (*pattern != ']') {
                return CYE_GLOB_SYNTAX_ERROR; // unclosed [
            }
            if (negate) {
                matched = !matched;
            }
            if (!matched) {
                return CYE_GLOB_NO_MATCH;
            }

            pattern += 1;
            text += 1;
        } break;

        case '\\':
            pattern += 1;
            if (*pattern == '\0') {
                return CYE_GLOB_SYNTAX_ERROR; // unfinished escape
            }
        // fallthrough
        default: {
            if (*pattern == *text) {
                pattern += 1;
                text += 1;
            } else {
                return CYE_GLOB_NO_MATCH;
            }
        }
        }
    }

    if (*text == '\0') {
        while (*pattern == '*') {
            pattern += 1;
        }
        if (*pattern == '\0') {
            return CYE_GLOB_MATCHED;
        }
    }

    return CYE_GLOB_NO_MATCH;
}



//----------------------------------------------------------------------------------
//  Dynamic String Implementation
//----------------------------------------------------------------------------------


// NOTE: Don't use this yet
// TODO: Improve and use this to sanity check ds_printf and printlike functions
static int cye_count_non_scaped_percent(ZString s) {
    int count = 0;
    int i = 0;

    while (s[i] != '\0') {
        if (s[i] == '%') {
            // Check if the '%' is escaped
            // WARN: This only check one level of escaped
            // actual it fails with "\\%s" for examples it'l think its
            // escaped when it's not. This function is to be taken not as exact
            // but a lower bound of %'s but still need to check, basically don't use this yet
            if (i == 0 || s[i - 1] != '\\') {
                count++;
            }
        }
        i++;
    }
    return count;
}

void cye_ds_printf(Cye_DString *ds, ZString fmt, ...) {

    unused(cye_count_non_scaped_percent);
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    cye_assert(n >= 0);
    usz chk_point = cye_temp_save();
    char *result = cye_talloc(n + 1);

    cye_assert(result != NULL && "Extend the size of the temporary allocator");

    va_start(args, fmt);
    vsnprintf(result, n + 1, fmt, args);
    va_end(args);
    cye_ds_write_buf(ds, result, n); // Don't write the null terminator
    cye_temp_restore(chk_point);
}

//----------------------------------------------------------------------------------
//  Utils Math Implementation
//----------------------------------------------------------------------------------

// Clamp float value
f32 cye_clamp(f32 value, f32 min, f32 max) {
    f32 result = (value < min)? min : value;
    if (result > max) result = max;
    return result;
}

// Calculate linear interpolation between two floats
f32 cye_lerp(f32 start, f32 end, f32 amount) {
    f32 result = start + amount*(end - start);
    return result;
}

// Normalize input value within input range
f32 cye_normalize(f32 value, f32 start, f32 end) {
    f32 result = (value - start)/(end - start);
    return result;
}

// remap input value within input range to output range
f32 cye_remap(f32 value, f32 inputStart, f32 inputEnd, f32 outputStart, f32 outputEnd) {
    f32 result = (value - inputStart)/(inputEnd - inputStart)*(outputEnd - outputStart) + outputStart;
    return result;
}

// Floor function implementation without math.h
f32 cye_floorf(f32 x) {
    int32_t i = (int32_t)x;
    return (x < 0.0f && x != i) ? i - 1.0f : (f32)i;
}

// Absolute value for float
f32 cye_fabsf(f32 x) {
    union {
        f32 f;
        uint32_t i;
    } u = { .f = x };
    u.i &= 0x7FFFFFFF;  // Clear sign bit
    return u.f;
}

// Maximum of two floats
f32 cye_fmaxf(f32 x, f32 y) {
    // Handle NaN cases first
    if (x != x) return y;
    if (y != y) return x;
    // Normal comparison
    return x > y ? x : y;
}

// Wrap input value from min to max
f32 cye_wrap(f32 value, f32 min, f32 max) {
    f32 result = value - (max - min)*cye_floorf((value - min)/(max - min));
    return result;
}

// Check whether two given f32s are almost equal
int cye_float_equals(f32 x, f32 y) {
    int result = (cye_fabsf(x - y)) <= (EPSILON*cye_fmaxf(1.0f, cye_fmaxf(cye_fabsf(x), cye_fabsf(y))));
    return result;
}

//------------------------------------------------------------------------------------
//  Utils Implementation
//------------------------------------------------------------------------------------

void cye_set_trace_level(Cye_Log_Level level) {
    cye_threshold_log_level = level;
}

// TODO: Add colors from nabs.h
void cye_trace_log(Cye_Log_Level level, ZString fmt, ...) {
    // Level below current threshold, don't log anythin
    if (level < cye_threshold_log_level) return;

    va_list args;
    va_start(args, fmt);
    char buffer[CYE_MAX_TRACE_LOG_MSG_LENGTH] = { 0 };

    ZString color = "";
    ZString reset = "";
    ZString bold = "";


    switch (level) {
        case CYE_LOG_TRACE:   break;
        case CYE_LOG_DEBUG:   color = ESCAPE_CODE_OKCYAN;  reset = ESCAPE_CODE_RESET; break;
        case CYE_LOG_INFO:    color = ESCAPE_CODE_LOG;     reset = ESCAPE_CODE_RESET; break;
        case CYE_LOG_OKAY:    color = ESCAPE_CODE_OKGREEN; reset = ESCAPE_CODE_RESET; break;
        case CYE_LOG_WARNING: color = ESCAPE_CODE_WARNING; reset = ESCAPE_CODE_RESET; break;
        case CYE_TRACE_ERROR: color = ESCAPE_CODE_ERROR;   reset = ESCAPE_CODE_RESET; break;
        case CYE_LOG_FATAL:   color = ESCAPE_CODE_ERROR;   reset = ESCAPE_CODE_RESET; bold = ESCAPE_CODE_BOLD; break;
        case CYE_LOG_ALL:     break;
        case CYE_LOG_NONE:    break;
        default: cye_unreachable("cye_trace_log"); break;
    }

#if !defined(PLATFORM_WINDOWS)
    if (!isatty(STDOUT_FILENO)) {
        color = ""; reset = ""; bold = "";
    }
#else
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) != FILE_TYPE_CHAR) {
        color = ""; reset = ""; bold = "";
    }
#endif

    const usz max_len = CYE_MAX_TRACE_LOG_MSG_LENGTH;
    usz written = 0 ;
    switch (level) {
        case CYE_LOG_TRACE:   written = snprintf(buffer, max_len, "%sTRACE%s%s: ", color, reset, bold); break;
        case CYE_LOG_DEBUG:   written = snprintf(buffer, max_len, "%sDEBUG%s%s: ", color, reset, bold); break;
        case CYE_LOG_INFO:    written = snprintf(buffer, max_len, "%sINFO%s%s:  ", color, reset, bold); break;
        case CYE_LOG_OKAY:    written = snprintf(buffer, max_len, "%sOKAY%s%s:  ", color, reset, bold); break;
        case CYE_LOG_WARNING: written = snprintf(buffer, max_len, "%sWARN%s%s:  ", color, reset, bold); break;
        case CYE_TRACE_ERROR: written = snprintf(buffer, max_len, "%sERROR%s%s: ", color, reset, bold); break;
        case CYE_LOG_FATAL:   written = snprintf(buffer, max_len, "%sFATAL%s%s: ", color, reset, bold); break;
        case CYE_LOG_ALL:     written = snprintf(buffer, max_len, "%sALL%s%s:   ", color, reset, bold); break;
        case CYE_LOG_NONE:    return;
        default: cye_unreachable("cye_trace_log");         break;
    }


    //TODO: Better name
    usz fmt_size = (usz)strlen(fmt);
    memcpy(
        buffer + strlen(buffer),
        fmt,
        (fmt_size < (max_len - written))
          ? fmt_size
          : (max_len - written)
    );

    strcat(buffer, "\n");
    vprintf(buffer, args);
    snprintf(buffer, max_len, "%s", reset);
    fflush(stdout);
    va_end(args);

    // Ensure death if fatal
    if (CYE_LOG_FATAL == level) {
        exit(EXIT_FAILURE);
    }
}

void cye__assert_handler(char const *prefix, char const *condition, char const *file, int line, char const *msg, ...) {
    fprintf(stderr, "%s:%d: %s: ", file, line, prefix);
    if (condition) {
        fprintf(stderr, "`%s` ", condition);
    }
    if (msg) {
        va_list va;
        va_start(va, msg);
        vfprintf(stderr, msg, va);
        va_end(va);
    }
    fprintf(stderr, "\n");
}

TString cye_file_stats_tstring(Cye_File_Stats stats) {
    return cye_tprintf(cye_file_stats_fmt, cye_file_stats_fmt_arg(stats));
}

TString cye_str_slice_tstring(Cye_String_Slice ss) {
    return cye_tprintf(cye_ss_fmt, cye_ss_fmt_arg(ss));
}

TString cye_ds_tstring(Cye_DString ds) {
    return cye_tprintf(cye_ds_fmt, cye_ds_fmt_arg(ds));
}

ZString cye_cpu_architecture() {
#if defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
  return "x86_32";
#elif defined(__ARM_ARCH_2__)
  return "ARM2";
#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
  return "ARM3";
#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
  return "ARM4T";
#elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
  return "ARM5"
#elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
  return "ARM6T2";
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
  return "ARM6";
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  return "ARM7";
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  return "ARM7A";
#elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
  return "ARM7R";
#elif defined(__ARM_ARCH_7M__)
  return "ARM7M";
#elif defined(__ARM_ARCH_7S__)
  return "ARM7S";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "ARM64";
#elif defined(mips) || defined(__mips__) || defined(__mips)
  return "MIPS";
#elif defined(__sh__)
  return "SUPERH";
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
  return "POWERPC";
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
  return "POWERPC64";
#elif defined(__sparc__) || defined(__sparc)
  return "SPARC";
#elif defined(__m68k__)
  return "M68K";
#else
  return "UNKNOWN";
#endif
}

// Base on https://stackoverflow.com/a/75644008
// > .NET Core uses 4096 * sizeof(WCHAR) buffer on stack for FormatMessageW call. And...thats it.
// >
// > https://github.com/dotnet/runtime/blob/3b63eb1346f1ddbc921374a5108d025662fb5ffd/src/coreclr/utilcode/posterror.cpp#L264-L265
#ifndef CYE_WIN32_ERR_MSG_SIZE
#   define CYE_WIN32_ERR_MSG_SIZE (4096 * sizeof(WCHAR))
#endif // CYE_WIN32_ERR_MSG_SIZE

#ifdef PLATFORM_WINDOWS
char *win32_error_message(DWORD err) {
    static char win32ErrMsg[CYE_WIN32_ERR_MSG_SIZE] = {0};
    DWORD errMsgSize = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err,
        LANG_USER_DEFAULT, win32ErrMsg, CYE_WIN32_ERR_MSG_SIZE, NULL
    );

    if (errMsgSize == 0) {
        if (GetLastError() != ERROR_MR_MID_NOT_FOUND) {
            if (sprintf(win32ErrMsg, "Could not get error message for 0x%lX", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        } else {
            if (sprintf(win32ErrMsg, "Invalid Windows Error code (0x%lX)", err) > 0) {
                return (char *)&win32ErrMsg;
            } else {
                return NULL;
            }
        }
    }

    while (errMsgSize > 1 && isspace(win32ErrMsg[errMsgSize - 1])) {
        win32ErrMsg[--errMsgSize] = '\0';
    }

    return win32ErrMsg;
}
#endif // PLATFORM_WINDOWS

#endif // CYE_IMPLEMENTATION


/*..................................................................................
 .                                                                                 .
 .                                SHORT NAMES                                      .
 .                                                                                 .
 ...................................................................................
*/
#ifndef _CYE_NO_SHORT_NAMES_GUARD_
#define _CYE_NO_SHORT_NAMES_GUARD_
#if !defined(CYE_NO_SHORT_NAMES)

//----------------------------------------------------------------------------------
//  Tweakable Constants Short Names
//----------------------------------------------------------------------------------

// NOTE: Does it make sense to shorten these?

//----------------------------------------------------------------------------------
//  Structures Definition with Prefix Short Names
//----------------------------------------------------------------------------------

#define LOG_ALL     CYE_LOG_ALL
#define LOG_TRACE   CYE_LOG_TRACE
#define LOG_DEBUG   CYE_LOG_DEBUG
#define LOG_INFO    CYE_LOG_INFO
#define LOG_OKAY    CYE_LOG_OKAY
#define LOG_WARNING CYE_LOG_WARNING
#define LOG_ERROR   CYE_TRACE_ERROR
#define LOG_FATAL   CYE_LOG_FATAL
#define LOG_NONE    CYE_LOG_NONE

#define Log_Level           Cye_Log_Level
#define DArray              Cye_DArray
#define Path_DArray         Cye_Path_DArray
#define File_Filter         Cye_File_Filter

#define FILE_KIND_REGULAR   CYE_FILE_KIND_REGULAR
#define FILE_KIND_DIRECTORY CYE_FILE_KIND_DIRECTORY
#define FILE_KIND_SYMLINK   CYE_FILE_KIND_SYMLINK
#define FILE_KIND_OTHER     CYE_FILE_KIND_OTHER
#define File_Kind           Cye_File_Kind

#define GLOB_SYNTAX_ERROR   CYE_GLOB_SYNTAX_ERROR,
#define GLOB_NO_MATCH       CYE_GLOB_NO_MATCH,
#define GLOB_MATCHED        CYE_GLOB_MATCHED
#define Match_Result        Cye_Match_Result

#define PATTERN_NO_ESCAPE   CYE_PATTERN_NO_ESCAPE
#define PATTERN_PATH        CYE_PATTERN_PATH
#define PATTERN_PERIOD      CYE_PATTERN_PERIOD
#define Pattern_Flags       Cye_Pattern_Flags

#define File_Stats          Cye_File_Stats
#define DString             Cye_DString

#define INVALID_PROCESS     CYE_INVALID_PROCESS
#define INVALID_FILE_HANDLE CYE_INVALID_FILE_HANDLE
#define Process             Cye_Process
#define File_Handle         Cye_File_Handle

#define Process_DArray      Cye_Process_DArray
#define Command             Cye_Command
#define Command_Redirect    Cye_Command_Redirect
#define String_Slice        Cye_String_Slice
#define String_Slice_DArray Cye_String_Slice_DArray
#define Context             Cye_Context

#define Glob_Filter_Data    Cye_Glob_Filter_Data

#define Pipe_Handle         Cye_Pipe_Handle
#define INVALID_PIPE_HANDLE CYE_INVALID_PIPE_HANDLE
#define Pipe                Cye_Pipe
#define INVALID_PIPE        CYE_INVALID_PIPE
#define Capture_Result      Cye_Capture_Result



//------------------------------------------------------------------------------------
//  Global Variables Short Names
//------------------------------------------------------------------------------------

#define temp_data           cye_temp_data
#define threshold_log_level cye_threshold_log_level
#define context             cye_context


//------------------------------------------------------------------------------------
//  Process and File Short Names
//------------------------------------------------------------------------------------
#define file_open_for_read  cye_file_open_for_read
#define file_open_for_write cye_file_open_for_write
#define file_close          cye_file_close

#define process_wait_all           cye_process_wait_all
#define process_wait_all_and_reset cye_process_wait_all_and_reset
#define process_wait               cye_process_wait


//------------------------------------------------------------------------------------
//  Commands Short Names
//------------------------------------------------------------------------------------
#define cmd_append                       cye_cmd_append

#define cmd_extend                       cye_cmd_extend
#define cmd_free                         cye_cmd_free

#define ds_write_cmd                     cye_ds_write_cmd

#define cmd_run_async                    cye_cmd_run_async
#define cmd_run_async_and_reset          cye_cmd_run_async_and_reset
#define cmd_run_async_redirect           cye_cmd_run_async_redirect
#define cmd_run_async_redirect_and_reset cye_cmd_run_async_redirect_and_reset

#define cmd_run_sync                     cye_cmd_run_sync
#define cmd_run_sync_and_reset           cye_cmd_run_sync_and_reset
#define cmd_run_sync_redirect            cye_cmd_run_sync_redirect
#define cmd_run_sync_redirect_and_reset  cye_cmd_run_sync_redirect_and_reset
#define cmd_run_sync_capture_and_reset   cye_cmd_run_sync_capture_and_reset



//------------------------------------------------------------------------------------
//  Storage Short Names
//------------------------------------------------------------------------------------


#define temp_context        cye_temp_context
#define default_context     cye_default_context
#define set_default_context cye_set_default_context

#define tstrdup     cye_tstrdup
#define talloc      cye_talloc
#define trealloc    cye_trealloc
#define tprintf     cye_tprintf

#define temp_reset   cye_temp_reset
#define temp_save    cye_temp_save
#define temp_restore cye_temp_restore

#define treset       cye_temp_reset
#define tsave        cye_temp_save
#define trestore     cye_temp_restore


//------------------------------------------------------------------------------------
//  Path Short Names
//------------------------------------------------------------------------------------

#define make_dir_if_not_exists          cye_make_dir_if_not_exists
#define copy_file                       cye_copy_file
#define copy_dir                        cye_copy_dir
#define read_dir                        cye_read_dir
#define read_dir_filtered               cye_read_dir_filtered

#define append_file                     cye_append_file
#define append_file_zstr                cye_append_file_zstr
#define write_file                      cye_write_file
#define write_file_zstr                 cye_write_file_zstr
#define read_file                       cye_read_file
#define ds_read_file                    cye_ds_read_file
#define path_file_kind                  cye_path_file_kind
#define path_temp_normalize             cye_path_temp_normalize
#define path_create_from_array          cye_path_create_from_array

#define path_create                     cye_path_create
#define path_temp_create                cye_path_temp_create


#define path_base_name                  cye_path_base_name
#define path_expand_user                cye_path_expand_user
#define path_expand_vars                cye_path_expand_vars

#define needs_rebuild_from_buf          cye_needs_rebuild_from_buf
#define needs_rebuild                   cye_needs_rebuild

#define path_temp_cwd                   cye_path_temp_cwd
#define path_set_cwd                    cye_path_set_cwd

#define file_exists                     cye_file_exists
#define file_stats                      cye_file_stats
#define is_absolute                     cye_is_absolute
#define is_relative                     cye_is_relative
#define is_file                         cye_is_file
#define is_dir                          cye_is_dir
#define is_executable                   cye_is_executable
#define find_executable                 cye_find_executable
#define is_period_dir                   cye_is_period_dir
#define is_link                         cye_is_link
#define is_mount                        cye_is_mount
#define is_same_path                    cye_is_same_path

#define path_join                       cye_path_join
#define path_size                       cye_path_size
#define path_real                       cye_path_real
#define path_absolute                   cye_path_absolute
#define path_relative                   cye_path_relative

#define path_home                       cye_path_home
#define path_cwd                        cye_path_cwd
#define path_parent                     cye_path_parent
#define path_owner                      cye_path_owner
#define path_stem                       cye_path_stem
#define path_dir_of                     cye_path_dir_of
#define path_ext                        cye_path_ext
#define path_touch                      cye_path_touch

#define make_dir                           cye_make_dir
#define make_dirs                          cye_make_dirs
#define make_dir_include_parents           cye_make_dir_include_parents
#define make_dir_include_parents_from_tstr cye_make_dir_include_parents_from_tstr
#define remove_file                        cye_remove_file
#define remove_dir                         cye_remove_dir
#define remove_dirs                        cye_remove_dirs
#define path_move                          cye_path_move
#define path_rename                        cye_path_rename
#define path_renames                       cye_path_renames
#define path_replace                       cye_path_replace
#define path_scandir                       cye_path_scandir

#define list_dir                           cye_list_dir
#define path_glob                          cye_path_glob
#define path_tglob                         cye_path_tglob

#define file_stats_fmt                     cye_file_stats_fmt
#define file_stats_fmt_arg                 cye_file_stats_fmt_arg

//------------------------------------------------------------------------------------
//  Pipe Short Names
//------------------------------------------------------------------------------------

#define pipe_open         cye_pipe_open
#define pipe_close        cye_pipe_close
#define is_pipe_valid     cye_is_pipe_valid
#define pipe_read         cye_pipe_read
#define pipe_write        cye_pipe_write
#define pipe_close_handle cye_pipe_close_handle



//------------------------------------------------------------------------------------
//  Dynamic Array Short Names
//------------------------------------------------------------------------------------
#define da_append      cye_da_append
#define da_remove      cye_da_remove
#define da_remove_item cye_da_remove_item
#define da_free        cye_da_free
#define da_append_many cye_da_append_buf //@deprecated: prefer _buf suffix
#define da_append_buf  cye_da_append_buf
#define da_fmt         cye_da_fmt
#define da_fmt_arg     cye_da_fmt_arg


//------------------------------------------------------------------------------------
//  Slices Short Names
//------------------------------------------------------------------------------------


#define Slice Cye_Slice

#define slice_make     cye_slice_make
#define slice_from_arr cye_slice_from_arr

#define slice_empty     cye_slice_empty
#define STR_SLICE_EMPTY CYE_STR_SLICE_EMPTY

#define slice_range    cye_slice_range
#define slice_prefix   cye_slice_prefix
#define slice_suffix   cye_slice_suffix
#define slice_equal    cye_slice_equal
#define slice_contains cye_slice_contains
#define slice_is_empty cye_slice_is_empty
#define slice_at       cye_slice_at
#define slice_copy     cye_slice_copy
#define slice_index_of cye_slice_index_of


#define slice_fmt     cye_slice_fmt
#define slice_fmt_arg cye_slice_fmt_arg
//------------------------------------------------------------------------------------
//  String Slice Short Names
//------------------------------------------------------------------------------------

#define str_slice_make             cye_str_slice_make
#define str_slice_from_zstr        cye_str_slice_from_zstr
#define str_slice_trim             cye_str_slice_trim
#define str_slice_to_zstr          cye_str_slice_to_zstr
#define str_slice_strip_left       cye_str_slice_strip_left
#define str_slice_strip_right      cye_str_slice_strip_right
#define str_slice_make_len         cye_str_slice_make_len
#define str_slice_equals           cye_str_slice_equals
#define str_slice_equals_zstr      cye_str_slice_equals_zstr
#define str_slice_contains         cye_str_slice_contains
#define str_slice_split            cye_str_slice_split
#define str_slice_split_zstr       cye_str_slice_split_zstr
#define str_slice_split_first      cye_str_slice_split_first
#define str_slice_starts_with      cye_str_slice_starts_with
#define str_slice_ends_with        cye_str_slice_ends_with
#define str_slice_ends_with_zstr   cye_str_slice_ends_with_zstr
#define str_slice_starts_with_zstr cye_str_slice_starts_with_zstr

#define ss_make             cye_str_slice_make
#define ss_from_zstr        str_slice_from_zstr
#define ss_trim             cye_str_slice_trim
#define ss_to_zstr          cye_str_slice_to_zstr
#define ss_strip_left       cye_str_slice_strip_left
#define ss_strip_right      cye_str_slice_strip_right
#define ss_make_len         cye_str_slice_make_len
#define ss_equals           cye_str_slice_equals
#define ss_equals_zstr      cye_str_slice_equals_zstr
#define ss_contains         cye_str_slice_contains
#define ss_split            cye_str_slice_split
#define ss_split_zstr       cye_str_slice_split_zstr
#define ss_split_first      cye_str_slice_split_first
#define ss_starts_with      cye_str_slice_starts_with
#define ss_ends_with        cye_str_slice_ends_with
#define ss_ends_with_zstr   cye_str_slice_ends_with_zstr
#define ss_starts_with_zstr cye_str_slice_starts_with_zstr

#define ss_fmt     cye_ss_fmt
#define ss_fmt_arg cye_ss_fmt_arg

//------------------------------------------------------------------------------------
//  ZString Short Names
//------------------------------------------------------------------------------------
#define zstr_ends_with     cye_zstr_ends_with
#define zstr_starts_with   cye_zstr_starts_with
#define zstr_match_pattern cye_zstr_match_pattern
#define zstr_ordinal       cye_zstr_ordinal

#define pattern_match          cye_pattern_match
#define is_pattern_well_formed cye_is_pattern_well_formed
#define glob_match             cye_glob_match


//----------------------------------------------------------------------------------
//  Dynamic String Short Names
//----------------------------------------------------------------------------------


#define ds_write_buf  cye_ds_write_buf
#define ds_write_zstr cye_ds_write_zstr
#define ds_write_zero cye_ds_write_zero
#define ds_write      cye_ds_write
#define ds_write_char cye_ds_write_char


// Free the memory allocated by a string builder
#define ds_clear  cye_ds_clear
#define ds_free   cye_ds_free
#define ds_printf cye_ds_printf

#define ds_fmt     cye_ds_fmt
#define ds_fmt_arg cye_ds_fmt_arg

//----------------------------------------------------------------------------------
//  Mathematics Short Names
//----------------------------------------------------------------------------------
// This are likely to have colisions
#ifndef min
#   define min          cye_min
#endif
#ifndef max
#   define max          cye_max
#endif

#define clamp        cye_clamp
#define lerp         cye_lerp
#define normalize    cye_normalize
#define remap        cye_remap
#define wrap         cye_wrap
#define float_equals cye_float_equals

//------------------------------------------------------------------------------------
//  Utils Short Names
//------------------------------------------------------------------------------------

#define set_trace_level cye_set_trace_level
#define trace_log       cye_trace_log
#define trace_info      cye_trace_info
#define trace_okay      cye_trace_okay
#define trace_error     cye_trace_error
#define trace_warn      cye_trace_warn
#define trace_fatal     cye_trace_fatal
#define trace_debug     cye_trace_debug

#define return_defer cye_return_defer
#define result_defer cye_result_defer

// Consider using logging instead ? Maybe not
#define todo            cye_todo
#define unreachable     cye_unreachable
#define panic           cye_panic
#define not_implemented cye_not_implemented
#define shift           cye_shift

#define assert_msg cye_assert_msg
#define assert cye_assert


#define file_stats_tstring cye_file_stats_tstring
#define str_slice_tstring  cye_str_slice_tstring
#define ds_tstring         cye_ds_tstring
#define tstring            cye_tstring

#define swap         cye_swap cye_swap
#define sort_reverse cye_sort_reverse
#define sort         cye_sort
#define sort_q       cye_sort_q
#define bubble_sort  cye_bubble_sort
#define quick_sort   cye_quick_sort

#define cpu_architecture *cye_cpu_architecture

#define file_fmt     cye_file_fmt
#define file_fmt_arg cye_file_fmt_arg
#define fmt          cye_fmt


#endif // CYE_NO_SHORT_NAMES

#endif // _CYE_NO_SHORT_NAMES_GUARD_

//TODO: Make a localized space for common undef to helpout when undefs are needed
// make in a way that it doesnt trigger any warning from the compiler ok?

// EOF


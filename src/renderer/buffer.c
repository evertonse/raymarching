typedef enum {
   BUFFER_TYPE_NONE,
   BUFFER_TYPE_VERTEX,
   BUFFER_TYPE_INDEX,
   BUFFER_TYPE_UNIFORM,
   BUFFER_TYPE_STORAGE,
   BUFFER_TYPE_TEXTURE_BUFFER,
} Buffer_Type;

// NOTE: What I like about not using enum flags is that the full state is valid, from the user's point of view you choose 1 single enum and go to town as theres no possibily of incorrect flags combination.
typedef enum {
    // Mutually exclusive
    BUFFER_USAGE_STATIC,
    BUFFER_USAGE_SUBDATA,
    BUFFER_USAGE_ORPHANABLE, // Old opengl api had this concept i'll leave here for performance testing
    BUFFER_USAGE_MAP_READ,
    BUFFER_USAGE_MAP_WRITE,
    BUFFER_USAGE_MAP_READ_WRITE,
    BUFFER_USAGE_MAP_PERSISTENT_READ,
    BUFFER_USAGE_MAP_PERSISTENT_WRITE,
    BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE,
} Buffer_Usage;

typedef struct {
   u32   handle;
   isz   size;
   u32   binding;
   void* mapped_ptr;
   Buffer_Usage usage;
   Buffer_Type  type;
} Buffer;

// NOTE: Most of these are unused actually. We can get away with just some images and a buffer interface and thats it, no need to especialist.
// They're probably gonna be gone soon

//--------------------------------------
// Texture Buffer Object (Texture_Buffer)
//--------------------------------------
typedef struct {
    Texture texture;
    Buffer  buffer;
} Texture_Buffer;

//--------------------------------------
// Uniform Buffer Object (Uniform_Buffer)
//--------------------------------------
typedef struct {
    Buffer buffer;
    isz offset;  // How data we pushed (current cursor)
    u8* cpu_mem;
} Uniform_Buffer;

typedef struct {
    Buffer buffer;
    isz count; // How many indices
               // Maybe add a type to support short 16 bit indices
} Index_Buffer;

typedef struct {
    Buffer buffer;
    isz count;   // How many vertex's
} Vertex_Buffer;

bool is_valid_buffer(const Buffer b) {
   if (b.handle == 0) {
      return false;
   }
   bool size_ok = b.size > 0;

// In debug also check that the size matches with OpenGL's opinion
#if defined(RENDERER_DEBUG)
   GLint size = 0;
   glGetNamedBufferParameteriv(b.handle, GL_BUFFER_SIZE, &size);
   size_ok = size_ok && (size == b.size);
#endif
  return size_ok;
}

Buffer create_buffer(Buffer_Usage usage, const void *data, isz size) {
   Buffer buffer = {0};
   if (size <= 0) {
      trace_warn("Trying to create a 0 sized buffer from pointer %p. Really human?", data);
      return (Buffer){0};
   }
   buffer.type       = BUFFER_TYPE_NONE;
   buffer.binding    = -1;
   buffer.size       = size;
   buffer.usage      = usage;
   buffer.mapped_ptr = nullptr;

   GLbitfield storage_flags = 0;
   GLbitfield map_flags     = 0;
   bool       should_map    = false;
   bool       use_mutable   = false;

   // Determine appropriate flags based on usage
   switch (usage) {
      case BUFFER_USAGE_STATIC: {
         storage_flags = 0; // immutable, best for static data
         map_flags     = 0;
         should_map    = false;
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_SUBDATA: {
         storage_flags = GL_DYNAMIC_STORAGE_BIT;
         map_flags     = 0;
         should_map    = false;
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_ORPHANABLE: {
         storage_flags = 0;
         map_flags     = 0;
         should_map    = false;
         use_mutable   = true;
         break;
      }
      case BUFFER_USAGE_MAP_READ: {
         storage_flags = GL_MAP_READ_BIT;
         map_flags     = GL_MAP_READ_BIT;
         should_map    = false; // Not persistent, user maps later
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_MAP_WRITE: {
         storage_flags = GL_MAP_WRITE_BIT;
         map_flags     = GL_MAP_WRITE_BIT;
         should_map    = false; // Not persistent, user maps later
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_MAP_READ_WRITE: {
         storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
         map_flags     = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
         should_map    = false; // Not persistent, user maps later
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_MAP_PERSISTENT_READ: {
         storage_flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         map_flags     = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         should_map    = true; // Persistent, map immediately
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_MAP_PERSISTENT_WRITE: {
         storage_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         map_flags     = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         should_map    = true; // Persistent, map immediately
         use_mutable   = false;
         break;
      }
      case BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE: {
         storage_flags = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         map_flags     = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
         should_map    = true; // Persistent, map immediately
         use_mutable   = false;
         break;
      }
      default: {
         assert_msg(0, "Invalid Buffer_Usage value.");
      } break;
   }

   glCreateBuffers(1, &buffer.handle);

   if (use_mutable) {
      // Use mutable storage for resizable buffers (allows orphaning)
      glNamedBufferData(buffer.handle, size, data, GL_DYNAMIC_DRAW);
   } else {
      // Use immutable storage for non-resizable buffers
      glNamedBufferStorage(buffer.handle, size, data, storage_flags);
   }

   // Map only for persistent mappings
   if (should_map) {
      buffer.mapped_ptr = glMapNamedBufferRange(buffer.handle, 0, size, map_flags);
      assert_msg(buffer.mapped_ptr != nullptr, "Failed to map buffer");
   }

   return buffer;
}


void volatile *map_buffer(Buffer *buffer) {
   assert(buffer && is_valid_buffer(*buffer));

   // Return existing mapping if already mapped
   if (buffer->mapped_ptr != nullptr) {
      return buffer->mapped_ptr;
   }

   // Determine access flags based on buffer usage
   GLbitfield access = 0;
   switch (buffer->usage) {
   case BUFFER_USAGE_MAP_READ:
   case BUFFER_USAGE_MAP_PERSISTENT_READ:
      access = GL_MAP_READ_BIT;
      break;

   case BUFFER_USAGE_MAP_WRITE:
   case BUFFER_USAGE_MAP_PERSISTENT_WRITE:
      access = GL_MAP_WRITE_BIT;
      break;

   case BUFFER_USAGE_MAP_READ_WRITE:
   case BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE:
      access = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
      break;

   default:
      assert_msg(0, "Buffer usage does not support mapping: %d", buffer->usage);
      return nullptr;
   }

   if (buffer->usage == BUFFER_USAGE_MAP_PERSISTENT_READ || buffer->usage == BUFFER_USAGE_MAP_PERSISTENT_WRITE || buffer->usage == BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE) {
      access |= GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
   }

   buffer->mapped_ptr = glMapNamedBufferRange(buffer->handle, 0, buffer->size, access);

   if (buffer->mapped_ptr == nullptr) {
      GLenum error = glGetError();
      assert_msg(0, "Failed to map buffer. OpenGL error: 0x%x", error);
   }

   return buffer->mapped_ptr;
}

void unmap_buffer(Buffer* buffer) {
   assert(buffer && is_valid_buffer(*buffer));

   if (nullptr != buffer->mapped_ptr) {
      GLboolean success = glUnmapNamedBuffer(buffer->handle);
      buffer->mapped_ptr = nullptr;

      if (GL_FALSE == success) {
         trace_warn("Buffer unmapping failed - data may be corrupted");
      }
   }
}

void destroy_buffer(Buffer* buffer) {
   if (!buffer) {
      trace_warn("Don't you dare try to destroy a null buffer, this incident will be reported.");
   }
   if (buffer && !is_valid_buffer(*buffer)) {
      return; // Already destroyed or invalid
   }

   if (buffer->mapped_ptr != nullptr) {
      unmap_buffer(buffer);
   }

   glDeleteBuffers(1, &buffer->handle);

   *buffer = (Buffer){0};
}

Buffer create_buffer_copy(const Buffer *source) {
   assert(source && is_valid_buffer(*source));

   Buffer result = {0};

   result = create_buffer(source->usage, nullptr, source->size);

   result.type    = source->type;
   result.binding = source->binding;

   // Copy data directly on GPU
   glCopyNamedBufferSubData(source->handle, // Source buffer
                            result.handle,  // Destination buffer
                            0,              // Source offset
                            0,              // Destination offset
                            source->size    // Size in bytes
   );

   GLenum error = glGetError();
   if (error != GL_NO_ERROR) {
       assert_msg(0, "Failed to copy buffer data. OpenGL error: 0x%x", error);
      // If copy failed, clean up and return invalid buffer
      destroy_buffer(&result);
      result = (Buffer){0};
   }

   return result;
}

bool copy_from_buffer(
      Buffer *destination,  isz destination_offset,
      const Buffer *source, isz source_offset,
      isz size_in_bytes_to_copy
) {
   assert(source && is_valid_buffer(*source));
   assert(destination && is_valid_buffer(*destination));
   isz size = size_in_bytes_to_copy;

   // Range checks
   if (  source_offset      + size > source->size
      || destination_offset + size > destination->size
   ) {
      trace_error("%s: out of range copy requested", __func__);
      return false;
   }


   // Copy data directly on GPU
   glCopyNamedBufferSubData(
      source->handle,       // source buffer
      destination->handle,  // destination buffer
      source_offset,        // read from source offset
      destination_offset,   // write to destination offset
      size                  // size in bytes
   );

   GLenum error = glGetError();
   if (error != GL_NO_ERROR) {
       trace_error("Failed to copy buffer data. OpenGL error: 0x%x", error);
       return false;
   }

   return true;
}

// You do this by creating a fence object. This is a token in the command stream that you can test to see if it has been completed.
// Since the stream is an ordered list, if the fence has completed, then every command issued before that fence was issued has also completed.
// Sync objects have a specific type, which defines their signaling behavior. Currently, there is only one type: fences (since opengl is done being updated we're never gonna get another type xD).
GLsync sync_point(GLsync sync) {
   if (sync) {
      glDeleteSync(sync);
   }
   // The only available value for condition GL_SYNC_GPU_COMMANDS_COMPLETE
   // Currently, the flags field has no possible parameters; it should be 0. The field exists in case of future extensions to this functionality.
   sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
   assert(glIsSync(sync) == GL_TRUE);
   return sync;
}

void wait_sync_point(GLsync sync) {
   if (sync == nullptr) {
      return;
   }
   static constexpr isz max_tries = 800;
   static constexpr usz timeout_ns = 1;
   GLenum wait = 0;
   for (isz idx = 0; true || idx < max_tries; idx++) {
      // This function will not return until one of two things happens: the sync object parameter becomes signaled, or a number of nanoseconds greater than or equal to the timeout parameter passes
      wait = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, timeout_ns);
      if (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED) {
         trace_debug("We ball we this (wait == GL_ALREADY_SIGNALED || wait == GL_CONDITION_SATISFIED)");
         return;
      } else if (wait == GL_TIMEOUT_EXPIRED) {
         // trace_warn("Client Wait timedout (set to %d nanoseconds).", timeout_ns);
      } else if (wait == GL_WAIT_FAILED) {
         trace_debug("Client Wait Failed");
      }
   }
   trace_warn("Client Wait surpassed max tries (%d) each with a timeout of %d ns.", max_tries, timeout_ns);
}


bool is_valid_uniform_buffer(const Uniform_Buffer ub) {
    return is_valid_buffer(ub.buffer) && ub.cpu_mem;
}

bool is_valid_texture_buffer(const Texture_Buffer tb) {

#if defined(RENDERER_DEBUG)
    if (!is_valid_texture(tb.texture) || !is_valid_buffer(tb.buffer)) return false;
    if (tb.texture.handle == 0 || tb.buffer.handle == 0) return false;

    // Check texture buffer type
    GLint type;
    glGetTextureLevelParameteriv(tb.texture.handle, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &type);
    return type != 0;
#else
    return tb.texture.handle && tb.buffer.handle;
#endif
}

inline bool is_valid_index_buffer(const Index_Buffer ib) {
    return is_valid_buffer(ib.buffer) && ib.count > 0;
}

inline bool is_valid_vertex_buffer(const Vertex_Buffer vb) {
    return is_valid_buffer(vb.buffer) && vb.count > 0;
}


// Return the index of one position after the last byte written;
isz update_buffer(const Buffer *buffer, const void *data, isz offset, isz size) {
   assert(buffer);
   if (!is_valid_buffer(*(Buffer*)buffer)) {
      trace_error("Buffer invalid, %s denied.", __func__);
      // debug_break();
      // __debugbreak();
      __builtin_trap();
      return offset;
   }

   if (0 == size) {
      trace_warn("You might have switched offset with size. I ain't creating types for different index for type safety. Or maybe you wanna update 0 bytes, who knows, imma just be annoying to let you know.");
      return offset;
   }

   if (offset + size > buffer->size) {
      trace_error("Buffer write would exceed bounds. No data was written, fix your bounds.");
      return offset;
   }

   if (buffer->mapped_ptr) {
      assert(
            BUFFER_USAGE_MAP_READ                  == buffer->usage
         || BUFFER_USAGE_MAP_WRITE                 == buffer->usage
         || BUFFER_USAGE_MAP_READ_WRITE            == buffer->usage
         || BUFFER_USAGE_MAP_PERSISTENT_READ       == buffer->usage
         || BUFFER_USAGE_MAP_PERSISTENT_WRITE      == buffer->usage
         || BUFFER_USAGE_MAP_PERSISTENT_READ_WRITE == buffer->usage
      );
      memcpy((char *)buffer->mapped_ptr + offset, data, size);
   } else {
      glNamedBufferSubData(buffer->handle, offset, size, data);
   }
   return offset + size;
}

//
// NOTE: The data in now undefined after calling this resize function. Maybe we can try to do a realloc, but
//       for now just assume that it's lost. Caller probably have the data on cpu somewhere and can better judge.
//
// Buffer growth using orphaning if resize is in its usage, otherwise destroy the earlier buffer
// its destroyed and return a new buffer with same characteristics with the new required size
bool resize_buffer_if_needed(Buffer *buffer, isz required_size) {
    assert(buffer);
    if (!is_valid_buffer(*buffer)) {
      trace_warn("Buffer can't weasel your way outta calling 'create_buffer' with a cheeky resize on a invalid buffer mate, nt tho.");
      return false;
    }

   // Check for shrinking
   if (required_size < buffer->size) {
      trace_warn("Buffer shrinking not supported. Current size: %zu, requested size: %zu", buffer->size, required_size);
      return false;
   }

   // If buffer is already large enough, no resize needed
   if (buffer->size >= required_size) {
      return true;
   }

   bool is_orphanable = buffer->usage == BUFFER_USAGE_ORPHANABLE;
   if (is_orphanable) {
      assert_msg(buffer->mapped_ptr == nullptr, "Buffers with orphaning shouldn't be mapped");
      // Use orphaning reallocate the same buffer with new size
      glNamedBufferData(buffer->handle, required_size, nullptr, GL_DYNAMIC_DRAW);
      buffer->size = required_size;

   } else {
      // Case when useing buffer with newer storage OpenGL API

      // Store the old buffer properties
      Buffer_Usage old_usage = buffer->usage;
      Buffer_Type  old_type  = buffer->type;
      u32 old_binding        = buffer->binding;

      // Destroy the old buffer
      destroy_buffer(buffer);

      // Create new buffer with same usage but new size, and obviously the handle
      Buffer new_buffer  = create_buffer(old_usage, nullptr, required_size);
      new_buffer.type    = old_type;
      new_buffer.binding = old_binding;

      // Replace the original buffer in-place
      *buffer = new_buffer;
   }
   return true;
}

void bind_buffer(Buffer* buffer, Buffer_Type type, i64 binding) {
   GLenum target = 0;

   switch (type) {
      case BUFFER_TYPE_UNIFORM: target = GL_UNIFORM_BUFFER; break;
      case BUFFER_TYPE_STORAGE: target = GL_SHADER_STORAGE_BUFFER; break;
      default: return; // Not bindable
   }

   glBindBufferBase(target, binding, buffer->handle);
   buffer->binding = binding;
   buffer->type    = type;
}

void bind_buffer_draw_indirect(Buffer* buffer) {
   glBindBuffer(GL_DRAW_INDIRECT_BUFFER, buffer->handle);
}

void bind_buffer_view(Buffer *buffer, Buffer_Type type, isz binding, isz offset, isz size) {
   if (!buffer || buffer->handle == 0 || size <= 0) {
      // Just ignore basically
      trace_debug("%s Invalid buffer or size.\n", __func__);
      return;
   }

   GLenum target = 0;
   switch (type) {
   case BUFFER_TYPE_UNIFORM:
      target = GL_UNIFORM_BUFFER;
      break;
   case BUFFER_TYPE_STORAGE:
      target = GL_SHADER_STORAGE_BUFFER;
      break;
   default:
      trace_warn("%s Invalid buffer or size.\n Unsupported buffer type. %d", __func__, type);
      return;
   }

   glBindBufferRange(target, binding, buffer->handle, offset, size);
   buffer->binding = binding;
   buffer->type = type;
}

inline void delete_texture_buffer(Texture_Buffer* buffer) {
    destroy_texture(&buffer->texture);
    destroy_buffer(&buffer->buffer);
    *buffer = (Texture_Buffer){0};
}

void attach_buffer_to_texture(const Texture* texture, const Buffer* buffer) {
   if (texture->type != TEXTURE_TYPE_BUFFER) return;
   if (buffer->type  != BUFFER_TYPE_TEXTURE_BUFFER) return;

   GLenum internal_format;

   switch (texture->format) {
      case TEXTURE_FORMAT_RGBA8: {
         internal_format = GL_RGBA8;
         break;
      }
      case TEXTURE_FORMAT_RGB8: {
         internal_format = GL_RGB8;
         break;
      }
      case TEXTURE_FORMAT_RG8: {
         internal_format = GL_RG8;
         break;
      }
      case TEXTURE_FORMAT_R8: {
         internal_format = GL_R8;
         break;
      }
      case TEXTURE_FORMAT_RGBA32F: {
         internal_format = GL_RGBA32F;
         break;
      }
      case TEXTURE_FORMAT_DEPTH24: {
         internal_format = GL_DEPTH_COMPONENT24;
         break;
      }
      case TEXTURE_FORMAT_SHADOW: {
         internal_format = GL_DEPTH_COMPONENT24;
         break;
      }
      default: {
         assert_msg(false, "Unsupported texture format\n");
         return;
      }
   }
   glTextureBuffer(texture->handle, internal_format, buffer->handle);
}

Texture_Buffer create_texture_buffer(
    isz size, const void* data,
    Texture_Format format,
    i64 binding // Not used in this case
) {
    Texture_Buffer result = {0};

    result.buffer = create_buffer(
        BUFFER_USAGE_STATIC,
        data,
        size
    );

    result.texture = create_texture_extended(
        0, 0, nullptr,
        format,
        TEXTURE_TYPE_BUFFER,
        0
    );

    // Associate the buffer with the texture
    GLenum gl_internal_format = 0;
    switch (format) {
        case TEXTURE_FORMAT_RGBA32F: gl_internal_format = GL_RGBA32F; break;
        case TEXTURE_FORMAT_RGBA8:   gl_internal_format = GL_RGBA8;   break;
        case TEXTURE_FORMAT_RGB8:    gl_internal_format = GL_RGB8;    break;
        case TEXTURE_FORMAT_RG8:     gl_internal_format = GL_RG8;     break;
        case TEXTURE_FORMAT_R8:      gl_internal_format = GL_R8;      break;
        default:
            assert_msg(0, "Unsupported texture format for buffer");
        }

    glTextureBuffer(result.texture.handle, gl_internal_format, result.buffer.handle);
    return result;
}



// Shit's unused and overengineering.
#define STD140_ALIGN __attribute__((aligned(16)))
Uniform_Buffer create_uniform_buffer(isz size, i64 binding) {
    Uniform_Buffer result = {0};

    result.buffer = create_buffer(
        BUFFER_USAGE_SUBDATA,
        nullptr,
        size
    );

    result.offset = 0;
    result.cpu_mem = malloc(size); // Push data here and upload all at once
    return result;
}


// You might divy up the array of vertex in diffent ways, so we use vertices_count allow you to query how many was it. But the buffer is always buffer_size, not taking into account vertinces_count
Vertex_Buffer create_vertex_buffer(const void* data, isz buffer_size, isz vertices_count) {
    Vertex_Buffer result = {0};
    result.count = vertices_count;

    result.buffer = create_buffer(
        BUFFER_USAGE_SUBDATA,
        data,
        buffer_size
    );

    return result;
}

Index_Buffer create_index_buffer(const u32* data, isz index_count) {
    Index_Buffer result = {0};
    isz size = index_count * size_of(*data);
    result.count = index_count;
    assert_msg(data, "We're using static memory, that means we can't update it, so we need to set it once, meaning right now!");

    result.buffer = create_buffer(
        BUFFER_USAGE_STATIC,
        data,
        size
    );

    return result;
}

//--------------------------------------
// Some Notes for me to remember how to use it from Shaders:
//--------------------------------------

// Shader Binding Usage
// - Uniform_Buffer: layout(std140, binding = N) uniform BlockName {}
// - Texture_Buffer: uniform samplerBuffer texBuffer;
// - Storage_Buffer: layout(std430, binding = N) buffer BlockName {}
// - Image2D       : layout(rgba32f, binding = N) uniform image2D myImage;
// - Texture2D     : uniform sampler2D tex;

// Image2D vs Regular Texture:
// - Image2D supports read AND write operations from shaders (imageLoad/imageStore).
// - Regular Texture (sampler2D) is read-only and supports filtering and mipmaps.
// - You can bind the same GL_TEXTURE_2D to both image2D and sampler2D with different usage (e.g. bind to both for writing and sampling).

// Uniform vs Texture vs Storage_Buffer:
// - Uniform Buffer: Fast, small data, 16-byte alignment. Shared across programs. Limited size (64KB).
// - Texture Buffer: 1D only, best for tightly packed uniform-like arrays. Read-only in shaders.
// - Storage_Buffer: Most flexible, larger storage, can read-write. Slower than Uniform_Buffers for small data.
// - Image2D       : Arbitrary read/write, good for post-processing passes. Requires memory barriers.

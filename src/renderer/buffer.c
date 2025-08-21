typedef enum {
   BUFFER_TYPE_NONE,
   BUFFER_TYPE_VERTEX,
   BUFFER_TYPE_INDEX,
   BUFFER_TYPE_UNIFORM,
   BUFFER_TYPE_STORAGE,
   BUFFER_TYPE_TEXTURE_BUFFER,
   // BUFFER_TYPE_DRAW_COMMAND,
} Buffer_Type;

// NOTE: What I like about not using enum flags is that the full state is valid, from the user's point of view you choose 1 single enum and go to town as theres no possibily of incorrect flags combination.
typedef enum {
    // TODO: Is it ever necessary to make it mappable but not dynamic?
    BUFFER_USAGE_DYNAMIC,              // Dynamic means can update with subdata calls, but is not mappable
    BUFFER_USAGE_DYNAMIC_READ,         // Dynamic but can map for reading only
    BUFFER_USAGE_DYNAMIC_WRITE,        // Dynamic but can map for writing only
    BUFFER_USAGE_DYNAMIC_READ_WRITE,   // Dynamic, can map for read and write
    BUFFER_USAGE_STATIC,               // GPU only memory, can ever be changed after is set, might allow some optimizations
    BUFFER_USAGE_PERSISTENT,           // Mapped always and coherent
    BUFFER_USAGE_PERSISTENT_READ_ONLY, // Mapped always
} Buffer_Usage;



typedef enum {
    DATA_TYPE_FLOAT,
    DATA_TYPE_VEC2,
    DATA_TYPE_VEC3,
    DATA_TYPE_VEC4,
    DATA_TYPE_MAT4,
    DATA_TYPE_FLOAT_ARRAY,
    DATA_TYPE_VEC2_ARRAY,
    DATA_TYPE_VEC3_ARRAY,
    DATA_TYPE_VEC4_ARRAY,
    DATA_TYPE_MAT4_ARRAY,
} Data_Type;

typedef struct {
   u32   handle;
   isz   size;
   u32   binding;    // Binding point      for      ub/SSBO
   void* mapped_ptr; // For     persistent mappings or regular old-ass mapping
   Buffer_Usage usage;
   Buffer_Type  type;
} Buffer;



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

//--------------------------------------
// Shader Storage Buffer Object (Storage_Buffer)
//--------------------------------------
typedef struct {
    Buffer buffer;
} Storage_Buffer;

// TODO: By defauled we should just have a create buffer that takes  usage data and size, let the user decide the binding point whenever and also what type it is shouldn't concern us
Buffer create_buffer_extended(Buffer_Type type, Buffer_Usage usage, const void *data, isz size, i64 binding) {
    Buffer buf = {0};
    buf.type = type;
    buf.binding = binding;
    buf.size = size;
    buf.usage = usage;
    buf.mapped_ptr = NULL;

    GLbitfield storage_flags = 0;
    GLbitfield map_flags = 0;
    bool should_map = false;

    // Determine appropriate flags based on usage
    switch (usage) {
        case BUFFER_USAGE_DYNAMIC: {
           storage_flags = GL_DYNAMIC_STORAGE_BIT;
           map_flags     = 0;
           should_map    = false;
        } break;

        case BUFFER_USAGE_DYNAMIC_WRITE: {
           storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_WRITE_BIT;
           map_flags     = GL_MAP_WRITE_BIT;
           should_map    = true;
        } break;

        case BUFFER_USAGE_DYNAMIC_READ: {
           storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT;
           map_flags     = GL_MAP_READ_BIT;
           should_map    = true;
        } break;

        case BUFFER_USAGE_DYNAMIC_READ_WRITE: {
           storage_flags = GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
           map_flags     = GL_MAP_READ_BIT | GL_MAP_WRITE_BIT;
           should_map    = true;
        } break;

        case BUFFER_USAGE_STATIC: {
           storage_flags = 0; // immutable, best for static data
           map_flags     = 0;
           should_map    = false;
        } break;

        case BUFFER_USAGE_PERSISTENT: {
           storage_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
           map_flags     = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
           should_map    = true;
        } break;

        case BUFFER_USAGE_PERSISTENT_READ_ONLY: {
           storage_flags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT ;
           map_flags     = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT;
           should_map    = true;
        } break;

        default: {
            assert_msg(0, "Invalid Buffer_Usage value.");
        } break;
    }

    // Create and allocate buffer
    glCreateBuffers(1, &buf.handle);
    glNamedBufferStorage(buf.handle, size, data, storage_flags);

    // Map if needed
    if (should_map) {
        buf.mapped_ptr = glMapNamedBufferRange(buf.handle, 0, size, map_flags);
    }

    // Bind to UBO/SSBO if necessary
    GLenum target = 0;
    switch (type) {
        case BUFFER_TYPE_UNIFORM: target = GL_UNIFORM_BUFFER; break;
        case BUFFER_TYPE_STORAGE: target = GL_SHADER_STORAGE_BUFFER; break;
        default: break; // No binding needed for others (Vertex, Index, Texture buffers)
    }

    // TODO: check for maximum biding allowed by the opengl implementation in this machine and tell the user
    if (binding != -1 && target != 0) {
        glBindBufferBase(target, binding, buf.handle);
    }

    return buf;
}

Buffer create_buffer(const void *data, isz size) {
    return create_buffer_extended(BUFFER_TYPE_NONE, BUFFER_USAGE_DYNAMIC_READ_WRITE, data,  size, -1);
}

Buffer create_buffer_copy(const Buffer *source, Buffer_Usage usage) {
  assert(source && source->size > 0);
  Buffer result = {0};
  result = create_buffer_extended(source->type, usage, NULL,  source->size, source->binding);
  // Copy data directly on GPU
  glCopyNamedBufferSubData(source->handle, // Source buffer
                           result.handle,  // Destination buffer
                           0,              // Source offset
                           0,              // Destination offset
                           source->size    // Size in bytes
  );

  return result;
}

// You do this by creating a fence object. This is a token in the command stream that you can test to see if it has been completed. 
// Since the stream is an ordered list, if the fence has completed, then every command issued before that fence was issued has also completed.
// Sync objects have a specific type, which defines their signaling behavior. Currently, there is only one type: fences.
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

bool is_valid_buffer(const Buffer b) {
#if 1 || defined(DEBUG)
    if (b.handle == 0) return false;

    GLint size = 0;
    glGetNamedBufferParameteriv(b.handle, GL_BUFFER_SIZE, &size);
    return size > 0;
#else
    return b && b.handle != 0;
#endif
}

bool is_valid_uniform_buffer(const Uniform_Buffer ub) {
    return is_valid_buffer(ub.buffer) && ub.cpu_mem;
}

bool is_valid_storage_buffer(const Storage_Buffer sb) {
    return is_valid_buffer(sb.buffer);
}

bool is_valid_texture_buffer(const Texture_Buffer tb) {

#if 1 || defined(DEBUG)
    if (!is_valid_texture(tb.texture) || !is_valid_buffer(tb.buffer)) return false;
    if (tb.texture.handle == 0 || tb.buffer.handle == 0) return false;

    // Check texture buffer type
    GLint type;
    glGetTextureLevelParameteriv(tb.texture.handle, 0, GL_TEXTURE_BUFFER_DATA_STORE_BINDING, &type);
    return type != 0;
#else
    return tb && tb.texture.handle && tb.buffer.handle;
#endif
}

inline bool is_valid_index_buffer(const Index_Buffer ib) {
    return is_valid_buffer(ib.buffer) && ib.count > 0;
}

inline bool is_valid_vertex_buffer(const Vertex_Buffer vb) {
    return is_valid_buffer(vb.buffer) && vb.count > 0;
}



// Return the index of one position after the last byte written;
isz update_buffer_(const Buffer buf, const void* data, isz size, isz offset) {
    glNamedBufferSubData(buf.handle, offset, size, data);
    return offset + size;
}

isz update_buffer_mapped_ptr(const Buffer buf, const void* data, isz size, isz offset) {
    // Optional: Add bounds checking if you store buffer size in Buffer struct
    #ifdef DEBUG
    if (offset + size > buf.size) {
        // Handle error - could assert, return error code, etc.
        assert(0 && "Buffer write would exceed bounds");
        return offset; // Return unchanged offset on error
    }
    #endif
    memcpy((char*)buf.mapped_ptr + offset, data, size);
    return offset + size;
}

isz update_buffer(const void* buffer, const void* data, isz size, isz offset) {
    const Buffer *buf = buffer;
    assert(buf);

    if (buf->mapped_ptr) {
        assert(buf->usage == BUFFER_USAGE_PERSISTENT);
        memcpy((char*)buf->mapped_ptr + offset, data, size);
    } else {
        glNamedBufferSubData(buf->handle, offset, size, data);
    }
    return offset + size;
}


// FIX gl explosed
void* map_buffer(Buffer* buf, GLbitfield access) {
   buf->mapped_ptr = glMapNamedBuffer(buf->handle, access);
   return buf->mapped_ptr;
}

void unmap_buffer(Buffer* buf) {
    glUnmapNamedBuffer(buf->handle);
    buf->mapped_ptr = nullptr;
}

void destroy_buffer(Buffer* buf) {
    glDeleteBuffers(1, &buf->handle);
    if (buf->mapped_ptr) {
        unmap_buffer(buf);
    }
    *buf = (Buffer){0};
}

void bind_buffer(Buffer* buf, i64 binding) {
    GLenum target = 0;

    switch (buf->type) {
    case BUFFER_TYPE_UNIFORM: target = GL_UNIFORM_BUFFER; break;
    case BUFFER_TYPE_STORAGE: target = GL_SHADER_STORAGE_BUFFER; break;
    default: return; // Not bindable
    }

    glBindBufferBase(target, binding, buf->handle);
    buf->binding = binding;
}

void bind_buffer_as_type(Buffer* buf, Buffer_Type type, i64 binding) {
    GLenum target = 0;

    switch (type) {
    case BUFFER_TYPE_UNIFORM: target = GL_UNIFORM_BUFFER; break;
    case BUFFER_TYPE_STORAGE: target = GL_SHADER_STORAGE_BUFFER; break;
    default: return; // Not bindable
    }

    glBindBufferBase(target, binding, buf->handle);
    buf->binding = binding;
    buf->type = type;
}


void bind_buffer_slice(const Buffer* buf, isz size, isz offset) {
    GLenum target = 0;
    switch (buf->type) {
    case BUFFER_TYPE_UNIFORM: target = GL_UNIFORM_BUFFER; break;
    case BUFFER_TYPE_STORAGE: target = GL_SHADER_STORAGE_BUFFER; break;
    default: return; // Not bindable
    }
    glBindBufferRange(target, buf->binding, buf->handle, offset, size);
}

void bind_buffer_slice_as_type(Buffer* buf, Buffer_Type type, isz binding, isz size, isz offset) {
    if (!buf || buf->handle == 0 || size <= 0) {
        trace_error("bind_buffer_slice_as_type: Invalid buffer or size.\n");
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
        trace_warn("bind_buffer_slice_as_type: Unsupported buffer type (%d).\n", type);
        return;
    }

    glBindBufferRange(target, binding, buf->handle, offset, size);
    buf->binding = binding;
    buf->type = type;
}



inline void delete_texture_buffer(Texture_Buffer* buf) {
    destroy_texture(&buf->texture);
    destroy_buffer(&buf->buffer);
    *buf = (Texture_Buffer){0};
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

    result.buffer = create_buffer_extended(
        BUFFER_TYPE_TEXTURE_BUFFER,
        BUFFER_USAGE_STATIC,
        data,
        size,
        -1 // Texture buffer doesn't use ub/SSBO binding points
    );

    result.texture = create_texture_extended(
        0, 0, NULL,
        format,
        TEXTURE_TYPE_BUFFER,
        1
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



#define STD140_ALIGN __attribute__((aligned(16)))
Uniform_Buffer create_uniform_buffer(isz size, i64 binding) {
    Uniform_Buffer result = {0};

    result.buffer = create_buffer_extended(
        BUFFER_TYPE_UNIFORM,
        BUFFER_USAGE_DYNAMIC,
        NULL,
        size,
        binding
    );

    result.offset = 0;
    result.cpu_mem = malloc(size); // Push data here and upload all at once
    return result;
}

Storage_Buffer create_storage_buffer(isz size, i64 binding, const void* data, bool persistent) {
    Storage_Buffer result = {0};
    result.buffer = create_buffer_extended(
        BUFFER_TYPE_STORAGE,
        BUFFER_USAGE_DYNAMIC,
        data,
        size,
        binding
    );
    return result;
}

// You might divy up the array of vertex in diffent ways, so we use vertices_count allow you to query how many was it. But the buffer is always buffer_size, not taking into account vertinces_count
Vertex_Buffer create_vertex_buffer(const void* data, isz buffer_size, isz vertices_count) {
    Vertex_Buffer result = {0};
    result.count = vertices_count;

    result.buffer = create_buffer_extended(
        BUFFER_TYPE_VERTEX,
        BUFFER_USAGE_DYNAMIC,
        data,
        buffer_size,
        -1
    );

    return result;
}

Index_Buffer create_index_buffer(const u32* data, isz index_count) {
    Index_Buffer result = {0};
    isz size = index_count * size_of(*data);
    result.count = index_count;
    assert_msg(data, "We're using static memory, that means we can't update it, so we need to set it once, meaning right now!");

    result.buffer = create_buffer_extended(
        BUFFER_TYPE_INDEX,
        BUFFER_USAGE_STATIC,
        data,
        size,
        -1
    );

    return result;
}

static isz std140_base_alignment(Data_Type type) {
    switch (type) {
        case DATA_TYPE_FLOAT:       return 4;
        case DATA_TYPE_VEC2:        return 8;
        case DATA_TYPE_VEC3:
        case DATA_TYPE_VEC4:        return 16;
        case DATA_TYPE_MAT4:        return 16;

        case DATA_TYPE_FLOAT_ARRAY: return 16;
        case DATA_TYPE_VEC2_ARRAY:  return 16;
        case DATA_TYPE_VEC3_ARRAY:  return 16;
        case DATA_TYPE_VEC4_ARRAY:  return 16;
        case DATA_TYPE_MAT4_ARRAY:  return 16;
        default: return 4;
    }
}

static isz std140_element_size(Data_Type type, isz count) {
    switch (type) {
        case DATA_TYPE_FLOAT:       return size_of(f32);
        case DATA_TYPE_VEC2:        return size_of(f32) * 2;

        case DATA_TYPE_VEC3:
        case DATA_TYPE_VEC4:        return size_of(f32) * 4;

        case DATA_TYPE_MAT4:        return size_of(f32) * 16;

        case DATA_TYPE_FLOAT_ARRAY: return 16 * count;
        case DATA_TYPE_VEC2_ARRAY:  return 16 * count;
        case DATA_TYPE_VEC3_ARRAY:  return 16 * count;
        case DATA_TYPE_VEC4_ARRAY:  return 16 * count;
        case DATA_TYPE_MAT4_ARRAY:  return 16 * 4 * count;
        default: return 0;
    }
}

void push_uniform(Uniform_Buffer* ub, Data_Type type, const void* data, isz count) {
    assert(ub && ub->cpu_mem);

    isz align = std140_base_alignment(type);
    ub->offset = (ub->offset + align - 1) & ~(align - 1); // std140 align

    switch (type) {
        case DATA_TYPE_VEC3: {
            const float* src = (const float*)data;
            float tmp[4] = { src[0], src[1], src[2], 0.0f };
            memcpy(ub->cpu_mem + ub->offset, tmp, sizeof(tmp));
            ub->offset += 16;
            return;
        }

        case DATA_TYPE_VEC3_ARRAY: {
            const float* src = (const float*)data;
            for (isz i = 0; i < count; ++i) {
                float tmp[4] = { src[i*3+0], src[i*3+1], src[i*3+2], 0.0f };
                memcpy(ub->cpu_mem + ub->offset, tmp, 16);
                ub->offset += 16;
            }
            return;
        }

        case DATA_TYPE_VEC2_ARRAY: {
            const float* src = (const float*)data;
            for (isz i = 0; i < count; ++i) {
                float tmp[4] = { src[i*2+0], src[i*2+1], 0, 0 };
                memcpy(ub->cpu_mem + ub->offset, tmp, 16);
                ub->offset += 16;
            }
            return;
        }

        case DATA_TYPE_FLOAT_ARRAY: {
            const float* src = (const float*)data;
            for (isz i = 0; i < count; ++i) {
                float tmp[4] = { src[i], 0, 0, 0 };
                memcpy(ub->cpu_mem + ub->offset, tmp, 16);
                ub->offset += 16;
            }
            return;
        }

        case DATA_TYPE_VEC4_ARRAY: {
            const float* src = (const float*)data;
            for (isz i = 0; i < count; ++i) {
                memcpy(ub->cpu_mem + ub->offset, &src[i*4], 16);
                ub->offset += 16;
            }
            return;
        }

        case DATA_TYPE_MAT4_ARRAY: {
            const float* src = (const float*)data;
            for (isz i = 0; i < count; ++i) {
                memcpy(ub->cpu_mem + ub->offset, &src[i*16], 64);
                ub->offset += 64;
            }
            return;
        }

        case DATA_TYPE_MAT4: {
            memcpy(ub->cpu_mem + ub->offset, data, sizeof(float) * 16);
            ub->offset += 64;
            return;
        }

        default: {
            isz size = std140_element_size(type, count);
            memcpy(ub->cpu_mem + ub->offset, data, size);
            ub->offset += size;
            return;
        }
    }
}


void push_uniform_float(Uniform_Buffer* ub, float value) {
    push_uniform(ub, DATA_TYPE_FLOAT, &value, 1);
}

void push_uniform_vec3(Uniform_Buffer* ub, Vector3 v) {
    push_uniform(ub, DATA_TYPE_VEC3, &v, 1);
}

void push_uniform_vec4(Uniform_Buffer* ub, Vector4 v) {
    push_uniform(ub, DATA_TYPE_VEC4, &v, 1);
}

void push_uniform_mat4(Uniform_Buffer* ub, Matrix m) {
    push_uniform(ub, DATA_TYPE_MAT4, &m, 1);
}

// Arrays
void push_uniform_vec3_array(Uniform_Buffer* ub, Vector3* arr, isz count) {
    push_uniform(ub, DATA_TYPE_VEC3_ARRAY, arr, count);
}

void push_uniform_vec4_array(Uniform_Buffer* ub, Vector4* arr, isz count) {
    push_uniform(ub, DATA_TYPE_VEC4_ARRAY, arr, count);
}

void push_uniform_mat4_array(Uniform_Buffer* ub, Matrix* arr, isz count) {
    push_uniform(ub, DATA_TYPE_MAT4_ARRAY, arr, count);
}





//--------------------------------------
// Shader Binding Usage
//--------------------------------------
// Uniform_Buffer: layout(std140, binding = N) uniform BlockName {}
// Texture_Buffer: uniform samplerBuffer texBuffer;
// Storage_Buffer: layout(std430, binding = N) buffer BlockName {}
// Image2D: layout(rgba32f, binding = N) uniform image2D myImage;
// Texture2D: uniform sampler2D tex;

//--------------------------------------
// Notes:
//--------------------------------------
// Image2D vs Regular Texture:
// - Image2D supports read-write operations from shaders (imageLoad/imageStore).
// - Regular Texture (sampler2D) is read-only and supports filtering and mipmaps.
// - You can bind the same GL_TEXTURE_2D to both image2D and sampler2D with different usage (e.g. bind to both for writing and sampling).

// Uniform vs Texture vs Storage_Buffer:
// - Uniform Buffer: Fast, small data, 16-byte alignment. Shared across programs. Limited size (e.g. 64KB).
// - Texture Buffer: 1D only, best for tightly packed uniform-like arrays. Read-only in shaders.
// - Storage_Buffer: Most flexible, larger storage, can read-write. Slower than Uniform_Buffers for small data.
// - Image2D: Arbitrary read/write, good for GPGPU or post-processing passes. Requires memory barriers.



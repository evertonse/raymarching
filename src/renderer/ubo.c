typedef struct {
    u8* buffer;          // CPU buffer
    usize capacity;      // Total bytes
    usize offset;        // Current write cursor
} UniformBufferWriter;

UniformBufferWriter make_uniform_writer(u8* mem, usize size) {
    return (UniformBufferWriter){ .buffer = mem, .capacity = size, .offset = 0 };
}

#define ALIGN_UP(v, a) (((v) + (a) - 1) & ~((a) - 1))

void ubo_write_padding(UniformBufferWriter* writer, usize alignment) {
    writer->offset = ALIGN_UP(writer->offset, alignment);
}

void ubo_push_float(UniformBufferWriter* writer, float f) {
    ubo_write_padding(writer, 4);
    memcpy(writer->buffer + writer->offset, &f, sizeof(float));
    writer->offset += sizeof(float);
}

void ubo_push_vec2(UniformBufferWriter* writer, Vector2 v) {
    ubo_write_padding(writer, 8);
    memcpy(writer->buffer + writer->offset, &v, sizeof(Vector2));
    writer->offset += sizeof(Vector2);
}

void ubo_push_vec4(UniformBufferWriter* writer, Vector4 v) {
    ubo_write_padding(writer, 16);
    memcpy(writer->buffer + writer->offset, &v, sizeof(Vector4));
    writer->offset += sizeof(Vector4);
}

void ubo_push_mat4(UniformBufferWriter* writer, Matrix mat) {
    ubo_write_padding(writer, 16);
    memcpy(writer->buffer + writer->offset, &mat, sizeof(Matrix)); // column-major
    writer->offset += sizeof(Matrix);
}


// MikkTSpace context helpers
typedef struct {
    Mesh *mesh;
} Mikk_User_Data;


// Returns number of faces (triangles always)
int mikk_get_num_faces(const SMikkTSpaceContext* ctx) {
    Mikk_User_Data* ud = (Mikk_User_Data*)ctx->m_pUserData;
    return ud->mesh->indices.count / 3;
}

int mikk_get_num_vertices_of_face(const SMikkTSpaceContext* ctx, const int face) {
    return 3; // always triangles
}

void mikk_get_position(const SMikkTSpaceContext* ctx, float outPos[3], const int face, const int vert) {
    Mikk_User_Data* ud = (Mikk_User_Data*)ctx->m_pUserData;
    Mesh* mesh = ud->mesh;

    u32 idx = mesh->indices.items[face * 3 + vert];
    Vector3 p = mesh->vertices.positions[idx];
    outPos[0] = p.x; outPos[1] = p.y; outPos[2] = p.z;
}

void mikk_get_normal(const SMikkTSpaceContext* ctx, float outNorm[3], const int face, const int vert) {
    Mikk_User_Data* ud = (Mikk_User_Data*)ctx->m_pUserData;
    Mesh* mesh = ud->mesh;

    u32 idx = mesh->indices.items[face * 3 + vert];
    Vector3 n = mesh->vertices.normals[idx];
    outNorm[0] = n.x; outNorm[1] = n.y; outNorm[2] = n.z;
}

void mikk_get_tex_coord(const SMikkTSpaceContext* ctx, float outUV[2], const int face, const int vert) {
    Mikk_User_Data* ud = (Mikk_User_Data*)ctx->m_pUserData;
    Mesh* mesh = ud->mesh;

    u32 idx = mesh->indices.items[face * 3 + vert];
    Vector2 uv = mesh->vertices.uvs[idx];
    outUV[0]   = uv.x; outUV[1] = uv.y;
}

// Store tangent + handedness
void mikk_set_tagent_space_basic(const SMikkTSpaceContext* ctx, const float tangent[3], const float sign, const int face, const int vert) {
    Mikk_User_Data* ud = (Mikk_User_Data*)ctx->m_pUserData;
    Mesh* mesh = ud->mesh;

    u32 idx = mesh->indices.items[face * 3 + vert];
    if (!mesh->vertices.tangents) {
        mesh->vertices.tangents = (Vector4*)calloc(mesh->vertices.count, size_of(Vector4));
    }
    mesh->vertices.tangents[idx] = (Vector4){ tangent[0], tangent[1], tangent[2], sign };
}


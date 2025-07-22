

#include "assets/all_obj.h"
typedef struct {
   Vector3 *vertices;
   Vector3 *normals;
   Vector2 *uvs;
   u32 *indices;

   u32 vertices_count;
   u32 uvs_count;
   u32 normals_count;
   u32 indices_count;
} Mesh;

// Macro to define a mesh from OBJ data
#define DEFINE_MESH(prefix, ext)                           \
   static Mesh prefix##_mesh = {                           \
      .vertices       = (Vector3 *)prefix##_objVerts,      \
      .normals        = (Vector3 *)prefix##_objNormals,    \
      .uvs            = (Vector2 *)prefix##_objTexCoords,  \
      .indices        = (u32 *)prefix##_objIndexes,        \
                                                           \
      .vertices_count = prefix##_objVertsCount,            \
      .uvs_count      = prefix##_objTexCoordsCount,        \
      .normals_count  = prefix##_objNormalsCount,          \
      .indices_count  = prefix##_objIndexesCount           \
   };                                                      \
   static char *prefix##_texture_path = "res/textures/" #prefix ext

DEFINE_MESH(bamboo, ".jpg");
DEFINE_MESH(enemy, ".png");
DEFINE_MESH(tiger, "_yellow.png");
DEFINE_MESH(horse, ".png");

static Mesh cube_mesh = {
   .vertices       = (Vector3 *)cube_objVerts,
   .normals        = (Vector3 *)cube_objNormals,
   .uvs            = (Vector2 *)cube_objTexCoords,
   .indices        = (u32 *)cube_objIndexes,

   .vertices_count = cube_objVertsCount,
   .uvs_count      = cube_objTexCoordsCount,
   .normals_count  = cube_objNormalsCount,
   .indices_count  = cube_objIndexesCount
};

Mesh generate_sphere_mesh_old(f32 radius, int rings, int slices) {
   Mesh mesh = {0};

   int vertex_count = (rings + 1) * (slices + 1);
   int index_count = rings * slices * 6;

   // Allocate one block for positions, normals, and uvs
   usz pos_size  = size_of(Vector3) * vertex_count;
   usz norm_size = size_of(Vector3) * vertex_count;
   usz uv_size   = size_of(Vector2) * vertex_count;

   void *vertex_data  = malloc(pos_size + norm_size + uv_size);
   Vector3 *positions = (Vector3 *)vertex_data;
   Vector3 *normals   = (Vector3 *)((u8 *)vertex_data + pos_size);
   Vector2 *uvs       = (Vector2 *)((u8 *)vertex_data + pos_size + norm_size);

   u32 *indices = malloc(size_of(u32) * index_count);

   int v = 0;
   for (int ring = 0; ring <= rings; ++ring) {
      float phi = (float)ring / rings * PI;
      float y = cosf(phi);
      float r = sinf(phi);

      for (int slice = 0; slice <= slices; ++slice) {
         float theta = (float)slice / slices * 2.0f * PI;
         float x = r * cosf(theta);
         float z = r * sinf(theta);

         Vector3 pos = {x * radius, y * radius, z * radius};
         Vector3 normal = {x, y, z};
         Vector2 uv = {(float)slice / slices, 1.0f - (float)ring / rings};

         positions[v] = pos;
         normals[v] = normal;
         uvs[v] = uv;
         v++;
      }
   }

   int i = 0;
   for (int ring = 0; ring < rings; ++ring) {
      for (int slice = 0; slice < slices; ++slice) {
         int a = (ring + 0) * (slices + 1) + slice;
         int b = (ring + 1) * (slices + 1) + slice;
         int c = a + 1;
         int d = b + 1;

         indices[i++] = a;
         indices[i++] = b;
         indices[i++] = c;

         indices[i++] = c;
         indices[i++] = b;
         indices[i++] = d;
      }
   }

   mesh.vertices       = positions;
   mesh.normals        = normals;
   mesh.uvs            = uvs;
   mesh.indices        = indices;

   mesh.vertices_count = vertex_count;
   mesh.normals_count  = vertex_count;
   mesh.uvs_count      = vertex_count;
   mesh.indices_count  = index_count;

   return mesh;
}

Mesh generate_sphere_mesh(float radius, int rings, int slices) {
   Mesh mesh = {0};

   int vertex_count = (rings + 1) * (slices + 1);
   int index_count = rings * slices * 6;

   size_t vertex_array_size = vertex_count * (size_of(Vector3) + size_of(Vector3) + size_of(Vector2));
   size_t index_array_size = index_count * size_of(unsigned int);

   void *memory = malloc(vertex_array_size + index_array_size);
   unsigned char *ptr = (unsigned char *)memory;

   mesh.vertices = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.normals = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.uvs = (Vector2 *)ptr;
   ptr += vertex_count * size_of(Vector2);
   mesh.indices = (unsigned int *)ptr;

   mesh.vertices_count = vertex_count;
   mesh.normals_count = vertex_count;
   mesh.uvs_count = vertex_count;
   mesh.indices_count = index_count;

   int v = 0;
   for (int i = 0; i <= rings; i++) {
      float phi = (float)i / rings * M_PI;
      for (int j = 0; j <= slices; j++) {
         float theta = (float)j / slices * 2.0f * M_PI;

         float x = sinf(phi) * cosf(theta);
         float y = cosf(phi);
         float z = sinf(phi) * sinf(theta);

         mesh.vertices[v] = (Vector3){radius * x, radius * y, radius * z};
         mesh.normals[v] = (Vector3){x, y, z};
         mesh.uvs[v] = (Vector2){(float)j / slices, (float)i / rings};
         v++;
      }
   }

   int k = 0;
   for (int i = 0; i < rings; i++) {
      for (int j = 0; j < slices; j++) {
         int i0 = i * (slices + 1) + j;
         int i1 = i0 + 1;
         int i2 = i0 + slices + 1;
         int i3 = i2 + 1;

         mesh.indices[k++] = i0;
         mesh.indices[k++] = i2;
         mesh.indices[k++] = i1;

         mesh.indices[k++] = i1;
         mesh.indices[k++] = i2;
         mesh.indices[k++] = i3;
      }
   }

   return mesh;
}

// Mesh
inline bool is_valid_mesh(Mesh mesh) { return mesh.vertices != NULL && mesh.indices != NULL && mesh.vertices_count > 0 && mesh.indices_count > 0; }

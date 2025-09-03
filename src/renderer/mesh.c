#include "assets/all_obj.h"
// void *data;
// bool is_from_single_data_buffer;
typedef struct {
   struct {
      Vector3 *positions;
      Vector3 *normals;

      union {
         Vector2 *uvs;
         Vector2 *texcoords;
      };

      struct {
         // Order is important
         Vector4Int indices;
         Vector4    weights;
         // should have one of each per position or none
      } *joints;
      u32 count;
   } vertices;

   struct {
      struct {
         isz indices_offset;
         isz indices_count;
         isz material_index;
      } *items;
      u32 count;
   } surfaces;

   struct {
      u32 *items;
      u32 count;
   } indices;
} Mesh;


// Macro to define a mesh from OBJ data
#define DEFINE_MESH(prefix, ext)                              \
   static Mesh prefix##_mesh = {                              \
      .vertices = {                                           \
         .positions      = (Vector3 *)prefix##_objVerts,      \
         .normals        = (Vector3 *)prefix##_objNormals,    \
         .uvs            = (Vector2 *)prefix##_objTexCoords,  \
         .count          = prefix##_objVertsCount             \
      },                                                      \
      .indices = {                                            \
         .items = (u32 *)prefix##_objIndexes,                 \
         .count  = prefix##_objIndexesCount                   \
      },                                                      \
   };                                                         \
   static char *prefix##_texture_path = "res/textures/" #prefix ext

DEFINE_MESH(bamboo, ".jpg");
DEFINE_MESH(enemy, ".png");
DEFINE_MESH(tiger, "_yellow.png");
DEFINE_MESH(horse, ".png");

static Mesh cube_mesh = {
   .vertices = {
      .positions       = (Vector3 *)cube_objVerts,
      .normals        = (Vector3 *)cube_objNormals,
      .uvs            = (Vector2 *)cube_objTexCoords,
      .count          = cube_objVertsCount
   },
   .indices = {
      .items = (u32 *)cube_objIndexes,
      .count =  cube_objIndexesCount
   }
};

Mesh generate_sphere_mesh(float radius, int rings, int slices) {
   Mesh mesh = {0};

   int vertex_count = (rings + 1) * (slices + 1);
   int index_count = rings * slices * 6;

   size_t vertex_array_size = vertex_count * (size_of(Vector3) + size_of(Vector3) + size_of(Vector2));
   size_t index_array_size = index_count * size_of(unsigned int);

   void *memory = malloc(vertex_array_size + index_array_size);
   unsigned char *ptr = (unsigned char *)memory;

   mesh.vertices.positions = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.vertices.normals = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.vertices.uvs = (Vector2 *)ptr;
   ptr += vertex_count * size_of(Vector2);
   mesh.indices.items = (unsigned int *)ptr;

   mesh.vertices.count = vertex_count;
   mesh.indices.count  = index_count;

   int v = 0;
   for (int i = 0; i <= rings; i += 1) {
      float phi = (float)i / rings * M_PI;
      for (int j = 0; j <= slices; j += 1) {
         float theta = (float)j / slices * 2.0f * M_PI;

         float x = sinf(phi) * cosf(theta);
         float y = cosf(phi);
         float z = sinf(phi) * sinf(theta);

         mesh.vertices.positions[v] = (Vector3){radius * x, radius * y, radius * z};
         mesh.vertices.normals[v]   = (Vector3){x, y, z};
         mesh.vertices.uvs[v]       = (Vector2){(float)j / slices, (float)i / rings};
         v += 1;
      }
   }

   int k = 0;
   for (int i = 0; i < rings; i += 1) {
      for (int j = 0; j < slices; j += 1) {
         int i0 = i * (slices + 1) + j;
         int i1 = i0 + 1;
         int i2 = i0 + slices + 1;
         int i3 = i2 + 1;

         mesh.indices.items[k++] = i0;
         mesh.indices.items[k++] = i2;
         mesh.indices.items[k++] = i1;

         mesh.indices.items[k++] = i1;
         mesh.indices.items[k++] = i2;
         mesh.indices.items[k++] = i3;
      }
   }

   return mesh;
}

Mesh create_mesh_from_interleaved(const float *interleaved, usz count) {
   Mesh mesh = {0};

   // 8 floats per vertex: 3 (pos) + 3 (normal) + 2 (uv)
   const usz floats_per_vertex = 8;
   const usz vertex_count = count / floats_per_vertex;

   usz vertex_data_size = vertex_count * size_of(Vector3);
   usz normal_data_size = vertex_count * size_of(Vector3);
   usz uv_data_size     = vertex_count * size_of(Vector2);
   usz index_data_size  = vertex_count * size_of(u32);

   usz total_size = vertex_data_size + normal_data_size + uv_data_size + index_data_size;
   void *block = malloc(total_size);

   // Assign pointers within the block
   mesh.vertices.positions = (Vector3 *)block;
   mesh.vertices.normals   = (Vector3 *)((char *)block + vertex_data_size);
   mesh.vertices.uvs       = (Vector2 *)((char *)block + vertex_data_size + normal_data_size);
   mesh.indices.items      = (u32 *)    ((char *)block + vertex_data_size + normal_data_size + uv_data_size);

   // At last, fill in the data
   for (usz i = 0; i < vertex_count; i += 1) {
       const float *v = &interleaved[i * floats_per_vertex];
       mesh.vertices.positions[i] = (Vector3){v[0], v[1], v[2]};
       mesh.vertices.normals  [i] = (Vector3){v[3], v[4], v[5]};
       mesh.vertices.uvs      [i] = (Vector2){v[6], v[7]};
       mesh.indices.items     [i] = (u32)i;
   }

   mesh.vertices.count = vertex_count;
   mesh.indices.count  = vertex_count;
   return mesh;
}


// Mesh
bool inline is_valid_mesh(Mesh mesh) {
   return nullptr != mesh.indices.items
      &&  nullptr != mesh.vertices.positions
      &&  nullptr != mesh.vertices.normals
      &&  nullptr != mesh.vertices.uvs
      &&  mesh.vertices.count > 0
      &&  mesh.indices.count > 0
   ;
}

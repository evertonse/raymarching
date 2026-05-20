#include "assets/all_obj.h"
#include "raymath.h"
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

      Vector4 *tangents; // One per vertex as well or null

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

void recalc_mesh_normals(Mesh *mesh) {
   if (!mesh || mesh->vertices.count == 0 || mesh->indices.count == 0) return;

   // zero out normals
   memset(mesh->vertices.normals, 0, mesh->vertices.count * sizeof(Vector3));

   for (u32 i = 0; i + 2 < mesh->indices.count; i += 3) {
      u32 i0 = mesh->indices.items[i+0];
      u32 i1 = mesh->indices.items[i+1];
      u32 i2 = mesh->indices.items[i+2];

      if (i0 >= mesh->vertices.count || i1 >= mesh->vertices.count || i2 >= mesh->vertices.count)
         continue; // safety

      Vector3 p0 = mesh->vertices.positions[i0];
      Vector3 p1 = mesh->vertices.positions[i1];
      Vector3 p2 = mesh->vertices.positions[i2];

      Vector3 e1 = sub(p1, p0);
      Vector3 e2 = sub(p2, p0);

      Vector3 fn = cross(e1, e2);

      // accumulate face normal to each vertex
      mesh->vertices.normals[i0].x += fn.x;
      mesh->vertices.normals[i0].y += fn.y;
      mesh->vertices.normals[i0].z += fn.z;

      mesh->vertices.normals[i1].x += fn.x;
      mesh->vertices.normals[i1].y += fn.y;
      mesh->vertices.normals[i1].z += fn.z;

      mesh->vertices.normals[i2].x += fn.x;
      mesh->vertices.normals[i2].y += fn.y;
      mesh->vertices.normals[i2].z += fn.z;
   }

   // normalize all normals
   for (u32 v = 0; v < mesh->vertices.count; v++) {
      mesh->vertices.normals[v] = Vector3Normalize(mesh->vertices.normals[v]);
   }
}

Mesh generate_sphere_mesh(float radius, int rings, int slices) {
   Mesh mesh = {0};

   int vertex_count = (rings + 1) * (slices + 1);
   int index_count = rings * slices * 6;

   size_t vertex_array_size = vertex_count * (size_of(Vector3) + size_of(Vector3) + size_of(Vector2));
   size_t index_array_size = index_count * size_of(uint);
   size_t surfaces_size = 1 * size_of(mesh.surfaces.items[0]);

   void *memory = malloc(vertex_array_size + index_array_size + surfaces_size);
   unsigned char *ptr = (unsigned char *)memory;

   mesh.vertices.positions = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.vertices.normals = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);
   mesh.vertices.uvs = (Vector2 *)ptr;
   ptr += vertex_count * size_of(Vector2);
   mesh.indices.items = (uint *)ptr;
   ptr += index_array_size;
   mesh.surfaces.items = (void*)ptr;

   mesh.vertices.count = vertex_count;
   mesh.indices.count  = index_count;
   mesh.surfaces.count = 1;

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

   struct {
      struct {
         isz indices_offset;
         isz indices_count;
         isz material_index;
      } *items;
      u32 count;
   } surfaces;

   mesh.surfaces.count = 1;
   mesh.surfaces.items[0].indices_offset = 0;
   mesh.surfaces.items[0].indices_count  = index_count;
   mesh.surfaces.items[0].material_index = -1;
   return mesh;
}

// From this:
// center = (cos(u), 0, sin(u)) * major_radius
// normal_circle = (cos(u), 0, sin(u))
// vertex = center + normal_circle * (cos(v) * minor_radius) + up * (sin(v) * minor_radius)
Mesh generate_torus_mesh(
   float major_radius, // distance from center
   float minor_radius, // radius of the tube
   int rings, int sides
) {
   Mesh mesh = {0};

   int vertex_count = (rings + 1) * (sides + 1);
   int index_count = rings * sides * 6;

   size_t vertex_array_size = vertex_count * (size_of(Vector3) + size_of(Vector3) + size_of(Vector2));

   size_t index_array_size = index_count * size_of(unsigned int);
   size_t surfaces_size = size_of(mesh.surfaces.items[0]);

   void *memory = malloc(vertex_array_size + index_array_size + surfaces_size);
   unsigned char *ptr = (unsigned char *)memory;

   mesh.vertices.positions = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);

   mesh.vertices.normals = (Vector3 *)ptr;
   ptr += vertex_count * size_of(Vector3);

   mesh.vertices.uvs = (Vector2 *)ptr;
   ptr += vertex_count * size_of(Vector2);

   mesh.indices.items = (unsigned int *)ptr;
   ptr += index_array_size;

   mesh.surfaces.items = (void *)ptr;

   mesh.vertices.count = vertex_count;
   mesh.indices.count = index_count;
   mesh.surfaces.count = 1;

   int vtx = 0;

   for (int i = 0; i <= rings; i++) {
      float u = (float)i / rings * 2.0f * M_PI;

      float cos_u = cosf(u);
      float sin_u = sinf(u);

      Vector3 circle_center = {cos_u * major_radius, 0.0f, sin_u * major_radius};

      Vector3 radial_dir = {cos_u, 0.0f, sin_u};

      for (int j = 0; j <= sides; j++) {
         float v = (float)j / sides * 2.0f * M_PI;

         float cos_v = cosf(v);
         float sin_v = sinf(v);

         Vector3 offset = {radial_dir.x * (cos_v * minor_radius), sin_v * minor_radius, radial_dir.z * (cos_v * minor_radius)};

         Vector3 pos = {circle_center.x + offset.x, circle_center.y + offset.y, circle_center.z + offset.z};

         mesh.vertices.positions[vtx] = pos;

         Vector3 normal = normalize(offset);
         mesh.vertices.normals[vtx] = normal;

         mesh.vertices.uvs[vtx] = (Vector2){(float)i / rings, (float)j / sides};

         vtx++;
      }
   }

   int k = 0;

   for (int i = 0; i < rings; i++) {
      for (int j = 0; j < sides; j++) {
         int i0 = i * (sides + 1) + j;
         int i1 = i0 + 1;
         int i2 = i0 + (sides + 1);
         int i3 = i2 + 1;

         mesh.indices.items[k++] = i0;
         mesh.indices.items[k++] = i2;
         mesh.indices.items[k++] = i1;

         mesh.indices.items[k++] = i1;
         mesh.indices.items[k++] = i2;
         mesh.indices.items[k++] = i3;
      }
   }

   mesh.surfaces.items[0].indices_offset = 0;
   mesh.surfaces.items[0].indices_count = index_count;
   mesh.surfaces.items[0].material_index = -1;

   return mesh;
}


Mesh generate_quad_mesh(float width, float height) {
   Mesh mesh = {0};

   int vertex_count = 4;
   int index_count  = 6;

   size_t positions_size = vertex_count * size_of(Vector3);
   size_t normals_size   = vertex_count * size_of(Vector3);
   size_t uvs_size       = vertex_count * size_of(Vector2);
   size_t indices_size   = index_count * size_of(u32);
   size_t surfaces_size  = 1 * size_of(mesh.surfaces.items[0]);

   void *memory = malloc(positions_size + normals_size + uvs_size + indices_size + surfaces_size);
   u8 *ptr = memory;

   mesh.vertices.positions =  (Vector3 *)ptr;
   ptr += positions_size;
   mesh.vertices.normals   =  (Vector3 *)ptr;
   ptr += normals_size;
   mesh.vertices.uvs       =  (Vector2 *)ptr;
   ptr += uvs_size;
   mesh.indices.items      =  (u32 *)ptr;
   ptr += indices_size;
   mesh.surfaces.items     =  (void *)ptr;
   mesh.vertices.count     =  vertex_count;
   mesh.indices.count      =  index_count;
   mesh.surfaces.count     =  1;

   float hw = width  * 0.5f;
   float hh = height * 0.5f;

   // Centered on origin, facing +Z
   mesh.vertices.positions[0] = (Vector3){-hw,-hh, 0};
   mesh.vertices.positions[1] = (Vector3){ hw,-hh, 0};
   mesh.vertices.positions[2] = (Vector3){ hw, hh, 0};
   mesh.vertices.positions[3] = (Vector3){-hw, hh, 0};

   for (int i = 0; i < 4; i++) {
      // We're looking into +Z so the what normal is facing us in -Z.
      mesh.vertices.normals[i] = (Vector3){0, 0, -1};
   }

   mesh.vertices.uvs[0] = (Vector2){0, 1};
   mesh.vertices.uvs[1] = (Vector2){1, 1};
   mesh.vertices.uvs[2] = (Vector2){1, 0};
   mesh.vertices.uvs[3] = (Vector2){0, 0};

   mesh.vertices.uvs[0] = (Vector2){0, 0};
   mesh.vertices.uvs[1] = (Vector2){1, 0};
   mesh.vertices.uvs[2] = (Vector2){1, 1};
   mesh.vertices.uvs[3] = (Vector2){0, 1};

   mesh.indices.items[0] = 0;
   mesh.indices.items[1] = 1;
   mesh.indices.items[2] = 2;
   mesh.indices.items[3] = 0;
   mesh.indices.items[4] = 2;
   mesh.indices.items[5] = 3;

   mesh.surfaces.items[0].indices_offset = 0;
   mesh.surfaces.items[0].indices_count  = index_count;
   mesh.surfaces.items[0].material_index = -1;

   return mesh;
}


Mesh create_mesh_from_interleaved(const float *interleaved, usz count) {
   Mesh mesh = {0};

   // 8 floats per vertex: 3 (pos) + 3 (normal) + 2 (uv)
   const usz floats_per_vertex = 8;
   const usz vertex_count = count / floats_per_vertex;

   usz vertex_data_size   = vertex_count * size_of(Vector3);
   usz normal_data_size   = vertex_count * size_of(Vector3);
   usz uv_data_size       = vertex_count * size_of(Vector2);
   usz index_data_size    = vertex_count * size_of(u32);
   usz surfaces_data_size = 1 * size_of(mesh.surfaces.items[0]);

   usz total_size = vertex_data_size + normal_data_size + uv_data_size + index_data_size + surfaces_data_size;
   void *block = malloc(total_size);

   // Assign pointers within the block
   mesh.vertices.positions = (Vector3 *)block;
   mesh.vertices.normals   = (Vector3 *)((char *)block + vertex_data_size);
   mesh.vertices.uvs       = (Vector2 *)((char *)block + vertex_data_size + normal_data_size);
   mesh.indices.items      = (u32 *)    ((char *)block + vertex_data_size + normal_data_size + uv_data_size);
   mesh.surfaces.items     = (void*)    ((char *)block + vertex_data_size + normal_data_size + uv_data_size + index_data_size);

   // At last, fill in the data
   for (usz i = 0; i < vertex_count; i += 1) {
      const float *v = &interleaved[i * floats_per_vertex];
      mesh.vertices.positions[i] = (Vector3){v[0], v[1], v[2]};
      mesh.vertices.normals  [i] = (Vector3){v[3], v[4], v[5]};
      mesh.vertices.uvs      [i] = (Vector2){v[6], v[7]};
      mesh.indices.items     [i] = (u32)i;
   }

   mesh.surfaces.items[0].material_index = -1;
   mesh.surfaces.items[0].indices_offset = 0;
   mesh.surfaces.items[0].indices_count  = vertex_count;


   mesh.vertices.count = vertex_count;
   mesh.indices.count  = vertex_count;
   mesh.surfaces.count = 1;
   return mesh;
}


bool inline is_valid_mesh(Mesh mesh) {
   return nullptr != mesh.indices.items
      &&  nullptr != mesh.vertices.positions
      &&  nullptr != mesh.vertices.normals
      &&  nullptr != mesh.vertices.uvs
      &&  mesh.vertices.count > 0
      &&  mesh.indices.count > 0
   ;
}

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#undef Command
#include "tinyobj_loader_c.h"

#undef swap
#undef local
#include "ufbx.h"
#include "ufbx.c"

typedef struct {
   struct {
      Mesh *items;
      usz count;
   } meshes;

   struct {
		struct {
			Texture diffuse, specular, emissive;
		} *items;
		usz count;
   } materials;

   struct {
      void *items; // TODO:
      usz count;
   } bones;

} Model_Textured;


# if 0
Model create_model(const char* filepath) {
	assert(path_ext(filepath) == ".obj");
	tinyobj_attrib_t attributes   = { 0 };
	tinyobj_shape_t* shapes       = nullptr;
	tinyobj_material_t* materials = nullptr;

	usz shapes_count    = 0;
	usz materials_count = 0;

	DString ds = { 0 };
	ds_read_file(filepath, &ds):
   ds_write_zero(ds);
	char* file_data  = ds.data;
	Model model = { 0 };
	if (file_data == nullptr) {
		trace_error("%s: [%s] Unable to read obj file", __func__, filepath);
		return model;
	}

	usz data_size = ds.size;
	usz flags = TINYOBJ_FLAG_TRIANGULATE;
	int ret = tinyobj_parse_obj(&attributes, &shapes, &shapes_count, &materials, &materials_count, file_data, data_size, flags);

	if (ret != TINYOBJ_SUCCESS) {
		trace_error("MODEL: Unable to read obj data %s", fileName);
		return model;
	}
	ds_free(ds);

	// TODO: Read from tinyobj into model
   // 

	return model;
}
#endif



typedef struct {
    struct {
        Mesh* items;
        isz count;
    } meshes;

    struct {
        struct {
            const char* diffuse;
            const char* specular;
            const char* emissive;
        } *items;
        isz count;
    } materials;

    struct {
        void* items;
        isz count;
    } bones;
} Model;

static char *mmap_file(const char *filename, usz *len) {
#if defined(PLATFORM_WINDOWS) || !defined(PLATFORM_MINGW)
   HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);

   if (file == INVALID_HANDLE_VALUE) { /* E.g. Model may not have materials. */
      return NULL;
   }

   HANDLE fileMapping = CreateFileMapping(file, NULL, PAGE_READONLY, 0, 0, NULL);
   assert(fileMapping != INVALID_HANDLE_VALUE);

   LPVOID fileMapView = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
   char *fileMapViewChar = (char *)fileMapView;
   assert(fileMapView != NULL);

   DWORD file_size = GetFileSize(file, NULL);
   (*len) = (size_t)file_size;

   return fileMapViewChar;
#else
   struct stat sb;
   char *p;
   int fd;

   fd = open(filename, O_RDONLY);
   if (fd == -1) {
      perror("open");
      return NULL;
   }

   if (fstat(fd, &sb) == -1) {
      perror("fstat");
      return NULL;
   }

   if (!S_ISREG(sb.st_mode)) {
      fprintf(stderr, "%s is not a file\n", filename);
      return NULL;
   }

   p = (char *)mmap(0, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);

   if (p == MAP_FAILED) {
      perror("mmap");
      return NULL;
   }

   if (close(fd) == -1) {
      perror("close");
      return NULL;
   }

   (*len) = sb.st_size;

   return p;

#endif
}

static DString tinyobj_ds = { 0 };
static void tinyobj_file_reader_callback(void *ctx, const char *filename, int is_mtl, const char *obj_filename, char **buf, size_t *len) {
   if (!filename) {
      trace_error("%s null filename\n", __func__);
      (*buf) = NULL;
      (*len) = 0;
      return;
   }
   ZString model_directory = ctx;
   TString filepath = tprintf("%s%c%s", model_directory, PATH_SEPARATOR_CHAR, filename);
   int option = 3;

   if (option == 1) {
      usz data_size = 0;
      trace_info("Trying to info");
      *buf = mmap_file(filename, &data_size);
      (*len) = data_size;
   } else if (option == 2) {
      char *items = tinyobj_ds.items;
      ds_read_file(filepath, &tinyobj_ds);
      *buf = items;
      *len = tinyobj_ds.items - items;
   } else if (option == 3) {
      *buf = read_file(filepath);
      *len = strlen(*buf);
      trace_info("Buf(%p) len=%lld for filename `%s`", *buf, (isz)(*len), filename);
   } else {
      assert(0 && "fuck you");
   }
   if (*buf == nullptr) {
      trace_info("Buf is null for filename `%s`", filename);
      *len = 0;
   }
}


Model create_model_works_badly(const char *filepath) {
   Model model = {0};

   DString ds = {0};
   ds_read_file(filepath, &ds);
   if (!ds.data) {
      trace_error("Failed to read OBJ file: %s", filepath);
      return model;
   }


   tinyobj_attrib_t attrib = {0};
   tinyobj_shape_t *shapes = NULL;
   tinyobj_material_t *materials = NULL;
   usz shape_count = 0, material_count = 0;
   int flags = TINYOBJ_FLAG_TRIANGULATE;
   // int flags = 0;

   int result = 0;
   usz check_point = tsave();
   {
      trace_info("About to parse");
      ZString model_directory = path_parent(filepath);
      ZString filename = path_base_name(filepath);
      result = tinyobj_parse_obj(
         &attrib,
         &shapes,  &shape_count,
         &materials, &material_count,
         filename,   // filename for internal file loading
         tinyobj_file_reader_callback, (void*)model_directory,
         flags
      );
      trace_info("Just parsed");
   }
   trestore(check_point);


   if (result != TINYOBJ_SUCCESS) {
      trace_error("Failed to parse OBJ: %s", filepath);
      return model;
   }

   trace_info("Parsed successfully");

   model.meshes.count = shape_count;
   model.meshes.items = calloc(shape_count, size_of(Mesh));

   for (usz s = 0; s < shape_count; ++s) {
      const tinyobj_shape_t *shape = &shapes[s];
      usz index_start = shape->face_offset;
      usz index_count = shape->length;

      Mesh *mesh = &model.meshes.items[s];
      mesh->positions = malloc(size_of(Vector3) * index_count);
      mesh->normals   = malloc(size_of(Vector3) * index_count);
      mesh->texcoords = malloc(size_of(Vector2) * index_count);
      mesh->indices   = malloc(size_of(u32) * index_count);
      mesh->indices_count = index_count;
      mesh->vertices_count = index_count;

      for (usz i = 0; i < index_count; ++i) {
         usz face_idx = index_start + i;
         tinyobj_vertex_index_t idx = attrib.faces[face_idx];
         mesh->indices[i] = i;

         if (idx.v_idx >= 0) {
            float *v = &attrib.vertices[3 * idx.v_idx];
            mesh->positions[i] = (Vector3){v[0], v[1], v[2]};
         }

         if (idx.vn_idx >= 0) {
            float *n = &attrib.normals[3 * idx.vn_idx];
            mesh->normals[i] = (Vector3){n[0], n[1], n[2]};
         }

         if (idx.vt_idx >= 0) {
            float *t = &attrib.texcoords[2 * idx.vt_idx];
            mesh->texcoords[i] = (Vector2){t[0], t[1]};
         }
      }

      mesh->material_index = (attrib.material_ids && index_start < attrib.num_faces) ? attrib.material_ids[index_start] : -1;
   }

   model.materials.count = material_count;
   model.materials.items = calloc(material_count, size_of(*model.materials.items));

   for (usz i = 0; i < material_count; ++i) {
      tinyobj_material_t *mat = &materials[i];
      model.materials.items[i].diffuse  = strdup(mat->diffuse_texname ? mat->diffuse_texname : "");
      model.materials.items[i].specular = strdup(mat->specular_texname ? mat->specular_texname : "");
      // model.materials.items[i].emissive = strdup(mat->emission ? mat->emission : "");
   }

   tinyobj_shapes_free(shapes, shape_count);
   tinyobj_materials_free(materials, material_count);
   tinyobj_attrib_free(&attrib);

   // free(shapes);
   // free(materials);
   return model;
}

Model create_model(const char *filepath) {
   ufbx_load_opts opts = {
       .target_axes = ufbx_axes_right_handed_y_up,
       // .target_unit_meters = 1.0f,
       .geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES,
   };
   ufbx_error error; // Optional, pass NULL if you don't care about errors
   // ufbx_scene *scene = ufbx_load_file("res/models/survival-guitar-backpack/source/Survival_BackPack_2.fbx", &opts, &error);
   ufbx_scene *scene = ufbx_load_file("res/models/backpack/backpack.obj", &opts, &error);
   trace_struct(*scene);

   assert(scene->meshes.count > 0);
   assert_msg(scene->meshes.count == scene->nodes.count - 1, "%lld meshes, %lld nodes", scene->meshes.count, scene->nodes.count);

   Model model = {0};

   // model.meshes.count = scene->meshes.count;
   model.meshes.count = 0;
   model.meshes.items = malloc(scene->meshes.count * size_of(model.meshes.items[0]));

   for (usz node_idx = 0; node_idx < scene->nodes.count; node_idx++) {
      ufbx_node *node = scene->nodes.data[node_idx];
      if (nullptr == node->mesh) {
         continue;
      }

      if (node->is_root) {
         printf("Node %s is root\n", node->name.data);
      }
      printf("%s\n", node->name.data);
      printf("-> mesh with %zu faces\n", node->mesh->faces.count);

      // trace_struct(*node->mesh);

      auto mesh = node->mesh;
      auto materials = node->materials;

      isz vertices_count = 0;
      usz triangles_count = mesh->num_triangles;
      isz tri_indices_count = mesh->max_face_triangles * 3;
      usz checkpoint = tsave();
      u32 *tri_indices = talloc(tri_indices_count * size_of(u32));

      usz indices_count = triangles_count * 3;

      // Malloc once and set the pointers
      Mesh model_mesh = {0};
      {
         usz positions_size = triangles_count * 3 * size_of(model_mesh.positions[0]);
         usz normals_size   = triangles_count * 3 * size_of(model_mesh.normals[0]);
         usz uvs_size       = triangles_count * 3 * size_of(model_mesh.uvs[0]);
         isz indices_size   = indices_count   * size_of(u32);
         // Final indices will occupy less memory that we're setting here
         char* data = malloc(positions_size + normals_size + uvs_size + indices_size);

         model_mesh.positions = (Vector3*)(data + 0);
         model_mesh.normals   = (Vector3*)(data + positions_size);
         model_mesh.uvs       = (Vector2*)(data + positions_size + normals_size);
         model_mesh.indices   = (u32*    )(data + positions_size + normals_size + uvs_size);

         model_mesh.positions_count = triangles_count * 3;
         model_mesh.uvs_count       = triangles_count * 3;
         model_mesh.normals_count   = triangles_count * 3;
      }

      for (usz face_idx = 0; face_idx < mesh->faces.count; face_idx++) {
         ufbx_face face = mesh->faces.data[face_idx];
         u32 tri_count = ufbx_triangulate_face(tri_indices, tri_indices_count, mesh, face);
         // Iterate over each triangle corner contiguously.
         for (isz tri_idx = 0; tri_idx < tri_count * 3; tri_idx++) {
            u32 index = tri_indices[tri_idx];
            ufbx_vec3 ufbx_position = ufbx_get_vertex_vec3(&mesh->vertex_position, index);
            ufbx_vec3 ufbx_normal   = ufbx_get_vertex_vec3(&mesh->vertex_normal, index);
            ufbx_vec2 ufbx_uv       = ufbx_get_vertex_vec2(&mesh->vertex_uv, index);

            Vector3 position = {(f32)ufbx_position.x, (f32)ufbx_position.y, (f32)ufbx_position.z};
            Vector3 normal   = {(f32)ufbx_normal.x,   (f32)ufbx_normal.y,   (f32)ufbx_normal.z};
            Vector2 uv       = {(f32)ufbx_uv.x,       (f32)ufbx_uv.y};
            model_mesh.positions[vertices_count] = position;
            model_mesh.normals[vertices_count]   = normal;
            model_mesh.uvs[vertices_count]       = uv;
            model_mesh.indices[vertices_count]   = vertices_count;
            vertices_count += 1;
         }
      }

      trestore(checkpoint);

      assert((isz)vertices_count == (isz)triangles_count * 3
            && model_mesh.positions_count == vertices_count
            && model_mesh.normals_count   == vertices_count
            && model_mesh.uvs_count       == vertices_count
      );


      if (true) {
         // Generate the index buffer.
         ufbx_vertex_stream streams[] = {
             {model_mesh.positions, vertices_count, size_of(model_mesh.positions[0])},
             {model_mesh.normals,   vertices_count, size_of(model_mesh.normals[0])},
             {model_mesh.uvs,       vertices_count, size_of(model_mesh.uvs[0])},
         };

         // This call will deduplicate vertices, modifying the arrays passed in `streams[]`,
         // indices are written in `indices[]` and the number of unique vertices is returned.
         isz vertices_count_new = (isz)ufbx_generate_indices(streams, count_of(streams), model_mesh.indices, indices_count, NULL, NULL);
         model_mesh.positions_count = vertices_count_new;
         model_mesh.normals_count   = vertices_count_new;
         model_mesh.uvs_count       = vertices_count_new;

         // model_mesh.indices_count   = vertices_count_new;
         model_mesh.indices_count = indices_count;


         if (vertices_count_new < vertices_count) {
            trace_okay("ufbx_generate_indices optimized from %lld to %lld", vertices_count, vertices_count_new);
         } else if (vertices_count_new == vertices_count) {
            trace_info("ufbx_generate_indices did jack shit from %lld to %lld", vertices_count, vertices_count_new);
         } else {
            trace_error("ufbx_generate_indices did worsened (? ?) from %lld to %lld", vertices_count, vertices_count_new);
         }

      } else {
         model_mesh.indices_count = indices_count;
      }
      model.meshes.items[model.meshes.count++] = model_mesh;

   }

   assert(scene->meshes.count == (usz)model.meshes.count);
   ufbx_free_scene(scene);
   return model;
}


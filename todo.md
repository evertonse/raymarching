
# Raymarch Compute Pipeline
- [ ] Shader from single buffer by having #pragma Vertex in it
- [x] Render triangles with raymarching xD
- [ ] Render triangles from a model
- [x] Derive cube sdf
- [ ] Antialiasing
- [ ] All common operations plus smoothing
- [ ] Mirroring
- [ ] Focus and defocus
- [ ] Twisting
- [ ] Domain repetition unbounded
- [ ] Domain repetition bounded
- [ ] Actually understand soft shadows
- [x] Understand camera as origin and ray to canvas
- [ ] Understand camera as origin rectangle to canvas
- [ ] Understand ray direction distortion
- [ ] Understand camera distortion
- [ ] Understand camera as a basis vector in the context of raymarching
- [ ] Understand scaling and rotation of distance fields
- [ ] Call back error from opengl howw to setup

# Projection Pipeline
- [x] Renderbuffer, SSBO, Uniform buffer objects create and use

- [x] Vertex Pulling positions
- [ ] Vertex Pulling with normal and uvs

- [ ] Accomulated commands and then do one draw_multi_indexed_instanced_indirect, you might be reading a lot of meshes and accumulating, then you much all those vertices into from all meshes accumulated into a single Vertex Buffer and the same for other buffers always remembering and associating the mesh id to int offset into all those buffers to then do pulling from the shader side to decide the correct values (positions, uvs, texture units samples form texure arrays) for the current instance mesh.
- [ ] Use Persistent Buffer for uniforms
- [ ] Lear about synchronization

- [x] All DSA

- [ ] Light Shader showing a ball as the light position
- [ ] Make Shader reloadable even if its from memory (using its handle to create a file to mark time of creation)
- [ ] Instancing with uniform and without it
- [ ] Texture sampling from ssbo is possible?
- [ ] DrawIndirect
- [ ] Infinite drawing no distant shit being culled
- [ ] AZDO
- [ ] Bindless textures
- [ ] MSAA manually

- [ ] Material struct as uniform buffer
- [ ] Normal inverse transforming
- [ ] uniform buffers for most thing, model matrices shall be send Storage Buffer

- [x] (nuklear has been chosen) Now setup gui.c. RayGui with no raylib or some other immediate mode

- [x] Map any texture for usage as image2D

- [x] init, update, input architecture

- [ ] Load Model fbx and stuff
- [ ] Cubemap Skybox support

- [x] MSAA with multisampled textures
- [x] MSAA framebuffer resolving
- [x] MSAA framebuffer blitting checks
- [ ] Copy texture into another taking alpha into account
- [x] Update Vertex_Array to use the Buffer api

- [x] Fix Shader reaload lag
- [ ] Mix what you wrote in raymath.c with handmade_math to get the best of both (has to fix naming conv)

- [ ] ``upload_matrix4(string, data_ptr);`` - Shader should cache by using std_ds string hash map;
- [ ] Bindings of types of buffers should be cached
- [ ] Blend, Cull and other setting should be cached makiing sure nobody changes that that mean making sure no dependencies touch the context


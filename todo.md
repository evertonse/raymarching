
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
- [x] Vertex Pulling
- [ ] Vertex Pulling with normal and uvs
- [x] All DSA
- [ ] Instancing with uniform and without it
- [ ] Texture sampling from ssbo is possible?
- [ ] DrawIndirect
- [ ] Infinite drawing no distant shit being culled
- [ ] AZDO
- [ ] Bindless textures
- [ ] MSAA manually

- [ ] Material struct as uniform buffer
- [ ] Normal inverse transforming
- [ ] RayGui with no raylib or some other immediate mode

- [ ] Map any texture for usage as image2D

- [ ] init, update, input architecture

- [ ] Load Model fbx and stuff
- [ ] Cubemap Skybox support

- [x] MSAA with multisampled textures
- [x] MSAA framebuffer resolving
- [x] MSAA framebuffer blitting checks
- [ ] Copy texture into another taking alhpa into account
- [x] Update Vertex_Array to use the Buffer api

- [ ] ``upload_matrix4(string, data_ptr);`` - Shader should cache by using std_ds string hash map;


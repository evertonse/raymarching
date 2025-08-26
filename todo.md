
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
- [ ] Instead of assing has_animation, has_diffuse, pass a clamping value or intensity value that inquires a math operation instead of an if
- [ ] Abstract every renderable into cpu data and gpu data counterpart.
- [ ] Refactor Buffer to just have buffer with additional specific data, rather than multiple types.

- [ ] Investigate animation problem

- [ ] Make shared binary compatible types that gets included by both

- [ ] Shader remap error to correct line in folder

- [x] Renderbuffer, SSBO, Uniform buffer objects create and use

- [x] Vertex Pulling positions
- [ ] Vertex Pulling with normal and uvs

- [ ] Accumulated commands and then do one draw_multi_indexed_instanced_indirect, you might be reading a lot of meshes and accumulating, then you much all those vertices into from all meshes accumulated into a single Vertex Buffer and the same for other buffers always remembering and associating the mesh id to int offset into all those buffers to then do pulling from the shader side to decide the correct values (positions, uvs, texture units samples form texure arrays) for the current instance mesh.
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

- [ ] Big Buffers for all rendering data by a manager abstraction.
- [ ] Manager bind vb and ib, you forgot

- [x] Model Skeleton:  simple model
- [x] Model Animation: simple model
- [ ] Model Animation: various models with varying complexity
- [ ] Model Animation: Simplify updating animation
- [ ] Model GpuData: What is the best way to setup the gpu data for a model and pass a lighter structure around? Figure it out.
      For now  we're using some anonymous structure as a "bundle".
- [ ] Model Animation support multiply animation.
- [ ] Model Animation suport blending animation into another one smoothly.
- [ ] Model Animation suport multiple meshes with different geomtry_to_node matrices (inverse bind matrices).
- [ ] Model Animation Transistion from walk to idle
- [ ] Camera make the camera follow the carachter prolly by having a certing offsset from a certain view direction (the character's)

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
- [ ] Expand Shader to allow "#pragma once"
- [ ] Mix what you wrote in raymath.c with handmade_math to get the best of both (has to fix naming conv)

- [ ] ``upload_matrix4(string, data_ptr);`` - Shader should cache by using std_ds string hash map;
- [ ] Bindings of types of buffers should be cached
- [ ] Blend, Cull and other setting should be cached makiing sure nobody changes that that mean making sure no dependencies touch the context

- [ ] Sparse (Virtual) Texture Arrays
- [ ] SFX in games. How can it be done ?

## Model Loading
- [x] Retrive .mtl file from .obj when it tried to load it
- [x] Tender .obj with .mtl files, diffuse and specular
- [ ] Try .glb file with it
- [ ] Model loading with cgtlf
- [x] Sword.fbx with wrong absolute path, deal with it by taking the base_name and search relatively to the .fbx file passed



download zip,
find an online 3d viewer that properly renders the backface shade thing,
use that to convert the set of files to a .glb file,
open that in the default Windows 3d viewer because it does not support adding textures to .obj files (paint 3d does but it does not support .mtl),
apply default spin animation,
record the entire 3d viewer window with windows game bar,
find a random online editor to crop and cut,
export as gif (did not even have to convert mp4 to gif or anything, kewl)

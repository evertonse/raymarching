Simple program to explore raymarching using compute shaders in OpenGL. It offers a ShaderToy like interface for the shaders.
Plus now I'm working on abstractions for rasterization pipeline.

## Building

Currently I'm cross compiling from linux using mingw or clang and running on w11. Debugging using raddebugger, only cland with lld can output .pbd files from linux that I know of.

Look at build.sh to uncomment building from linux to linux. Need C23 capable compiler (using nullptr, typeof, constexpr).

```
./build.sh
```

# Resources

- https://github.com/electricsquare/raymarching-workshop?tab=readme-ov-file#camera
- [An introduction to Raymarching](https://www.youtube.com/watch?v=khblXafu7iA)
- [OpenGL only knows about NDC and it's left-handed](https://www.gingerbill.org/article/2024/11/10/opengl-is-not-right-handed/)
- [Deriving the perspective projection](https://youtu.be/k_L6edKHKfA?si=wgMU7ZRCrAcDgkFW)
- [Blender use SketchLab to export textures correctly](https://www.youtube.com/watch?v=P_Gxefjggu0)
- [Useful not as common single header libraries](https://github.com/r-lyeh/single_file_libs)
- [Normal map from albedo](https://github.com/Sir-Irk/si_normalmap)



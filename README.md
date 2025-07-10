Simple program to explore raymarching using compute shaders in OpenGL. It offers a ShaderToy like interface for the shaders

## Building

Currently I'm cross compiling from linux using mingw and running on w11.
Look at build.sh to uncomment building from linux to linux. Need C23 capable compiler (using nullptr, typeof, constexpr).

```
./build.sh
```

# Resources

- https://www.youtube.com/watch?v=khblXafu7iA
- https://github.com/electricsquare/raymarching-workshop?tab=readme-ov-file#camera
- [OpenGL only knows about NDC and it's left-handed](https://www.gingerbill.org/article/2024/11/10/opengl-is-not-right-handed/)
- [Deriving the perspective projection](https://youtu.be/k_L6edKHKfA?si=wgMU7ZRCrAcDgkFW)


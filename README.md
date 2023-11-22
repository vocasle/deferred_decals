# Deferred Decals in OpenGL

This is a very naive implementation of deferred decals. Very straightforward and absolutely unoptimized.
Here is an example of the deferred decal.

![Example](res/misc/example.png)
---
### How to build

Execute following CMake command to build the project:
```CMake
cmake -S . -B build
cmake --build build
```
AFAIK there is no external dependencies. All of the dependencies are stored inside `thirdparty` directory and are statically linked to the executable.
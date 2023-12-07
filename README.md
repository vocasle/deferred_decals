# Deferred Decals in OpenGL

This is a very naive implementation of deferred decals. Very straightforward and absolutely unoptimized.
Here is an example of decals.

![Example](res/misc/example.png)
---
### How to build

Execute following CMake command to build the project:
```CMake
cmake -S . -B build
cmake --build build
```
AFAIK there is no external dependencies. All of the dependencies are stored inside `thirdparty`
directory and are statically linked to the executable.

### How current implementation works
We use deferred rendering to implement decals. The key figure in deferred rendering is GBuffer.
Our GBuffer contains several buffers:
1. World Position
1. World Normal (normal that was sampled from normal map and reconstructed using TBN matrix)
1. Albedo
1. World Tangent
1. World Bitanget
1. World Vertex Normal

Whole render loop consists of following logical render passes:
1. Geometry Pass
1. Deferred Decals
1. Deferred Shading Pass
1. Copy GBuffer Depth Pass
1. Wireframe Pass

#### Geometry Pass
In this pass we draw all the geometry in the scene and fill GBuffer. We use custom framebuffer with N
color attachments and one depth stencil attachment.

#### Deferred Decals
In this pass we draw decals. Decals can write in any buffer of GBuffer. In our implementation this
pass writes only to Albedo and Normal buffers.

#### Deferred Shading Pass
In this pass we evaluate shading equation for all lights in the scene per pixel.

#### Copy GBuffer Depth Pass
In this pass we copy GBuffer's depth stencil texture to backbuffer's depth stencil texture.
This is done to draw something with forward shading.

#### Wireframe Pass
This is an auxiliary pass that draws bounding box of the decal volume.

### Deferred Decal Algorithm
To draw decal we need to draw a parallelepiped. One of the normals of the faces will be used as
direction of decal projection. We use -Y axis as direction of projection.

It is very important to orient parallelepiped in such a way so that -Y will face towards the mesh
that you want to be covered with decal. See screenshot below. Red arrows are -Y in local space of
the parallelepiped.
![Projection Direction](res/misc/projection_direction.png)

High level algorithm looks as follows:
1. We draw bboxes
1. In vertex shader we pass `gl_Position` to fragment shader and transform vertices
using casual MVP matrix.
1. In fragment shader we get NDC coordinates `screenPos` by dividing `ClipPos.xy` by `ClipPos.w`.
1. Then we convert those NDC coorditanes to `uv` coordinates by scaling so that all values lie in [0, 1],
i.e. multiply by 0.5 and add 0.5
1. We obtain depth (the Z coordinate) from `g_depth` texture using those `uv` coordinates.
1. Then we reconstruct world position `worldPos` by calling `WorldPosFromDepth` function.
1. After that we transform `worldPos` to local space of bounding box (`localPos`).
1. Then we transform `localPos.xz` from [-1,1] interval to [0,1] interval to use at as
`decalUV` UV coordinates.
1. After that we discard all fragments that lie outside of bounding box
1. For each fragment that lies inside bounding box we retrieve albedo and normal.
1. Then we write albedo and normal to GBuffer

---

Because we already have something in GBuffer's depth buffer we have to set a proper render
state to be able to draw parallelepipeds. We set depth access read only and change depth function
to `GL_GREATER`.

We copy GBuffer's depth to a texture to feed it to fragment shader in Deferred Decal Pass.

As of uniforms the really special are:
- `g_decalInvWorld` that contains inverse matrix that transforms a point to local space of
parallelipiped
- `g_depth` that contains copy of GBuffer's depth
- `g_tbn_tangent` that contains World Tangent
- `g_tbn_bitangent` that contains World Bitanget
- `g_tbn_normal` that contains World Vertex Normal
- `g_rtSize` that contains in x and y components width and height of framebuffer and inverse
of width and height of framebuffer in z and w components


Vertex shader for Deferred Decal Pass is nothing special. This is the same shader that was used for
Geometry Pass. In our implementation this is `vert.glsl`.

The main work happens in fragment shader `deferred_decal.glsl`.

Let's break shader output parameters:
```GLSL
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;
```

`gNormal` is GBuffer's Normal buffer. It will be used to alter normals in area where we will be drawing
decals.

`gAlbedoSpec` is GBuffer's Albedo buffer. We write color of the decals to this attachment.

Important uniforms were explained before and listed here for complitenes.
```GLSL
uniform mat4 g_invViewProj;
uniform mat4 g_decalInvWorld;
uniform vec4 g_rtSize;
uniform vec3 g_bboxMin;
uniform vec3 g_bboxMax;

uniform sampler2D g_depth;
uniform sampler2D g_albedo;
uniform sampler2D g_normal;
uniform sampler2D g_tbn_tangent;
uniform sampler2D g_tbn_bitangent;
uniform sampler2D g_tbn_normal;
```

Of all the shader input parameters only `ClipPos` probably needs some explanation. This is just an
interpolated `gl_Position`. 
```GLSL
in vec3 WorldPos;
in vec2 TexCoords;
in vec4 ClipPos;
```
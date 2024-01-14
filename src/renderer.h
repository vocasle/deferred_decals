#pragma once

#include "defines.h"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "mymath.h"
#include "myutils.h"

struct Vertex {
	Vec3D position;
	Vec3D normal;
	Vec2D texCoords;
	Vec4D tangent;
};

struct MeshProxy {
	u32 vao;
	u32 vbo;
	u32 ebo;
	u32 numIndices;
	Mat4X4 world;
	struct Texture2D *albedo;
	struct Texture2D *normal;
	struct Texture2D *specular;
};

struct ModelProxy {
	struct MeshProxy *meshes;
	u32 numMeshes;
};

struct ModelProxy *ModelProxy_Create(const i8 *path);

enum UniformType {
	UT_MAT4,
	UT_VEC4F,
	UT_VEC3F,
	UT_VEC2F,
	UT_FLOAT,
	UT_INT,
	UT_UINT,
};

enum ObjectIdentifier {
	OI_BUFFER,
	OI_INDEX_BUFFER,
	OI_VERTEX_BUFFER,
	OI_SHADER,
	OI_VERTEX_SHADER,
	OI_FRAGMENT_SHADER,
	OI_PROGRAM,
	OI_VERTEX_ARRAY,
	OI_QUERY,
	OI_PROGRAM_PIPELINE,
	OI_TRANSFORM_FEEDBACK,
	OI_SAMPLER,
	OI_TEXTURE,
	OI_RENDERBUFFER,
	OI_FRAMEBUFFER,
};

/// Texture2D
struct Texture2DCreateInfo
{
	i32 width;
	i32 height;
	i32 internalFormat;
	i32 format;
	i32 type;
	const i8 *name;
	boolean genFB;
	i32 framebufferAttachment;
};

struct Texture2D {
	i32 width;
	i32 height;
	i8 *name;
	u32 handle;
	i32 samplerLocation;
};

struct Texture2D *Texture2D_Create(const struct Texture2DCreateInfo *info);
void Texture2D_Init(struct Texture2D *t, const struct Texture2DCreateInfo *info);
void Texture2D_Load(struct Texture2D *t, const i8 *texPath, i32 internalFormat,
		i32 format, i32 type);
void Texture2D_Destroy(struct Texture2D *t);

/// Material
#define MAX_SAMPLERS 16
struct MaterialCreateInfo
{
	const i8 *vsPath;
	const i8 *fsPath;
    const i8 *name;
    const i8 *samplers[MAX_SAMPLERS];
    u32 numSamplers;
};

struct Material;

struct Material *Material_Create(const struct MaterialCreateInfo *info);
void Material_Destroy(struct Material *m);
void Material_SetUniform(struct Material *m, const i8 *name, u32 size, 
    const void *data, enum UniformType type);
u32 Material_GetHandle(const struct Material *m);
const i8 *Material_GetName(const struct Material *m);
void Material_SetTexture(struct Material *m, const i8 *name,
    const struct Texture2D *t);

// TODO Make private

void DebugBreak(void);
#define GLCHECK(x) x; \
do { \
	GLenum err; \
	while((err = glGetError()) != GL_NO_ERROR) \
	{ \
		UtilsDebugPrint("ERROR in call to OpenGL at %s:%d", __FILE__, __LINE__); \
		DebugBreak(); \
		exit(-1); \
	} \
} while (0)



void SetObjectName(enum ObjectIdentifier objectIdentifier, u32 name, const i8 *label);

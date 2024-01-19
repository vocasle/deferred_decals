#include "renderer.h"
#include "objloader.h"

#include <stdlib.h>
#include <string.h>

#include <mikktspace.h>
#include <stb_image.h>

struct File {
	i8 *contents;
	uint64_t size;
};

struct Material
{
	u32 programHandle;
    const i8 *name;
    struct MaterialCreateInfo createInfo;
};

void DebugBreak(void)
{
#if _WIN32
	__debugbreak();
#else
		raise(SIGTRAP);
#endif
}



static u32 GetUniformLocation(u32 programHandle, const i8 *name)
{
	GLCHECK(u32 loc = glGetUniformLocation(programHandle, name));
	return loc;
}

static void SetUniform(u32 programHandle, const i8 *name, u32 size, const void *data,
    enum UniformType type)
{
	const u32 loc = GetUniformLocation(programHandle, name);
	switch (type) {
        case UT_MAT4:
			GLCHECK(glUniformMatrix4fv(loc, 1, GL_FALSE, data));
			break;
        case UT_VEC4F:
			GLCHECK(glUniform4fv(loc, 1, data));
			break;
        case UT_VEC3F:
			GLCHECK(glUniform3fv(loc, 1, data));
			break;
        case UT_VEC2F:
			break;
        case UT_FLOAT:
			break;
        case UT_INT:
			GLCHECK(glUniform1i(loc, *(const i32 *)data));
			break;
        case UT_UINT:
			GLCHECK(glUniform1ui(loc, *(const u32 *)data));
                break;
		default:
			UtilsFatalError("FATAL ERROR: Failed to locate %s uniform");
        }
}

static struct File LoadShader(const i8 *shaderName)
{
	struct File out = {0};
	const u32 len = strlen(shaderName) + strlen(RES_HOME) + 2;
	i8 *absPath = malloc(len);
	snprintf(absPath, len, "%s/%s", RES_HOME, shaderName);
	UtilsDebugPrint("Loading %s", absPath);
	FILE *f = fopen(absPath, "rb");
	if (!f) {
		free(absPath);
		return out;
	}

	fseek(f, 0L, SEEK_END);
	out.size = ftell(f);
	rewind(f);

	out.contents = malloc(out.size);
	const uint64_t numRead = fread(out.contents,
			sizeof (i8), out.size, f);

	UtilsDebugPrint("Read %lu bytes. Expected %lu bytes",
			numRead, out.size);
	fclose(f);
	free(absPath);
	return out;
}

static i32 CompileShader(const struct File *shader, i32 shaderType,
    u32 *pHandle)
{
	GLCHECK(*pHandle = glCreateShader(shaderType));
	GLCHECK(glShaderSource(*pHandle, 1,
			(const GLchar **)&shader->contents,
			(const GLint*)&shader->size));
	GLCHECK(glCompileShader(*pHandle));
	i32 compileStatus = 0;
	GLCHECK(glGetShaderiv(*pHandle, GL_COMPILE_STATUS,
			&compileStatus));
	if (!compileStatus) {
		i32 len = 0;
		GLCHECK(glGetShaderiv(*pHandle, GL_INFO_LOG_LENGTH, &len));
		i8 *msg = malloc(len);
		GLCHECK(glGetShaderInfoLog(*pHandle, len, &len, msg));
		UtilsDebugPrint("ERROR: Failed to compile shader. %s", msg);
		free(msg);
	}

	return compileStatus;
}

static i32 LinkProgram(const u32 vs, const u32 fs, u32 *pHandle)
{
	GLCHECK(*pHandle = glCreateProgram());
	GLCHECK(glAttachShader(*pHandle, vs));
	GLCHECK(glAttachShader(*pHandle, fs));
	GLCHECK(glLinkProgram(*pHandle));

	i32 linkStatus = 0;
	GLCHECK(glGetProgramiv(*pHandle, GL_LINK_STATUS, &linkStatus));
	if (!linkStatus) {
		i32 len = 0;
		GLCHECK(glGetProgramiv(*pHandle, GL_INFO_LOG_LENGTH, &len));
		i8 *msg = malloc(len);
		GLCHECK(glGetProgramInfoLog(*pHandle, len, &len, msg));
		UtilsDebugPrint("ERROR: Failed to link shaders. %s", msg);
		free(msg);
	}

	return linkStatus;
}

static u32 CreateProgram(const i8 *fs, const i8 *vs, const i8 *programName)
{
	u32 programHandle = 0;
	struct File fragSource = LoadShader(fs);
	struct File vertSource = LoadShader(vs);

	u32 fragHandle = 0;
	if(!CompileShader(&fragSource, GL_FRAGMENT_SHADER, &fragHandle))
	{
		return 0;
	}
	u32 vertHandle = 0;
	if(!CompileShader(&vertSource, GL_VERTEX_SHADER, &vertHandle))
	{
		return 0;
	}
	if (!LinkProgram(vertHandle, fragHandle, &programHandle))
	{
		return 0;
	}

	SetObjectName(OI_FRAGMENT_SHADER, fragHandle, UtilsGetStrAfterChar(fs, '/'));
	SetObjectName(OI_VERTEX_SHADER, vertHandle, UtilsGetStrAfterChar(vs, '/'));
	SetObjectName(OI_PROGRAM, programHandle, programName);
	UtilsDebugPrint("Linked program %u (%s)", programHandle, programName);
	GLCHECK(glDeleteShader(vertHandle));
	GLCHECK(glDeleteShader(fragHandle));
	return programHandle;
}

static void CopyMaterialCreateInfo(struct MaterialCreateInfo *dest,
    const struct MaterialCreateInfo *src)
{
    dest->fsPath = src->fsPath;
    dest->vsPath = src->vsPath;
    dest->name = src->name;
    dest->numSamplers = src->numSamplers;
    for (u32 i = 0; i < dest->numSamplers; ++i) {
        dest->samplers[i] = src->samplers[i];
    }
}

struct Material *Material_Create(const struct MaterialCreateInfo *info)
{
    struct Material *m = malloc(sizeof *m);
    m->programHandle = CreateProgram(info->fsPath, info->vsPath, info->name);
    m->name = info->name;
    CopyMaterialCreateInfo(&m->createInfo, info);
    return m;
}

void Material_Destroy(struct Material *m)
{
    GLCHECK(glDeleteProgram(m->programHandle));
    free(m);
    m = NULL;
}

void Material_SetUniform(struct Material *m, const i8 *name, u32 size, 
    const void *data, enum UniformType type)
{
    SetUniform(m->programHandle, name, size, data, type);
}

u32 Material_GetHandle(const struct Material *m)
{
    return m->programHandle;
}

const i8 *Material_GetName(const struct Material *m)
{
    return m->name;
}

void Material_SetTexture(struct Material *m, const i8 *name,
    const struct Texture2D *t)
{
    for (u32 i = 0; i < m->createInfo.numSamplers; ++i) {
        if (strcmp(m->createInfo.samplers[i], name) == 0) {
            const i32 loc = (i32)i;
            Material_SetUniform(m, name, sizeof(i32), &loc, UT_INT);
            GLCHECK(glActiveTexture(GL_TEXTURE0 + loc));
            GLCHECK(glBindTexture(GL_TEXTURE_2D, t->handle));
            break;
        }
    }
}

void SetObjectName(enum ObjectIdentifier objectIdentifier, u32 name, const i8 *label)
{
	const i8 *prefix = NULL;
	i32 identifier = 0;
	switch (objectIdentifier) {
        case OI_BUFFER:
			prefix = "BUFFER";
			identifier = GL_BUFFER;
			break;
	case OI_INDEX_BUFFER:
	  prefix = "EBO";
	  identifier = GL_BUFFER;
	  break;
	case OI_VERTEX_BUFFER:
	  prefix = "VBO";
	  identifier = GL_BUFFER;
	  break;
        case OI_SHADER:
			prefix = "SHADER";
			identifier = GL_SHADER;
			break;
        case OI_VERTEX_SHADER:
			prefix = "VS";
			identifier = GL_SHADER;
			break;
        case OI_FRAGMENT_SHADER:
			prefix = "FS";
			identifier = GL_SHADER;
			break;			
        case OI_PROGRAM:
			prefix = "PROGRAM";
			identifier = GL_PROGRAM;
			break;
        case OI_VERTEX_ARRAY:
			prefix = "VAO";
			identifier = GL_VERTEX_ARRAY;
			break;
        case OI_QUERY:
			prefix = "QUERY";
			identifier = GL_QUERY;
			break;
        case OI_PROGRAM_PIPELINE:
			prefix = "PROGRAM_PIPELINE";
			identifier = GL_PROGRAM_PIPELINE;
			break;
        case OI_TRANSFORM_FEEDBACK:
			prefix = "TRANSFORM_FEEDBACK";
			identifier = GL_TRANSFORM_FEEDBACK;
			break;
        case OI_SAMPLER:
			prefix = "SAMPLER";
			identifier = GL_SAMPLER;
			break;
        case OI_TEXTURE:
			prefix = "TEXTURE";
			identifier = GL_TEXTURE;
			break;
        case OI_RENDERBUFFER:
			prefix = "RENDERBUFFER";
			identifier = GL_RENDERBUFFER;
			break;
        case OI_FRAMEBUFFER:
			prefix = "FRAMEBUFFER";
			identifier = GL_FRAMEBUFFER;
			break;
        }

	const i8 *fullLabel = UtilsFormatStr("%s_%s", label, prefix);
	GLCHECK(glObjectLabel(identifier, name, strlen(fullLabel), fullLabel));
}

struct Texture2D *Texture2D_Create(const struct Texture2DCreateInfo *info)
{
	struct Texture2D *t = malloc(sizeof *t);
	Texture2D_Init(t, info);
	return t;
}

void Texture2D_Init(struct Texture2D *t, const struct Texture2DCreateInfo *info)
{
	u32 handle = 0;
	GLCHECK(glGenTextures(1, &handle));
	GLCHECK(glBindTexture(GL_TEXTURE_2D, handle));
	GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, info->internalFormat, info->width,
		info->height, 0, info->format, info->type, NULL));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	if (info->genFB) {
		GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, info->framebufferAttachment,
			GL_TEXTURE_2D, handle, 0));
	}
	SetObjectName(OI_TEXTURE, handle, info->name);
	t->handle = handle;
	t->width = info->width;
	t->height = info->height;
	t->name = strdup(info->name);
}

void Texture2D_Destroy(struct Texture2D *t)
{
	free(t->name);
	free(t);
	t = NULL;
}

static struct ModelProxy *CreateModelProxy(const struct Model *m);
static struct ModelProxy *LoadModel(const i8 *filename)
{
	const i8 *absPath = UtilsFormatStr("%s/%s", RES_HOME, filename);
	struct Model* model = OLLoad(absPath);
	struct ModelProxy *proxy = NULL;
    if (model) {
	    for (u32 i = 0; i < model->NumMeshes; ++i) {
		    const struct Mesh *mesh = model->Meshes + i;
		    UtilsDebugPrint("Mesh %s, faces: %u, normals: %u, positions: %u, texCoords: %u",
					mesh->Name, mesh->NumFaces,
				    mesh->NumNormals, mesh->NumPositions,
				    mesh->NumTexCoords);
	    }

		proxy = CreateModelProxy(model);
		ModelFree(model);
    }
	return proxy;
}

static void ValidateModelProxy(const struct ModelProxy *m)
{
	UtilsDebugPrint("Validating ModelProxy. Num meshes: %d", m->numMeshes);
	for (u32 i = 0; i < m->numMeshes; ++i) {
		UtilsDebugPrint("Mesh %d, Num indices: %d", i, m->meshes[i].numIndices);
	}
}

struct CalculateTangetData {
	struct Vertex *vertices;
	u32 numVertices;
	const u32 *indices;
	u32 numIndices;
};

static void GetNormal(const SMikkTSpaceContext * pContext, f32 fvNormOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvNormOut[0] = data->vertices[iFace * 3 + iVert].normal.X;
	fvNormOut[1] = data->vertices[iFace * 3 + iVert].normal.Y;
	fvNormOut[2] = data->vertices[iFace * 3 + iVert].normal.Z;
}

static void GetPosition(const SMikkTSpaceContext * pContext, f32 fvPosOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvPosOut[0] = data->vertices[iFace * 3 + iVert].position.X;
	fvPosOut[1] = data->vertices[iFace * 3 + iVert].position.Y;
	fvPosOut[2] = data->vertices[iFace * 3 + iVert].position.Z;
}

static void GetTexCoords(const SMikkTSpaceContext * pContext, f32 fvTexcOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvTexcOut[0] = data->vertices[iFace * 3 + iVert].texCoords.X;
	fvTexcOut[1] = data->vertices[iFace * 3 + iVert].texCoords.Y;
}


static i32 GetNumVerticesOfFace(const SMikkTSpaceContext * pContext, const i32 iFace)
{
	return 3;
}

static void SetTSpaceBasic(const SMikkTSpaceContext * pContext, const f32 fvTangent[],
		const f32 fSign, const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	data->vertices[iFace * 3 + iVert].tangent.X = fvTangent[0];
	data->vertices[iFace * 3 + iVert].tangent.Y = fvTangent[1];
	data->vertices[iFace * 3 + iVert].tangent.Z = fvTangent[2];
	data->vertices[iFace * 3 + iVert].tangent.W = fSign;
}

static i32 GetNumFaces(const SMikkTSpaceContext * pContext)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	return data->numIndices / 3;
}

static void CalculateTangentArray(struct CalculateTangetData *data)
{
	SMikkTSpaceInterface interface = { 0 };
	interface.m_setTSpaceBasic = SetTSpaceBasic;
	interface.m_getNumFaces = GetNumFaces;
	interface.m_getNumVerticesOfFace = GetNumVerticesOfFace;
	interface.m_getPosition = GetPosition;
	interface.m_getNormal = GetNormal;
	interface.m_getTexCoord = GetTexCoords;

	SMikkTSpaceContext context = { .m_pInterface = &interface, .m_pUserData = data };
	genTangSpaceDefault(&context);
}

static struct ModelProxy *CreateModelProxy(const struct Model *m)
{
	if (!m || m->NumMeshes == 0) {
		return NULL;
	}

	const Mat4X4 world = MathMat4X4Identity();

	struct ModelProxy *ret = malloc(sizeof *ret);
	ret->meshes = malloc(sizeof(struct MeshProxy) * m->NumMeshes);
	ret->numMeshes = m->NumMeshes;
	// indices in *.obj are increased from 1 to N
	// and they do not reset to 0 when next mesh starts
	// say we have two cubes and first cube's last indices are
	// 4 0 1
	// then follows next cube and it's indices start from 8 and not from 1
	// 12 10 8
	// thus we add indexOffset and subtract it from index from *.obj
	u32 posIdxOffset = 0;
	u32 normIdxOffset = 0;
	u32 texIdxOffset = 0;
	for (u32 i = 0; i < m->NumMeshes; ++i) {
	    GLCHECK(glGenVertexArrays(1, &ret->meshes[i].vao));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].vbo));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].ebo));

		u32 *indices = malloc(sizeof *indices * m->Meshes[i].NumFaces);
		struct Vertex *vertices = malloc(sizeof *vertices * m->Meshes[i].NumFaces);
		for (u32 j = 0; j < m->Meshes[i].NumFaces; ++j) {
			const u32 posIdx = m->Meshes[i].Faces[j].posIdx - posIdxOffset;
			const u32 normIdx = m->Meshes[i].Faces[j].normIdx - normIdxOffset;
			const u32 texIdx = m->Meshes[i].Faces[j].texIdx - texIdxOffset;
			assert(posIdx < m->Meshes[i].NumPositions);
			assert(normIdx < m->Meshes[i].NumNormals);
			assert(texIdx < m->Meshes[i].NumTexCoords);
			vertices[j].position = *(Vec3D*)&m->Meshes[i].Positions[posIdx];
			vertices[j].normal = *(Vec3D*)&m->Meshes[i].Normals[normIdx];
			vertices[j].texCoords = *(Vec2D*)&m->Meshes[i].TexCoords[texIdx];
			vertices[j].tangent = MathVec4DZero();
			indices[j] = j;
		}
		posIdxOffset += m->Meshes[i].NumPositions;
		normIdxOffset += m->Meshes[i].NumNormals;
		texIdxOffset += m->Meshes[i].NumTexCoords;

		struct CalculateTangetData data = { vertices, m->Meshes[i].NumFaces, indices, m->Meshes[i].NumFaces }	;
		CalculateTangentArray(&data);

	    GLCHECK(glBindVertexArray(ret->meshes[i].vao));
		SetObjectName(OI_VERTEX_ARRAY, ret->meshes[i].vao, m->Meshes[i].Name);
	    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, ret->meshes[i].vbo));
	    GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(struct Vertex) * m->Meshes[i].NumFaces,
			    vertices, GL_STATIC_DRAW));
		free(vertices);


	    GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret->meshes[i].ebo));
	    GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * m->Meshes[i].NumFaces,
				indices, GL_STATIC_DRAW));

	    GLCHECK(glEnableVertexAttribArray(0));
	    GLCHECK(glEnableVertexAttribArray(1));
	    GLCHECK(glEnableVertexAttribArray(2));
	    GLCHECK(glEnableVertexAttribArray(3));
	    GLCHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
					(void*)offsetof(struct Vertex, position)));
	    GLCHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
					(void*)offsetof(struct Vertex, normal)));
	    GLCHECK(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
					(void*)offsetof(struct Vertex, texCoords)));
	    GLCHECK(glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(struct Vertex),
					(void*)offsetof(struct Vertex, tangent)));

		ret->meshes[i].numIndices = m->Meshes[i].NumFaces;
		ret->meshes[i].world = world;
        const u32 nameLen = strlen(m->Meshes[i].Name);
        ret->meshes[i].name = malloc(nameLen + 1);
        ZERO_MEMORY_SZ(ret->meshes[i].name, nameLen + 1);
        memcpy(ret->meshes[i].name, m->Meshes[i].Name, nameLen);

		free(indices);

	    SetObjectName(OI_VERTEX_ARRAY, ret->meshes[i].vao, m->Meshes[i].Name);
	    SetObjectName(OI_VERTEX_BUFFER, ret->meshes[i].vbo, m->Meshes[i].Name);
	    SetObjectName(OI_INDEX_BUFFER, ret->meshes[i].ebo, m->Meshes[i].Name);
	}

//	ValidateModelProxy(ret);

	return ret;
}

struct ModelProxy *ModelProxy_Create(const i8 *path)
{
	return LoadModel(path);
}

void Texture2D_Load(struct Texture2D *t, const i8 *texPath, i32 internalFormat,
		i32 format, i32 type)
{
	i32 channelsInFile = 0;
	u8 *data = stbi_load(texPath, &t->width, &t->height,
		&channelsInFile, STBI_rgb_alpha);
	if (!data) {
		UtilsDebugPrint("ERROR: Failed to load %s", texPath);
		exit(-1);
	}

	const i32 loc = UtilsStrFindLastChar(texPath, '/');
	t->name = strdup(texPath + loc + 1);

	UtilsDebugPrint("Loaded %s: %dx%d, channels: %d", t->name, t->width, t->height,
			channelsInFile);

	glGenTextures(1, &t->handle);
	glBindTexture(GL_TEXTURE_2D, t->handle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, t->width, t->height, 0, format, type, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	SetObjectName(OI_TEXTURE, t->handle, t->name);
	stbi_image_free(data);
}
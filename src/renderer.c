#include "renderer.h"
#include <stdlib.h>
#include <string.h>

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

static i32 LinkProgram(const u32 vs, const u32 fs,
		u32 *pHandle)
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
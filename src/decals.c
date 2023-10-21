#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <string.h>
#include <stdlib.h>

#include "myutils.h"
#include "objloader.h"
#include "mymath.h"

struct File {
	char *contents;
	uint64_t size;
};

struct MeshProxy {
	uint32_t vao;
	uint32_t vbo;
	uint32_t ebo;
	uint32_t numIndices;
	Mat4X4 world;
};

struct ModelProxy {
	struct MeshProxy *meshes;
	uint32_t numMeshes;
};

enum UniformType {
	UT_MAT4,
	UT_VEC4F,
	UT_VEC3F,
	UT_VEC2F,
	UT_FLOAT,
	UT_INT,
	UT_UINT,
};

struct File LoadShader(const char *shaderName);
int LinkProgram(const uint32_t vs, const uint32_t fs, uint32_t *pHandle);
int CompileShader(const struct File *shader, int shaderType, 
		uint32_t *pHandle);
void OnFramebufferResize(GLFWwindow *window, int width, int height);


struct Model *LoadModel(const char *filename);
struct ModelProxy *CreateModelProxy(const struct Model *m);

void SetUniform(uint32_t programHandle, const char *name, uint32_t size, void *data, enum UniformType type);

#define GLCHECK(x) x; \
do { \
	GLenum err; \
	while((err = glGetError()) != GL_NO_ERROR) \
	{ \
		UtilsDebugPrint("ERROR in call to OpenGL at %s:%d\n", __FILE__, __LINE__); \
	} \
} while (0)

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
	}

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
	    return -1;
	}

    glfwSetFramebufferSizeCallback(window, OnFramebufferResize); 

    struct Model *model = LoadModel("assets/room.obj");
    if (model) {
	    for (uint32_t i = 0; i < model->NumMeshes; ++i) {
		    const struct Mesh *mesh = model->Meshes + i;
		    UtilsDebugPrint("Mesh %s, faces: %u, normals: %u, positions: %u, texCoords: %u\n", mesh->Name, mesh->NumFaces,
				    mesh->NumNormals, mesh->NumPositions,
				    mesh->NumTexCoords);
	    }
    }

    struct File vsShaderSource = LoadShader("shaders/vert.glsl");
    struct File fsShaderSource = LoadShader("shaders/frag.glsl");
    uint32_t vsHandle = 0;
    if (!CompileShader(&vsShaderSource, GL_VERTEX_SHADER,
			    &vsHandle)) {
	    UtilsFatalError("ERROR: Failed to compile vertex shader\n");
	    return -1;
    }
    uint32_t fsHandle = 0;
    if (!CompileShader(&fsShaderSource, GL_FRAGMENT_SHADER,
			    &fsHandle)) {
	    UtilsFatalError("ERROR: Failed to compile fragment shader\n");
	    return -1;
    }
    uint32_t programHandle = 0;
    if (!LinkProgram(vsHandle, fsHandle, &programHandle)) {
	    UtilsFatalError("ERROR: Failed to link program\n");
	    return -1;
    }

	glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
	glCullFace(GL_BACK);

	struct ModelProxy *modelProxy = CreateModelProxy(model);

	Mat4X4 g_view = MathMat4X4Identity();
	Mat4X4 g_proj = MathMat4X4Identity();

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        GLCHECK(glClear(GL_COLOR_BUFFER_BIT));
		GLCHECK(glClearColor(1.0f, 1.0f, 0.0f, 1.0f));

		GLCHECK(glUseProgram(programHandle));

		SetUniform(programHandle, "g_view", sizeof(Mat4X4), &g_view, UT_MAT4);
		SetUniform(programHandle, "g_proj", sizeof(Mat4X4), &g_proj, UT_MAT4);

		for (uint32_t i = 0; i < modelProxy->numMeshes; ++i) {
			GLCHECK(glBindVertexArray(modelProxy->meshes[i].vao));
			SetUniform(programHandle, "g_world", sizeof(Mat4X4), &modelProxy->meshes[i].world, UT_MAT4);
			GLCHECK(glDrawElements(GL_TRIANGLES, modelProxy->meshes[i].numIndices, GL_UNSIGNED_INT,
				NULL));
		}

		
        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

struct File LoadShader(const char *shaderName)
{
	struct File out = {0};
	const uint32_t len = strlen(shaderName) + strlen(RES_HOME) + 2;
	char *absPath = malloc(len);
	snprintf(absPath, len, "%s/%s", RES_HOME, shaderName);
	UtilsDebugPrint("Loading %s\n", absPath);
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
			sizeof (char), out.size, f);

	UtilsDebugPrint("Read %lu bytes. Expected %lu bytes\n",
			numRead, out.size);
	fclose(f);
	free(absPath);
	return out;
}

int CompileShader(const struct File *shader, int shaderType,
		uint32_t *pHandle)
{
	GLCHECK(*pHandle = glCreateShader(shaderType));
	GLCHECK(glShaderSource(*pHandle, 1, 
			(const GLchar **)&shader->contents,
			(const GLint*)&shader->size));
	GLCHECK(glCompileShader(*pHandle));
	int compileStatus = 0;
	GLCHECK(glGetShaderiv(*pHandle, GL_COMPILE_STATUS,
			&compileStatus));
	if (!compileStatus) {
		int len = 0;
		GLCHECK(glGetShaderiv(*pHandle, GL_INFO_LOG_LENGTH, &len));
		char *msg = malloc(len);
		GLCHECK(glGetShaderInfoLog(*pHandle, len, &len, msg));
		UtilsDebugPrint("ERROR: Failed to compile shader. %s\n", msg);
		free(msg);
	}

	return compileStatus;
}

int LinkProgram(const uint32_t vs, const uint32_t fs,
		uint32_t *pHandle)
{
	GLCHECK(*pHandle = glCreateProgram());
	GLCHECK(glAttachShader(*pHandle, vs));
	GLCHECK(glAttachShader(*pHandle, fs));
	GLCHECK(glLinkProgram(*pHandle));
	
	int linkStatus = 0;
	GLCHECK(glGetProgramiv(*pHandle, GL_LINK_STATUS, &linkStatus));

	return linkStatus;
}


void OnFramebufferResize(GLFWwindow *window, int width,
		int height)
{
	GLCHECK(glViewport(0, 0, width, height));
}

struct Model *LoadModel(const char *filename)
{
	const char *absPath = UtilsFormatStr("%s/%s", RES_HOME, filename);
	struct Model* model = OLLoad(absPath);
	return model;
}

void PrintModelToFile(const struct Model *m)
{
	if (!m) {
		return;
	}

	FILE *out = fopen("model.txt", "w");
	for (uint32_t i = 0; i < m->NumMeshes; ++i) {
		fprintf(out, "%s\n", m->Meshes[i].Name);
		fprintf(out, "Num positions: %d, num indices: %d\n", m->Meshes[i].NumPositions,
				m->Meshes[i].NumFaces);
		for (uint32_t j = 0; j < m->Meshes[i].NumFaces; j += 3) {
			fprintf(out, "%d %d %d\n", m->Meshes[i].Faces[j].posIdx,
					m->Meshes[i].Faces[j + 1].posIdx,
					m->Meshes[i].Faces[j + 2].posIdx);
		}
		for (uint32_t j = 0; j < m->Meshes[i].NumPositions; ++j) {
			fprintf(out, "%f %f %f\n", m->Meshes[i].Positions[j].x,
				m->Meshes[i].Positions[j].y,
				m->Meshes[i].Positions[j].z);
		}
		fprintf(out, "\n\n");
	}
	fclose(out);
}

void ValidateModelProxy(const struct ModelProxy *m)
{
	UtilsDebugPrint("Validating ModelProxy. Num meshes: %d\n", m->numMeshes);
	for (uint32_t i = 0; i < m->numMeshes; ++i) {
		UtilsDebugPrint("Mesh %d, Num indices: %d\n", i, m->meshes[i].numIndices);
	}
}

struct ModelProxy *CreateModelProxy(const struct Model *m)
{
	if (!m || m->NumMeshes == 0) {
		return NULL;
	}

	PrintModelToFile(m);

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
	uint32_t indexOffset = 0;
	for (uint32_t i = 0; i < m->NumMeshes; ++i) {
	    GLCHECK(glGenVertexArrays(1, &ret->meshes[i].vao));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].vbo));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].ebo));

	    GLCHECK(glBindVertexArray(ret->meshes[i].vao));
	    GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, ret->meshes[i].vbo));
	    GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(struct Position) * m->Meshes[i].NumPositions,
			    m->Meshes[i].Positions, GL_STATIC_DRAW));

		uint32_t *indices = malloc(sizeof *indices * m->Meshes[i].NumFaces);
		for (uint32_t j = 0; j < m->Meshes[i].NumFaces; ++j) {
			indices[j] = m->Meshes[i].Faces[j].posIdx - indexOffset;
		}
		indexOffset += m->Meshes[i].NumFaces;
	    GLCHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret->meshes[i].ebo));
	    GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m->Meshes[i].NumFaces, 
				indices, GL_STATIC_DRAW));

	    GLCHECK(glEnableVertexAttribArray(0));
	    GLCHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			    0, NULL));

		ret->meshes[i].numIndices = m->Meshes[i].NumFaces;

		free(indices);
	}

	ValidateModelProxy(ret);

	return ret;
}


uint32_t GetUniformLocation(uint32_t programHandle, const char *name)
{
	GLCHECK(uint32_t loc = glGetUniformLocation(programHandle, name));
	return loc;
}

void SetUniform(uint32_t programHandle, const char *name, uint32_t size, void *data, enum UniformType type)
{
	const uint32_t loc = GetUniformLocation(programHandle, name);
	switch (type) {
        case UT_MAT4:
			GLCHECK(glUniformMatrix4fv(loc, 1, GL_FALSE, data));
			break;
        case UT_VEC4F:
			break;
        case UT_VEC3F:
			break;
        case UT_VEC2F:
			break;
        case UT_FLOAT:
			break;
        case UT_INT:
			break;
        case UT_UINT:
                break;
        }
}

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
};

struct ModelProxy {
	struct MeshProxy *meshes;
	uint32_t numMeshes;
};

struct File LoadShader(const char *shaderName);
int LinkProgram(const uint32_t vs, const uint32_t fs, uint32_t *pHandle);
int CompileShader(const struct File *shader, int shaderType, 
		uint32_t *pHandle);
void OnFramebufferResize(GLFWwindow *window, int width, int height);


struct Model *LoadModel(const char *filename);
struct ModelProxy *CreateModelProxy(const struct Model *m);

struct PerCamera {
	Mat4X4 viewProj;
};

struct PerObject {
	Mat4X4 world;
};

int main(void)
{
    GLFWwindow* window;

    /* Initialize the library */
    if (!glfwInit())
        return -1;

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0)
	    return -1;

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

    struct vec3 {
	    float x;
	    float y;
	    float z;
    };
    struct vec2 {
	    float u;
	    float v;
    };
    struct Vertex {
	   struct vec3 position;
	   struct vec3 normal;
	   struct vec2 texcoords;
    };

    const struct Vertex vertices[] = {
	    { .position = {-0.5f, -0.5f, 0.5f } },
	    { .position = {0.0f, 0.5f, 0.5f } },
	    { .position = {0.5f, -0.5f, 0.5f } }
    };

    const uint32_t indices[] = {0, 1, 2};

    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ebo = 0;

//    glGenVertexArrays(1, &vao);
//    glGenBuffers(1, &vbo);
//    glGenBuffers(1, &ebo);
//
//    glBindVertexArray(vao);
//
//    glBindBuffer(GL_ARRAY_BUFFER, vbo);
//    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
//
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);
//
//    glEnableVertexAttribArray(0);
//    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
//		    sizeof (struct Vertex), NULL);
//    glBindVertexArray(0);

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

 //   glDisable(GL_CULL_FACE);
//    glCullFace(GL_BACK);
//    glEnable(GL_DEPTH_TEST);
  //  glDisable(GL_DEPTH_TEST);

	struct ModelProxy *modelProxy = CreateModelProxy(model);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(1.0f, 1.0f, 0.0f, 1.0f);


		glUseProgram(programHandle);

		for (uint32_t i = 0; i < modelProxy->numMeshes; ++i) {
			glBindVertexArray(modelProxy->meshes[i].vao);

			glDrawElements(GL_TRIANGLES, modelProxy->meshes[i].numIndices, GL_UNSIGNED_INT,
				NULL);
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
	*pHandle = glCreateShader(shaderType);
	glShaderSource(*pHandle, 1, 
			(const GLchar **)&shader->contents,
			(const GLint*)&shader->size);
	glCompileShader(*pHandle);
	int compileStatus = 0;
	glGetShaderiv(*pHandle, GL_COMPILE_STATUS,
			&compileStatus);

	return compileStatus;
}

int LinkProgram(const uint32_t vs, const uint32_t fs,
		uint32_t *pHandle)
{
	*pHandle = glCreateProgram();
	glAttachShader(*pHandle, vs);
	glAttachShader(*pHandle, fs);
	glLinkProgram(*pHandle);
	
	int linkStatus = 0;
	glGetProgramiv(*pHandle, GL_LINK_STATUS, &linkStatus);

	return linkStatus;
}


void OnFramebufferResize(GLFWwindow *window, int width,
		int height)
{
	glViewport(0, 0, width, height);
}

struct Model *LoadModel(const char *filename)
{
	const char *absPath = UtilsFormatStr("%s/%s", RES_HOME, filename);
	struct Model* model = OLLoad(absPath);
	return model;
}


struct ModelProxy *CreateModelProxy(const struct Model *m)
{
	if (!m || m->NumMeshes == 0) {
		return NULL;
	}

	struct ModelProxy *ret = malloc(sizeof *ret);
	ret->meshes = malloc(sizeof(struct MeshProxy) * m->NumMeshes);
	ret->numMeshes = m->NumMeshes;
	for (uint32_t i = 0; i < m->NumMeshes; ++i) {
	    glGenVertexArrays(1, &ret->meshes[i].vao);
	    glGenBuffers(1, &ret->meshes[i].vbo);
	    glGenBuffers(1, &ret->meshes[i].ebo);

	    glBindVertexArray(ret->meshes[i].vao);

	    glBindBuffer(GL_ARRAY_BUFFER, ret->meshes[i].vbo);
	    glBufferData(GL_ARRAY_BUFFER, sizeof(struct Position) * m->Meshes[i].NumPositions,
			    m->Meshes[i].Positions, GL_STATIC_DRAW);

		uint32_t *indices = malloc(sizeof *indices * m->Meshes[i].NumFaces);
		for (uint32_t j = 0; j < m->Meshes[i].NumFaces; ++j) {
			indices[j] = m->Meshes[i].Faces[j].posIdx;
		}
	    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ret->meshes[i].ebo);
	    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m->Meshes[i].NumFaces, 
				indices, GL_STATIC_DRAW);

	    glEnableVertexAttribArray(0);
	    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
			    sizeof (struct Position), NULL);

		ret->meshes[i].numIndices = m->Meshes[i].NumFaces;

		free(indices);
	}

	return ret;
}

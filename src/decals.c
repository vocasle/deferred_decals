#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct File {
	char *contents;
	uint64_t size;
};

struct File LoadShader(const char *shaderName);
int LinkProgram(const uint32_t vs, const uint32_t fs,
		uint32_t *pHandle);
int CompileShader(const struct File *shader, int shaderType,
		uint32_t *pHandle);
void OnFramebufferResize(GLFWwindow *window, int width,
		int height);

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

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof indices, indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
		    sizeof (struct Vertex), NULL);
    glBindVertexArray(0);

    struct File vsShaderSource = LoadShader("shaders/vert.glsl");
    struct File fsShaderSource = LoadShader("shaders/frag.glsl");
    uint32_t vsHandle = 0;
    if (!CompileShader(&vsShaderSource, GL_VERTEX_SHADER,
			    &vsHandle)) {
	    printf("ERROR: Failed to compile vertex shader\n");
	    return -1;
    }
    uint32_t fsHandle = 0;
    if (!CompileShader(&fsShaderSource, GL_FRAGMENT_SHADER,
			    &fsHandle)) {
	    printf("ERROR: Failed to compile fragment shader\n");
	    return -1;
    }
    uint32_t programHandle = 0;
    if (!LinkProgram(vsHandle, fsHandle, &programHandle)) {
	    printf("ERROR: Failed to link program\n");
	    return -1;
    }

 //   glDisable(GL_CULL_FACE);
//    glCullFace(GL_BACK);
//    glEnable(GL_DEPTH_TEST);
  //  glDisable(GL_DEPTH_TEST);

    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
	glClearColor(1.0f, 1.0f, 0.0f, 1.0f);

	glBindVertexArray(vao);
	glUseProgram(programHandle);

	glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT,
			NULL);

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
	printf("Loading %s\n", absPath);
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

	printf("Read %lu bytes. Expected %lu bytes\n",
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

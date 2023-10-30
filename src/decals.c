#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <mikktspace.h>

#include "myutils.h"
#include "objloader.h"
#include "mymath.h"

struct File {
	char *contents;
	uint64_t size;
};

struct Vertex {
	Vec3D position;
	Vec3D normal;
	Vec2D texCoords;
	Vec4D tangent;
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

enum ObjectIdentifier {
	OI_BUFFER,
	OI_SHADER,
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

enum GBufferDebugMode {
	GDM_NONE,
	GDM_VERTEX_NORMAL,
	GDM_TANGENT,
	GDM_BITANGENT,
	GDM_NORMAL_MAP,
	GDM_ALBEDO,
	GDM_POSITION,
};

struct GBuffer {
	uint32_t position;
	uint32_t normal;
	uint32_t albedo;
	uint32_t framebuffer;
	uint32_t depthBuffer;
};

struct FullscreenQuadPass {
	uint32_t vbo;
	uint32_t vao;
};

struct FramebufferSize {
	int width;
	int height;
};

struct Texture2D {
	int32_t width;
	int32_t height;
	char *name;
	uint32_t handle;
	int32_t samplerLocation;
};

struct Game {
	struct GBuffer gbuffer;
	struct FramebufferSize framebufferSize;
	enum GBufferDebugMode gbufferDebugMode;
	struct Texture2D *albedoTextures;
	struct Texture2D *normalTextures;
	struct Texture2D *roughnessTextures;
	uint32_t numTextures;
};

struct File LoadShader(const char *shaderName);
int LinkProgram(const uint32_t vs, const uint32_t fs, uint32_t *pHandle);
int CompileShader(const struct File *shader, int shaderType, 
		uint32_t *pHandle);
void OnFramebufferResize(GLFWwindow *window, int width, int height);

struct Model *LoadModel(const char *filename);
struct ModelProxy *CreateModelProxy(const struct Model *m);

void SetUniform(uint32_t programHandle, const char *name, uint32_t size, const void *data, enum UniformType type);

void RotateLight(Vec3D *light, float radius);
double GetDeltaTime();

uint32_t CreateTexture2D(uint32_t width, uint32_t height, int internalFormat,
		int format, int type, int attachment, int genFB, const char *imagePath);

int CreateProgram(const char *fs, const char *vs, uint32_t *programHandle);

void RenderQuad(const struct FullscreenQuadPass *fsqPass);

void InitQuadPass(struct FullscreenQuadPass *fsqPass);

struct CalculateTangetData {
	struct Vertex *vertices;
	uint32_t numVertices;
	const uint32_t *indices;
	uint32_t numIndices;
};
void CalculateTangentArray(struct CalculateTangetData *data);

void SetObjectName(enum ObjectIdentifier objectIdentifier, uint32_t name,
		const char *label);

void ProcessInput(GLFWwindow* window, int key, int scancode, int action, int mods);

void Texture2D_Load(struct Texture2D *t, const char *texPath, int internalFormat,
		int format, int type);

#define GLCHECK(x) x; \
do { \
	GLenum err; \
	while((err = glGetError()) != GL_NO_ERROR) \
	{ \
		UtilsDebugPrint("ERROR in call to OpenGL at %s:%d", __FILE__, __LINE__); \
		raise(SIGTRAP); \
		exit(-1); \
	} \
} while (0)

#define ARRAY_COUNT(x) (uint32_t)(sizeof(x) / sizeof(x[0]))

void GLAPIENTRY
MessageCallback(GLenum source,
                GLenum type,
                GLuint id,
                GLenum severity,
                GLsizei length,
                const GLchar* message,
                const void* userParam)
{
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
            type, severity, message);
  exit(-1);
}

int InitGBuffer(struct GBuffer *gbuffer, const int fbWidth, const int fbHeight);

int main(void)
{
    GLFWwindow* window = NULL;

    /* Initialize the library */
    if (!glfwInit()) {
        return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

	struct Game game = { 0 };
	glfwGetFramebufferSize(window, &game.framebufferSize.width,
			&game.framebufferSize.height);
	glfwSetWindowUserPointer(window, &game);

	glfwSetKeyCallback(window, ProcessInput);

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
		    UtilsDebugPrint("Mesh %s, faces: %u, normals: %u, positions: %u, texCoords: %u",
					mesh->Name, mesh->NumFaces,
				    mesh->NumNormals, mesh->NumPositions,
				    mesh->NumTexCoords);
	    }
    }

//	uint32_t programHandle = 0;
//	if (!CreateProgram("shaders/frag.glsl", "shaders/vert.glsl", &programHandle)) {
//		UtilsFatalError("ERROR: Failed to create program\n");
//		return -1;
//	}

	uint32_t gbufferProgram = 0;
	if (!CreateProgram("shaders/gbuffer_frag.glsl", "shaders/vert.glsl", &gbufferProgram)) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	uint32_t deferredProgram = 0;
	if (!CreateProgram("shaders/deferred_frag.glsl", "shaders/deferred_vert.glsl", &deferredProgram)) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
//	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	struct ModelProxy *modelProxy = CreateModelProxy(model);

	const Vec3D origin = { 0 };
	const Vec3D up = { .Y = 1.0f }; 
	const Vec3D eyePos = { .Y = 15.0f, .Z = -30.0f };
	const float zNear = 0.1f;
	const float zFar = 1000.0f;
	Mat4X4 g_view = MathMat4X4ViewAt(&eyePos, &origin, &up);
	Mat4X4 g_proj = MathMat4X4PerspectiveFov(MathToRadians(90.0f),
			(float)game.framebufferSize.width / (float)game.framebufferSize.height,
			zNear, zFar);


//	Mat4X4 g_world = MathMat4X4RotateY(MathToRadians(90.0f));
	Mat4X4 g_world = MathMat4X4Identity();
	g_world.A[2][2] = -g_world.A[2][2];
	Mat4X4 rotY90 = MathMat4X4RotateY(MathToRadians(90.0f));
	g_world = MathMat4X4MultMat4X4ByMat4X4(&g_world, &rotY90);
	const float radius = 35.0f;
	Vec3D g_lightPos = { 0.0, 50.0, -20.0 };

	InitGBuffer(&game.gbuffer, game.framebufferSize.width, game.framebufferSize.height);

	struct FullscreenQuadPass fsqPass = { 0 };
	InitQuadPass(&fsqPass);
	stbi_set_flip_vertically_on_load(1);
	const char *albedoTexturePaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_albedo.png",
		"assets/dry-rocky-ground-bl/dry-rocky-ground_albedo.png",
		"assets/painted-worn-asphalt-bl/painted-worn-asphalt_albedo.png",
		"assets/rusty-metal-bl/rusty-metal_albedo.png"
	};

	const char *normalTexturesPaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_normal-ogl.png",
		"assets/dry-rocky-ground-bl/dry-rocky-ground_normal-ogl.png",
		"assets/painted-worn-asphalt-bl/painted-worn-asphalt_normal-ogl.png",
		"assets/rusty-metal-bl/rusty-metal_normal-ogl.png"
	};

	const char *roughnessTexturePaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_roughness.png",
		"assets/dry-rocky-ground-bl/dry-rocky-ground_roughness.png",
		"assets/painted-worn-asphalt-bl/painted-worn-asphalt_roughness.png",
		"assets/rusty-metal-bl/rusty-metal_roughness.png",
	};

	game.numTextures = ARRAY_COUNT(albedoTexturePaths);
	game.albedoTextures = malloc(sizeof *game.albedoTextures * game.numTextures);
	memset(game.albedoTextures, 0, sizeof *game.albedoTextures * game.numTextures);
	game.roughnessTextures = malloc(sizeof *game.roughnessTextures * game.numTextures);
	memset(game.roughnessTextures, 0, sizeof *game.roughnessTextures * game.numTextures);
	game.normalTextures = malloc(sizeof *game.normalTextures * game.numTextures);
	memset(game.normalTextures, 0, sizeof *game.normalTextures * game.numTextures);

	for (uint32_t i = 0; i < ARRAY_COUNT(albedoTexturePaths); ++i) {
		Texture2D_Load(game.albedoTextures + i, albedoTexturePaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
		Texture2D_Load(game.normalTextures + i, normalTexturesPaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
		Texture2D_Load(game.roughnessTextures + i, roughnessTexturePaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	}

	const int32_t C_ALBEDO_TEX_LOC = 0;
	const int32_t C_NORMAL_TEX_LOC = 1;
	const int32_t C_ROUGHNESS_TEX_LOC = 2;
	
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
		// GBuffer Pass
		// bind GBuffer and draw geometry to GBuffer
		{
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, game.gbuffer.framebuffer));
			GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			GLCHECK(glUseProgram(gbufferProgram));

			SetUniform(gbufferProgram, "g_view", sizeof(Mat4X4), &g_view, UT_MAT4);
			SetUniform(gbufferProgram, "g_proj", sizeof(Mat4X4), &g_proj, UT_MAT4);
			RotateLight(&g_lightPos, radius);
			SetUniform(gbufferProgram, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
			SetUniform(gbufferProgram, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);

			SetUniform(gbufferProgram, "g_albedoTex", sizeof(int32_t), &C_ALBEDO_TEX_LOC, UT_INT); 
			SetUniform(gbufferProgram, "g_normalTex", sizeof(int32_t), &C_NORMAL_TEX_LOC, UT_INT); 
			SetUniform(gbufferProgram, "g_roughnessTex", sizeof(int32_t),
					&C_ROUGHNESS_TEX_LOC, UT_INT); 
			SetUniform(gbufferProgram, "g_gbufferDebugMode", sizeof(int32_t),
					&game.gbufferDebugMode, UT_INT); 

			for (uint32_t i = 0; i < modelProxy->numMeshes; ++i) {
				const uint32_t texHandleIdx = i % ARRAY_COUNT(albedoTexturePaths);
//				UtilsDebugPrint("Binding texture %u (%s) for model %u",
//						texHandleIdx, albedoTexturePaths[texHandleIdx], i);
				GLCHECK(glActiveTexture(GL_TEXTURE0));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, game.albedoTextures[texHandleIdx].handle));
				GLCHECK(glActiveTexture(GL_TEXTURE1));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, game.normalTextures[texHandleIdx].handle));
				GLCHECK(glActiveTexture(GL_TEXTURE2));
				GLCHECK(glBindTexture(GL_TEXTURE_2D, game.roughnessTextures[texHandleIdx].handle));

				GLCHECK(glBindVertexArray(modelProxy->meshes[i].vao));
	//			SetUniform(programHandle, "g_world", sizeof(Mat4X4), &modelProxy->meshes[i].world, UT_MAT4);
				SetUniform(gbufferProgram, "g_world", sizeof(Mat4X4), &g_world, UT_MAT4);
				GLCHECK(glDrawElements(GL_TRIANGLES, modelProxy->meshes[i].numIndices, GL_UNSIGNED_INT,
					NULL));
			}
			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
		}

		// Deferred Shading Pass
		{
			GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			GLCHECK(glUseProgram(deferredProgram));
			GLCHECK(glActiveTexture(GL_TEXTURE0));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.position));
			GLCHECK(glActiveTexture(GL_TEXTURE1));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.normal));
			GLCHECK(glActiveTexture(GL_TEXTURE2));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.albedo));
	//		SetUniform(deferredProgram, "g_view", sizeof(Mat4X4), &g_view, UT_MAT4);
	//		SetUniform(deferredProgram, "g_proj", sizeof(Mat4X4), &g_proj, UT_MAT4);
			RotateLight(&g_lightPos, radius);
			SetUniform(deferredProgram, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
			SetUniform(deferredProgram, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);
			SetUniform(deferredProgram, "g_gbufferDebugMode", sizeof(int32_t),
					&game.gbufferDebugMode, UT_INT); 

			const uint32_t g_position = 0;
			const uint32_t g_normal = 1;
			const uint32_t g_albedo = 2;
			SetUniform(deferredProgram, "g_position", sizeof(uint32_t), &g_position, UT_INT);
			SetUniform(deferredProgram, "g_normal", sizeof(uint32_t), &g_normal, UT_INT);
			SetUniform(deferredProgram, "g_albedo", sizeof(uint32_t), &g_albedo, UT_INT);
			RenderQuad(&fsqPass);
		}

		// Copy gbuffer depth to default framebuffer's depth 
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, game.gbuffer.framebuffer);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the 		
        // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
			glBlitFramebuffer(0, 0, game.framebufferSize.width, game.framebufferSize.height,
					0, 0, game.framebufferSize.width, game.framebufferSize.height, 
					GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}

int CreateProgram(const char *fs, const char *vs, uint32_t *programHandle)
{
	struct File fragSource = LoadShader(fs);
	struct File vertSource = LoadShader(vs);

	uint32_t fragHandle = 0;
	if(!CompileShader(&fragSource, GL_FRAGMENT_SHADER, &fragHandle))
	{
		return 0;
	}
	uint32_t vertHandle = 0;
	if(!CompileShader(&vertSource, GL_VERTEX_SHADER, &vertHandle))
	{
		return 0;
	}
	if (!LinkProgram(vertHandle, fragHandle, programHandle))
	{
		return 0;
	}
	UtilsDebugPrint("Linked program %u", *programHandle);
	GLCHECK(glDeleteShader(vertHandle));
	GLCHECK(glDeleteShader(fragHandle));
	return 1;
}

struct File LoadShader(const char *shaderName)
{
	struct File out = {0};
	const uint32_t len = strlen(shaderName) + strlen(RES_HOME) + 2;
	char *absPath = malloc(len);
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
			sizeof (char), out.size, f);

	UtilsDebugPrint("Read %lu bytes. Expected %lu bytes",
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
		UtilsDebugPrint("ERROR: Failed to compile shader. %s", msg);
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
	if (!linkStatus) {
		int len = 0;
		GLCHECK(glGetProgramiv(*pHandle, GL_INFO_LOG_LENGTH, &len));
		char *msg = malloc(len);
		GLCHECK(glGetProgramInfoLog(*pHandle, len, &len, msg));
		UtilsDebugPrint("ERROR: Failed to link shaders. %s", msg);
		free(msg);
	}

	return linkStatus;
}


void OnFramebufferResize(GLFWwindow *window, int width,
		int height)
{
	GLCHECK(glViewport(0, 0, width, height));
	struct Game *game = glfwGetWindowUserPointer(window);
	game->framebufferSize.width = width;
	game->framebufferSize.height = height;
	InitGBuffer(&game->gbuffer, width, height);
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
	UtilsDebugPrint("Validating ModelProxy. Num meshes: %d", m->numMeshes);
	for (uint32_t i = 0; i < m->numMeshes; ++i) {
		UtilsDebugPrint("Mesh %d, Num indices: %d", i, m->meshes[i].numIndices);
	}
}

struct ModelProxy *CreateModelProxy(const struct Model *m)
{
	if (!m || m->NumMeshes == 0) {
		return NULL;
	}

//	PrintModelToFile(m);
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
	uint32_t posIdxOffset = 0;
	uint32_t normIdxOffset = 0;
	uint32_t texIdxOffset = 0;
	for (uint32_t i = 0; i < m->NumMeshes; ++i) {
	    GLCHECK(glGenVertexArrays(1, &ret->meshes[i].vao));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].vbo));
	    GLCHECK(glGenBuffers(1, &ret->meshes[i].ebo));

		uint32_t *indices = malloc(sizeof *indices * m->Meshes[i].NumFaces);
		struct Vertex *vertices = malloc(sizeof *vertices * m->Meshes[i].NumFaces);
		for (uint32_t j = 0; j < m->Meshes[i].NumFaces; ++j) {
			const uint32_t posIdx = m->Meshes[i].Faces[j].posIdx - posIdxOffset;
			const uint32_t normIdx = m->Meshes[i].Faces[j].normIdx - normIdxOffset;
			const uint32_t texIdx = m->Meshes[i].Faces[j].texIdx - texIdxOffset;
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
	    GLCHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * m->Meshes[i].NumFaces, 
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

		free(indices);
	}

//	ValidateModelProxy(ret);

	return ret;
}


uint32_t GetUniformLocation(uint32_t programHandle, const char *name)
{
	GLCHECK(uint32_t loc = glGetUniformLocation(programHandle, name));
	return loc;
}

void SetUniform(uint32_t programHandle, const char *name, uint32_t size, const void *data, enum UniformType type)
{
	const uint32_t loc = GetUniformLocation(programHandle, name);
	switch (type) {
        case UT_MAT4:
			GLCHECK(glUniformMatrix4fv(loc, 1, GL_FALSE, data));
			break;
        case UT_VEC4F:
			break;
        case UT_VEC3F:
			GLCHECK(glUniform3fv(loc, 1, data));
			break;
        case UT_VEC2F:
			break;
        case UT_FLOAT:
			break;
        case UT_INT:
			GLCHECK(glUniform1i(loc, *(const int32_t *)data));
			break;
        case UT_UINT:
			GLCHECK(glUniform1ui(loc, *(const uint32_t *)data));
                break;
        }
}

double GetDeltaTime()
{
	static double prevTime = 0.0;
	const double now = glfwGetTime();
	const double dt = now - prevTime;
	prevTime = now;
	return dt;
}

void RotateLight(Vec3D *light, float radius)
{
	static double time = 0.0;
	double dt = GetDeltaTime();
	time += dt;
	const float x = (float)(radius * cos(time));
	const float z = (float)(radius * sin(time));
	light->X = x;
	light->Z = z;
}

uint32_t CreateTexture2D(uint32_t width, uint32_t height, int internalFormat,
		int format, int type, int attachment, int genFB, const char *imagePath)
{
	uint8_t *data = NULL;
	if (imagePath)
	{
		int w = 0;
		int h = 0;
		int channelsInFile = 0;
		data = stbi_load(UtilsFormatStr("%s/%s", RES_HOME, imagePath), &w, &h,
			&channelsInFile, STBI_rgb_alpha);
		if (!data) {
			UtilsDebugPrint("ERROR: Failed to load %s", imagePath);
			exit(-1);
		}
		width = w;
		height = h;
	}

	uint32_t handle = 0;
	glGenTextures(1, &handle);
	glBindTexture(GL_TEXTURE_2D, handle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	if (genFB) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, handle, 0);
	}
	if (data) {
		stbi_image_free(data);
	}
	return handle;
}

void InitQuadPass(struct FullscreenQuadPass *fsqPass)
{
	const float quadVertices[] = {
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	glGenVertexArrays(1, &fsqPass->vao);
	glGenBuffers(1, &fsqPass->vbo);
	glBindVertexArray(fsqPass->vao);
	glBindBuffer(GL_ARRAY_BUFFER, fsqPass->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
}

void RenderQuad(const struct FullscreenQuadPass *fsqPass)
{
    glBindVertexArray(fsqPass->vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

int InitGBuffer(struct GBuffer *gbuffer, const int fbWidth, const int fbHeight)
{
	GLCHECK(glGenFramebuffers(1, &gbuffer->framebuffer));
    GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, gbuffer->framebuffer));

    GLCHECK(glGenRenderbuffers(1, &gbuffer->depthBuffer));
    GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, gbuffer->depthBuffer));
    GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
				fbWidth, fbHeight));
    GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
				GL_RENDERBUFFER, gbuffer->depthBuffer));

	gbuffer->position = CreateTexture2D(fbWidth, fbHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT,
			GL_COLOR_ATTACHMENT0, GL_TRUE, NULL);
	gbuffer->normal = CreateTexture2D(fbWidth, fbHeight, GL_RGBA16F, GL_RGBA, GL_FLOAT,
		GL_COLOR_ATTACHMENT1, GL_TRUE, NULL);
	gbuffer->albedo = CreateTexture2D(fbWidth, fbHeight, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE,
		GL_COLOR_ATTACHMENT2, GL_TRUE, NULL);

	const uint32_t attachments[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2 };
	GLCHECK(glDrawBuffers(3, attachments));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		UtilsDebugPrint("ERROR: Failed to create GBuffer framebuffer");
		return 0;
	}
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return 1;
}

void GetNormal(const SMikkTSpaceContext * pContext, float fvNormOut[], const int iFace, const int iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvNormOut[0] = data->vertices[iFace * 3 + iVert].normal.X;
	fvNormOut[1] = data->vertices[iFace * 3 + iVert].normal.Y;
	fvNormOut[2] = data->vertices[iFace * 3 + iVert].normal.Z;
}

void GetPosition(const SMikkTSpaceContext * pContext, float fvPosOut[], const int iFace, const int iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvPosOut[0] = data->vertices[iFace * 3 + iVert].position.X;
	fvPosOut[1] = data->vertices[iFace * 3 + iVert].position.Y;
	fvPosOut[2] = data->vertices[iFace * 3 + iVert].position.Z;
}

void GetTexCoords(const SMikkTSpaceContext * pContext, float fvTexcOut[], const int iFace, const int iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvTexcOut[0] = data->vertices[iFace * 3 + iVert].texCoords.X;
	fvTexcOut[1] = data->vertices[iFace * 3 + iVert].texCoords.Y;
}


int GetNumVerticesOfFace(const SMikkTSpaceContext * pContext, const int iFace)
{
	return 3;
}

void SetTSpaceBasic(const SMikkTSpaceContext * pContext, const float fvTangent[],
		const float fSign, const int iFace, const int iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	data->vertices[iFace * 3 + iVert].tangent.X = fvTangent[0];
	data->vertices[iFace * 3 + iVert].tangent.Y = fvTangent[1];
	data->vertices[iFace * 3 + iVert].tangent.Z = fvTangent[2];
	data->vertices[iFace * 3 + iVert].tangent.W = fSign;
}

int GetNumFaces(const SMikkTSpaceContext * pContext)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	return data->numIndices / 3;
}

void CalculateTangentArray(struct CalculateTangetData *data)
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

void SetObjectName(enum ObjectIdentifier objectIdentifier, uint32_t name, const char *label)
{
	const char *prefix = NULL;
	int identifier = 0; 
	switch (objectIdentifier) {
        case OI_BUFFER:
			prefix = "BUFFER";
			identifier = GL_BUFFER;
			break;
        case OI_SHADER:
			prefix = "SHADER";
			identifier = GL_SHADER;
			break;
        case OI_PROGRAM:
			prefix = "PROGRAM";
			identifier = GL_PROGRAM;
			break;
        case OI_VERTEX_ARRAY:
			prefix = "VERTEX_ARRAY";
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

	const char *fullLabel = UtilsFormatStr("%s_%s", prefix, label);
	GLCHECK(glObjectLabel(identifier, name, strlen(fullLabel), fullLabel));
}

void ProcessInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	struct Game *game = glfwGetWindowUserPointer(window);
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, 1);	
	}
	else if (key == GLFW_KEY_0 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_NONE;
	}
	else if (key == GLFW_KEY_1 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_VERTEX_NORMAL;
	}
	else if (key == GLFW_KEY_2 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_NORMAL_MAP;
	}
	else if (key == GLFW_KEY_3 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_TANGENT;
	}
	else if (key == GLFW_KEY_4 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_BITANGENT;
	}
	else if (key == GLFW_KEY_5 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_ALBEDO;
	}
	else if (key == GLFW_KEY_6 && action == GLFW_PRESS) {
		game->gbufferDebugMode = GDM_POSITION;
	}
}

void Texture2D_Load(struct Texture2D *t, const char *texPath, int internalFormat,
		int format, int type)
{
	int channelsInFile = 0;
	uint8_t *data = stbi_load(UtilsFormatStr("%s/%s", RES_HOME, texPath), &t->width, &t->height,
		&channelsInFile, STBI_rgb_alpha);
	if (!data) {
		UtilsDebugPrint("ERROR: Failed to load %s", texPath);
		exit(-1);
	}

	const int loc = UtilStrFindLastChar(texPath, '/');
	t->name = strdup(texPath + loc + 1);

	UtilsDebugPrint("Loaded %s: %dx%d, channels: %d", t->name, t->width, t->height,
			channelsInFile);

	glGenTextures(1, &t->handle);
	glBindTexture(GL_TEXTURE_2D, t->handle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, t->width, t->height, 0, format, type, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	SetObjectName(OI_TEXTURE, t->handle, t->name);
	stbi_image_free(data);
}

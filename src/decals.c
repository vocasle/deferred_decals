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

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL3_IMPLEMENTATION
#define NK_KEYSTATE_BASED_INPUT
#include <nuklear/nuklear.h>
#include <nuklear/nuklear_glfw_gl3.h>

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

#if _WIN32
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

struct Transform {
	Vec3D translation;
	Vec3D rotation;
	Vec3D scale;
};

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

enum GBufferDebugMode {
	GDM_NONE,
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
	uint32_t gbufferDepthTex;
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

struct Camera {
	Mat4X4 view;
	Mat4X4 proj;
	Vec3D position;
	Vec3D right;
	Vec3D front;
};

struct Game {
	struct GBuffer gbuffer;
	struct FramebufferSize framebufferSize;
	enum GBufferDebugMode gbufferDebugMode;
	struct Texture2D *albedoTextures;
	struct Texture2D *normalTextures;
	struct Texture2D *roughnessTextures;
	uint32_t numTextures;
	struct Camera camera;
	struct nk_glfw nuklear;
};

struct File LoadShader(const char *shaderName);
int LinkProgram(const uint32_t vs, const uint32_t fs, uint32_t *pHandle);
int CompileShader(const struct File *shader, int shaderType,
		uint32_t *pHandle);
void OnFramebufferResize(GLFWwindow *window, int width, int height);

struct ModelProxy *LoadModel(const char *filename);
struct ModelProxy *CreateModelProxy(const struct Model *m);

void SetUniform(uint32_t programHandle, const char *name, uint32_t size, const void *data, enum UniformType type);

double GetDeltaTime();

uint32_t CreateTexture2D(uint32_t width, uint32_t height, int internalFormat,
		int format, int type, int attachment, int genFB, const char *imagePath);

int CreateProgram(const char *fs, const char *vs, uint32_t *programHandle, const char *programName);

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

void ProcessInput(GLFWwindow* window);

void Texture2D_Load(struct Texture2D *t, const char *texPath, int internalFormat,
		int format, int type);

void Camera_Init(struct Camera *camera, const Vec3D *position,
		float fov, float aspectRatio, float zNear, float zFar);

void Game_Update(struct Game *game);

void PushRenderPassAnnotation(const char* passName);

void PopRenderPassAnnotation(void);

void UpdateDecalTransforms(Mat4X4 *decalWorlds, Mat4X4 *decalInvWorlds,
			   const struct Transform *decalTransforms, uint32_t numDecals);

void InitNuklear(GLFWwindow *window);

void DeinitNuklear(GLFWwindow* window);

void DebugBreak()
{
#if _WIN32
	__debugbreak();
#else
		raise(SIGTRAP);
#endif
}

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
	if (source != GL_DEBUG_SOURCE_APPLICATION && severity >= GL_DEBUG_SEVERITY_LOW) {
		fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
			(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""),
			type, severity, message);
	}

	if (type == GL_DEBUG_TYPE_ERROR) {
		exit(-1);
	}
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

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    const int version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
	    return -1;
	}

    glfwSetFramebufferSizeCallback(window, OnFramebufferResize);
    struct ModelProxy *modelProxy = LoadModel("assets/room.obj");
	{
		Mat4X4 rotate90 = MathMat4X4RotateY(MathToRadians(-90.0f));
		for (uint32_t i = 0; i < modelProxy->numMeshes; ++i) {
			modelProxy->meshes[i].world = rotate90;
		}
	}

	uint32_t phongProgram = 0;
	if (!CreateProgram("shaders/frag.glsl", "shaders/vert.glsl", &phongProgram, "Phong")) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	uint32_t deferredDecal = 0;
	if (!CreateProgram("shaders/deferred_decal.glsl", "shaders/vert.glsl", &deferredDecal, "Decal")) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	uint32_t gbufferProgram = 0;
	if (!CreateProgram("shaders/gbuffer_frag.glsl", "shaders/vert.glsl", &gbufferProgram, "GBuffer")) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	uint32_t deferredProgram = 0;
	if (!CreateProgram("shaders/deferred_frag.glsl", "shaders/deferred_vert.glsl", &deferredProgram, "Deffered")) {
		UtilsFatalError("ERROR: Failed to create program\n");
		return -1;
	}

	glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(MessageCallback, 0);

	struct ModelProxy *unitCube = LoadModel("assets/unit_cube.obj");
	Mat4X4 decalWorlds[2] = { 0 };
	Mat4X4 decalInvWorlds[2] = { 0 };
	struct Transform decalTransforms[2] = { 0 };
	{
	  const Vec3D scale = { 2.0f, 2.0f, 2.0f };
	  decalTransforms[0].scale = scale;
	  decalTransforms[1].scale = scale;
	  decalTransforms[1].translation.X = 2.0f;
	  decalTransforms[1].translation.Y = 5.0f;
	  decalTransforms[1].translation.Z = -9.0f;
	  // Always orient decal so that Y faces outward of surface that decal is applied to
	  decalTransforms[1].rotation.X = 90.0f;
	  UpdateDecalTransforms(decalWorlds, decalInvWorlds,
				decalTransforms, ARRAY_COUNT(decalTransforms));
	}

	const Vec3D eyePos = { 4.633266f, 9.594514f, 6.876969f };
	const float zNear = 0.1f;
	const float zFar = 1000.0f;

	Camera_Init(&game.camera, &eyePos, MathToRadians(90.0f),
			(float)game.framebufferSize.width / (float)game.framebufferSize.height,
			zNear, zFar);

	const Vec3D g_lightPos = { 0.0, 10.0, 0.0 };

	InitGBuffer(&game.gbuffer, game.framebufferSize.width, game.framebufferSize.height);

	struct FullscreenQuadPass fsqPass = { 0 };
	InitQuadPass(&fsqPass);
	stbi_set_flip_vertically_on_load(1);
	const char *albedoTexturePaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_albedo.png",
		"assets/rusty-metal-bl/rusty-metal_albedo.png",
		"assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_COL_1K_SPECULAR.png"
	};

	const char *normalTexturesPaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_normal-ogl.png",
		"assets/rusty-metal-bl/rusty-metal_normal-ogl.png",
		"assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_NRM_1K_SPECULAR.png"
	};

	const char *roughnessTexturePaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_roughness.png",
		"assets/rusty-metal-bl/rusty-metal_roughness.png",
		"assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_GLOSS_1K_SPECULAR.png"
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
	const int32_t C_DECAL_DEPTH_TEX_LOC = 0;
	const int32_t C_DECAL_ALBEDO_TEX_LOC = 1;
	const int32_t C_DECAL_NORMAL_TEX_LOC = 2;
	const uint32_t C_DECAL_TBN_TANGENT_TEX_LOC = 3;
	const uint32_t C_DECAL_TBN_BITANGENT_TEX_LOC = 4;
	const uint32_t C_DECAL_TBN_NORMAL_TEX_LOC = 5;
	const int32_t C_WOOD_TEX_IDX = 0;
	const int32_t C_RUSTY_METAL_TEX_IDX = 1;
	const int32_t C_BRICKS_TEX_IDX = 2;
	const int32_t DECAL_TEXTURE_INDICES[2] = { C_WOOD_TEX_IDX, C_BRICKS_TEX_IDX };

#if _WIN32
	glfwMaximizeWindow(window);
#endif

	InitNuklear(window);
	struct nk_colorf bg = { 0.10f, 0.18f, 0.24f, 1.0f };

    /* Loop until the user closes the window */
    while(!glfwWindowShouldClose(window))
    {
		nk_glfw3_new_frame(&game.nuklear);
		ProcessInput(window);
		Game_Update(&game);
		// GBuffer Pass
		{
			PushRenderPassAnnotation("GBuffer Pass");
			{
				PushRenderPassAnnotation("Geometry Pass");
				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, game.gbuffer.framebuffer));
				GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
				GLCHECK(glUseProgram(gbufferProgram));

				SetUniform(gbufferProgram, "g_view", sizeof(Mat4X4), &game.camera.view, UT_MAT4);
				SetUniform(gbufferProgram, "g_proj", sizeof(Mat4X4), &game.camera.proj, UT_MAT4);
				SetUniform(gbufferProgram, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
				SetUniform(gbufferProgram, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);

				SetUniform(gbufferProgram, "g_albedoTex", sizeof(int32_t), &C_ALBEDO_TEX_LOC, UT_INT);
				SetUniform(gbufferProgram, "g_normalTex", sizeof(int32_t), &C_NORMAL_TEX_LOC, UT_INT);
				SetUniform(gbufferProgram, "g_roughnessTex", sizeof(int32_t),
					&C_ROUGHNESS_TEX_LOC, UT_INT);

				for (uint32_t i = 0; i < modelProxy->numMeshes; ++i) {
					GLCHECK(glActiveTexture(GL_TEXTURE0));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.albedoTextures[C_RUSTY_METAL_TEX_IDX].handle));
					GLCHECK(glActiveTexture(GL_TEXTURE1));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.normalTextures[C_RUSTY_METAL_TEX_IDX].handle));
					GLCHECK(glActiveTexture(GL_TEXTURE2));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.roughnessTextures[C_RUSTY_METAL_TEX_IDX].handle));

					GLCHECK(glBindVertexArray(modelProxy->meshes[i].vao));
					SetUniform(gbufferProgram, "g_world", sizeof(Mat4X4), &modelProxy->meshes[i].world, UT_MAT4);
					GLCHECK(glDrawElements(GL_TRIANGLES, modelProxy->meshes[i].numIndices, GL_UNSIGNED_INT,
						NULL));
				}
				PopRenderPassAnnotation();
			}

			// Decal pass
			{
				PushRenderPassAnnotation("Decal Pass");
				// Set read only depth
				glDepthFunc(GL_GREATER);
				glDepthMask(GL_FALSE);
				glCullFace(GL_FRONT);
				// Copy gbuffer depth
				{
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.gbufferDepthTex));
					GLCHECK(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
							game.framebufferSize.width, game.framebufferSize.height));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));
				}
				// TODO: Set depth func to GL_LESS, set depth to read only
				// TODO: Reconstruct world position from depth
				// TODO: Need to copy depth buffer
				for (uint32_t i = 0; i < unitCube->numMeshes; ++i) {
					GLCHECK(glUseProgram(deferredDecal));
					GLCHECK(glActiveTexture(GL_TEXTURE0));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.gbufferDepthTex));
					GLCHECK(glActiveTexture(GL_TEXTURE1));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.albedoTextures[C_WOOD_TEX_IDX].handle));
					GLCHECK(glActiveTexture(GL_TEXTURE2));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.normalTextures[C_WOOD_TEX_IDX].handle));
					GLCHECK(glActiveTexture(GL_TEXTURE3));

					const Mat4X4 viewProj = MathMat4X4MultMat4X4ByMat4X4(&game.camera.view, &game.camera.proj);
					const Mat4X4 invViewProj = MathMat4X4Inverse(&viewProj);

					const Vec4D rtSize = { game.framebufferSize.width, game.framebufferSize.height,
						1.0f / game.framebufferSize.width, 1.0f / game.framebufferSize.height };

					const Vec3D bboxMin = { -1.0f, -1.0f, -1.0f };
					const Vec3D bboxMax = { 1.0f, 1.0f, 1.0f };

					SetUniform(deferredDecal, "g_bboxMin", sizeof(Vec3D), &bboxMin, UT_VEC3F);
					SetUniform(deferredDecal, "g_bboxMax", sizeof(Vec3D), &bboxMax, UT_VEC3F);
					SetUniform(deferredDecal, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
					SetUniform(deferredDecal, "g_rtSize", sizeof(Vec4D), &rtSize, UT_VEC4F);
					SetUniform(deferredDecal, "g_depth", sizeof(int32_t), &C_DECAL_DEPTH_TEX_LOC, UT_INT);
					SetUniform(deferredDecal, "g_view", sizeof(Mat4X4), &game.camera.view, UT_MAT4);
					SetUniform(deferredDecal, "g_proj", sizeof(Mat4X4), &game.camera.proj, UT_MAT4);
					SetUniform(deferredDecal, "g_invViewProj", sizeof(Mat4X4), &invViewProj, UT_MAT4);
					SetUniform(deferredDecal, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
					SetUniform(deferredDecal, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);
					SetUniform(deferredDecal, "g_albedo", sizeof(int32_t), &C_DECAL_ALBEDO_TEX_LOC, UT_INT);
					SetUniform(deferredDecal, "g_normal", sizeof(int32_t), &C_DECAL_NORMAL_TEX_LOC, UT_INT);
					GLCHECK(glBindVertexArray(unitCube->meshes[i].vao));
					for (uint32_t n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
						SetUniform(deferredDecal, "g_world", sizeof(Mat4X4), &decalWorlds[n], UT_MAT4);
						SetUniform(deferredDecal, "g_decalInvWorld", sizeof(Mat4X4), &decalInvWorlds[n], UT_MAT4);
						GLCHECK(glActiveTexture(GL_TEXTURE1));
						GLCHECK(glBindTexture(GL_TEXTURE_2D, game.albedoTextures[DECAL_TEXTURE_INDICES[n]].handle));
						GLCHECK(glActiveTexture(GL_TEXTURE2));
						GLCHECK(glBindTexture(GL_TEXTURE_2D, game.normalTextures[DECAL_TEXTURE_INDICES[n]].handle));
						GLCHECK(glDrawElements(GL_TRIANGLES, unitCube->meshes[i].numIndices, GL_UNSIGNED_INT, NULL));
					}
				}
				// Reset state
				glDepthFunc(GL_LESS);
				glDepthMask(GL_TRUE);
				glCullFace(GL_BACK);
				PopRenderPassAnnotation();
			}

			GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
			PopRenderPassAnnotation();
		}

		// Deferred Shading Pass
		{
			PushRenderPassAnnotation("Deferred Shading Pass");
			GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			GLCHECK(glUseProgram(deferredProgram));
			GLCHECK(glActiveTexture(GL_TEXTURE0));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.position));
			GLCHECK(glActiveTexture(GL_TEXTURE1));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.normal));
			GLCHECK(glActiveTexture(GL_TEXTURE2));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.albedo));
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
			PopRenderPassAnnotation();
		}

		// Copy gbuffer depth to default framebuffer's depth
		{
			PushRenderPassAnnotation("Copy GBuffer Depth Pass");
			glBindFramebuffer(GL_READ_FRAMEBUFFER, game.gbuffer.framebuffer);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); // write to default framebuffer
        // blit to default framebuffer. Note that this may or may not work as the internal formats of both the FBO and default framebuffer have to match.
        // the internal formats are implementation defined. This works on all of my systems, but if it doesn't on yours you'll likely have to write to the
        // depth buffer in another shader stage (or somehow see to match the default framebuffer's internal format with the FBO's internal format).
			glBlitFramebuffer(0, 0, game.framebufferSize.width, game.framebufferSize.height,
					0, 0, game.framebufferSize.width, game.framebufferSize.height,
					GL_DEPTH_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			PopRenderPassAnnotation();
		}

		// Wireframe pass
		{
			PushRenderPassAnnotation("Wireframe Pass");
			glUseProgram(phongProgram);
			SetUniform(phongProgram, "g_view", sizeof(Mat4X4), &game.camera.view, UT_MAT4);
			SetUniform(phongProgram, "g_proj", sizeof(Mat4X4), &game.camera.proj, UT_MAT4);
			SetUniform(phongProgram, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
			SetUniform(phongProgram, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);
			static const int isWireframe = 1;
			SetUniform(phongProgram, "g_wireframe", sizeof(int), &isWireframe, UT_INT);

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			for (uint32_t i = 0; i < unitCube->numMeshes; ++i) {
				glBindVertexArray(unitCube->meshes[i].vao);
				for (uint32_t n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
					SetUniform(phongProgram, "g_world", sizeof(Mat4X4), &decalWorlds[n], UT_MAT4);
					glDrawElements(GL_TRIANGLES, unitCube->meshes[i].numIndices, GL_UNSIGNED_INT, NULL);
				}
			}
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			PopRenderPassAnnotation();
		}

		// GUI Pass
		{
			PushRenderPassAnnotation("Nuklear Pass");
			struct nk_context* ctx = &game.nuklear.ctx;
			if (nk_begin(ctx, "Options", nk_rect(50, 50, 530, 250),
				NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
				NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
			{
				nk_layout_row_dynamic(ctx, 30, 1);
				for (uint32_t i = 0; i < ARRAY_COUNT(decalTransforms); ++i) {
				  nk_label(ctx, UtilsFormatStr("Decal %u:", i), NK_TEXT_ALIGN_LEFT);
				  nk_layout_row_dynamic(ctx, 30, 4);
				  nk_label(ctx, "Translation:", NK_TEXT_ALIGN_LEFT);
				  nk_property_float(ctx, "#X", -10.0f, &decalTransforms[i].translation.X, 10.0f, 0.1f, 0.0f);
				  nk_property_float(ctx, "#Y", -10.0f, &decalTransforms[i].translation.Y, 10.0f, 0.1f, 0.0f);
				  nk_property_float(ctx, "#Z", -10.0f, &decalTransforms[i].translation.Z, 10.0f, 0.1f, 0.0f);
				  nk_label(ctx, "Rotation:", NK_TEXT_ALIGN_LEFT);
				  nk_property_float(ctx, "#Pitch", -89.0f, &decalTransforms[i].rotation.X, 89.0f, 1.0f, 0.0f);
				  nk_property_float(ctx, "#Yaw", -180.0f, &decalTransforms[i].rotation.Y, 180.0f, 1.0f, 0.0f);
				  nk_property_float(ctx, "#Roll", -89.0f, &decalTransforms[i].rotation.Z, 89.0f, 1.0f, 0.0f);
				  nk_label(ctx, "Scale:", NK_TEXT_ALIGN_LEFT);
				  nk_property_float(ctx, "#X", 1.0f, &decalTransforms[i].scale.X, 10.0f, 0.5f, 0.0f);
				  nk_property_float(ctx, "#Y", 1.0f, &decalTransforms[i].scale.Y, 10.0f, 0.5f, 0.0f);
				  nk_property_float(ctx, "#Z", 1.0f, &decalTransforms[i].scale.Z, 10.0f, 0.5f, 0.0f);
				}
				if (nk_button_label(ctx, "Apply Transform")) {
				  UpdateDecalTransforms(decalWorlds, decalInvWorlds,
							decalTransforms, ARRAY_COUNT(decalTransforms));
				}
				nk_layout_row_dynamic(ctx, 25, 1);
			}
			nk_end(ctx);

			nk_glfw3_render(&game.nuklear, NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
			glDisable(GL_BLEND);
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDisable(GL_SCISSOR_TEST);
			PopRenderPassAnnotation();
		}

        /* Swap front and back buffers */
        glfwSwapBuffers(window);

        /* Poll for and process events */
        glfwPollEvents();
    }

	DeinitNuklear(window);
    glfwTerminate();
    return 0;
}

int CreateProgram(const char *fs, const char *vs, uint32_t *programHandle, const char *programName)
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

	SetObjectName(OI_FRAGMENT_SHADER, fragHandle, UtilsGetStrAfterChar(fs, '/'));
	SetObjectName(OI_VERTEX_SHADER, vertHandle, UtilsGetStrAfterChar(vs, '/'));
	SetObjectName(OI_PROGRAM, *programHandle, programName);
	UtilsDebugPrint("Linked program %u (%s)", *programHandle, programName);
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

struct ModelProxy *LoadModel(const char *filename)
{
	const char *absPath = UtilsFormatStr("%s/%s", RES_HOME, filename);
	struct Model* model = OLLoad(absPath);
	struct ModelProxy *proxy = NULL;
    if (model) {
	    for (uint32_t i = 0; i < model->NumMeshes; ++i) {
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

	    SetObjectName(OI_VERTEX_ARRAY, ret->meshes[i].vao, m->Meshes[i].Name);
	    SetObjectName(OI_VERTEX_BUFFER, ret->meshes[i].vbo, m->Meshes[i].Name);
	    SetObjectName(OI_INDEX_BUFFER, ret->meshes[i].ebo, m->Meshes[i].Name);
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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
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

	const uint32_t attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2 };
	GLCHECK(glDrawBuffers(ARRAY_COUNT(attachments), attachments));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		UtilsDebugPrint("ERROR: Failed to create GBuffer framebuffer");
		return 0;
	}
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	gbuffer->gbufferDepthTex = CreateTexture2D(fbWidth,
			fbHeight, GL_DEPTH_COMPONENT,
			GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0, 0, NULL);

	SetObjectName(OI_TEXTURE, gbuffer->albedo, "GBuffer.Albedo");
	SetObjectName(OI_TEXTURE, gbuffer->normal, "GBuffer.Normal");
	SetObjectName(OI_TEXTURE, gbuffer->position, "GBuffer.Position");

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

	const char *fullLabel = UtilsFormatStr("%s_%s", label, prefix);
	GLCHECK(glObjectLabel(identifier, name, strlen(fullLabel), fullLabel));
}

int IsKeyPressed(GLFWwindow *window, int key)
{
	const int state = glfwGetKey(window, key);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

void ProcessInput(GLFWwindow* window)
{
	struct Game *game = glfwGetWindowUserPointer(window);
	if (IsKeyPressed(window, GLFW_KEY_ESCAPE)) {
		glfwSetWindowShouldClose(window, 1);
	}
	else if (IsKeyPressed(window, GLFW_KEY_R)) {
		const Vec3D offset = { 0.0f, 1.0f, 0.0f };
		game->camera.position = MathVec3DAddition(&game->camera.position, &offset);
	}
	else if (IsKeyPressed(window, GLFW_KEY_F)) {
		const Vec3D offset = { 0.0f, -1.0f, 0.0f };
		game->camera.position = MathVec3DAddition(&game->camera.position, &offset);
	}
	else if (IsKeyPressed(window, GLFW_KEY_W)) {
		game->camera.position = MathVec3DAddition(&game->camera.position, &game->camera.front);
	}
	else if (IsKeyPressed(window, GLFW_KEY_S)) {
		game->camera.position = MathVec3DSubtraction(&game->camera.position, &game->camera.front);
	}
	else if (IsKeyPressed(window, GLFW_KEY_A)) {
		game->camera.position = MathVec3DSubtraction(&game->camera.position, &game->camera.right);
	}
	else if (IsKeyPressed(window, GLFW_KEY_D)) {
		game->camera.position = MathVec3DAddition(&game->camera.position, &game->camera.right);
	}
	else if (IsKeyPressed(window, GLFW_KEY_LEFT)) {
		const Mat4X4 rotation = MathMat4X4RotateY(MathToRadians(1.0f));
		Vec4D tmp = { game->camera.front.X, game->camera.front.Y,
			game->camera.front.Z, 0.0f };
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &rotation);
		game->camera.front.X = tmp.X;
		game->camera.front.Y = tmp.Y;
		game->camera.front.Z = tmp.Z;
		const Vec3D up = { .Y = 1.0f };
		game->camera.right = MathVec3DCross(&up, &game->camera.front);
	}
	else if (IsKeyPressed(window, GLFW_KEY_RIGHT)) {
		const Mat4X4 rotation = MathMat4X4RotateY(MathToRadians(-1.0f));
		Vec4D tmp = { game->camera.front.X, game->camera.front.Y,
			game->camera.front.Z, 0.0f };
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &rotation);
		game->camera.front.X = tmp.X;
		game->camera.front.Y = tmp.Y;
		game->camera.front.Z = tmp.Z;
		const Vec3D up = { .Y = 1.0f };
		game->camera.right = MathVec3DCross(&up, &game->camera.front);
	}
	// TODO: Fix pitch, currently front follows circle, if W is pressed continuously
	else if (IsKeyPressed(window, GLFW_KEY_UP)) {
		const Mat4X4 rotation = MathMat4X4RotateX(MathToRadians(1.0f));
		Vec4D tmp = { game->camera.front.X, game->camera.front.Y,
			game->camera.front.Z, 0.0f };
		const Mat4X4 invView = MathMat4X4Inverse(&game->camera.view);
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &game->camera.view);
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &rotation);
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &invView);
		game->camera.front.X = tmp.X;
		game->camera.front.Y = tmp.Y;
		game->camera.front.Z = tmp.Z;
		const Vec3D up = { .Y = 1.0f };
		game->camera.right = MathVec3DCross(&up, &game->camera.front);
	}
	else if (IsKeyPressed(window, GLFW_KEY_DOWN)) {
		const Mat4X4 invView = MathMat4X4Inverse(&game->camera.view);
		const Mat4X4 rotation = MathMat4X4RotateX(MathToRadians(-1.0f));
		Vec4D tmp = { game->camera.front.X, game->camera.front.Y,
			game->camera.front.Z, 0.0f };

		// Move front vector from world space to camera
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &game->camera.view);
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &rotation);
		tmp = MathMat4X4MultVec4DByMat4X4(&tmp, &invView);

		game->camera.front.X = tmp.X;
		game->camera.front.Y = tmp.Y;
		game->camera.front.Z = tmp.Z;
		const Vec3D up = { .Y = 1.0f };
		game->camera.right = MathVec3DCross(&up, &game->camera.front);
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

	const int loc = UtilsStrFindLastChar(texPath, '/');
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

void Camera_Init(struct Camera *camera, const Vec3D *position,
		float fov, float aspectRatio, float zNear, float zFar)
{
	const Vec3D front = { -0.390251f, -0.463592f, -0.795480f };
	const Vec3D up = { 0.0f, 1.0f, 0.0f };
	const Vec3D right = MathVec3DCross(&up, &front);
	camera->position = *position;
	camera->front = front;
	camera->right = right;
	camera->proj = MathMat4X4PerspectiveFov(fov, aspectRatio, zNear, zFar);
	const Vec3D focusPos = MathVec3DAddition(&camera->position, &front);
	camera->view = MathMat4X4ViewAt(&camera->position, &focusPos, &up);
}

void Game_Update(struct Game *game)
{
	const double dt = GetDeltaTime();
	const Vec3D focusPos = MathVec3DAddition(&game->camera.position, &game->camera.front);
	const Vec3D up = { 0.0f, 1.0f, 0.0f };
	game->camera.view = MathMat4X4ViewAt(&game->camera.position, &focusPos, &up);
}

void PushRenderPassAnnotation(const char* passName)
{
	glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, strlen(passName), passName);
}

void PopRenderPassAnnotation(void)
{
	glPopDebugGroup();
}

Mat4X4 TransformToMat4X4(const struct Transform *t)
{
	const Mat4X4 translation = MathMat4X4TranslateFromVec3D(&t->translation);
	const Mat4X4 rotation = MathMat4X4RotateFromVec3D(&t->rotation);
	const Mat4X4 scale = MathMat4X4ScaleFromVec3D(&t->scale);
	Mat4X4 ret = MathMat4X4MultMat4X4ByMat4X4(&translation, &rotation);
	ret = MathMat4X4MultMat4X4ByMat4X4(&ret, &scale);
	return ret;
}

void UpdateDecalTransforms(Mat4X4 *decalWorlds, Mat4X4 *decalInvWorlds,
			   const struct Transform *decalTransforms, uint32_t numDecals)
{
  for (uint32_t i = 0; i < numDecals; ++i) {
    const Mat4X4 translation = MathMat4X4TranslateFromVec3D(&decalTransforms[i].translation);
    const Vec3D angles = {MathToRadians(decalTransforms[i].rotation.X),
      MathToRadians(decalTransforms[i].rotation.Y),
      MathToRadians(decalTransforms[i].rotation.Z)};
    const Mat4X4 rotation = MathMat4X4RotateFromVec3D(&angles);
    const Mat4X4 scale = MathMat4X4ScaleFromVec3D(&decalTransforms[i].scale);
    Mat4X4 world = MathMat4X4MultMat4X4ByMat4X4(&scale, &rotation);
    world = MathMat4X4MultMat4X4ByMat4X4(&world, &translation);
    decalWorlds[i] = world;
    decalInvWorlds[i] = MathMat4X4Inverse(&decalWorlds[i]);
  }
}

struct nk_glfw *GetNuklearGLFW(GLFWwindow *w)
{
  struct Game *game = glfwGetWindowUserPointer(w);
  return &game->nuklear;
}

void InitNuklear(GLFWwindow *window)
{
	struct Game* game = glfwGetWindowUserPointer(window);
	nk_glfw3_init(&game->nuklear, window, NK_GLFW3_INSTALL_CALLBACKS);
	{
		struct nk_font_atlas* atlas = NULL;
		nk_glfw3_font_stash_begin(&game->nuklear, &atlas);

		struct nk_font *droid = nk_font_atlas_add_from_file(atlas,
			UtilsFormatStr("%s/%s", RES_HOME, "fonts/DroidSans.ttf"), 22, 0);
		nk_glfw3_font_stash_end(&game->nuklear);
		nk_style_load_all_cursors(&game->nuklear.ctx, atlas->cursors);
		nk_style_set_font(&game->nuklear.ctx, &droid->handle);
	}

}

void DeinitNuklear(GLFWwindow* window)
{
	struct Game* game = glfwGetWindowUserPointer(window);
	nk_glfw3_shutdown(&game->nuklear);
}

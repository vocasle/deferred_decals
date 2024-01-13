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
#include "defines.h"
#include "renderer.h"

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
	__declspec(dllexport) i32 AmdPowerXpressRequestHighPerformance = 1;
#endif

struct Transform {
	Vec3D translation;
	Vec3D rotation;
	Vec3D scale;
};

enum GBufferDebugMode {
	GDM_NONE,
	GDM_NORMAL_MAP,
	GDM_ALBEDO,
	GDM_POSITION,
};

struct GBuffer {
	u32 position;
	u32 normal;
	u32 albedo;
	u32 framebuffer;
	u32 depthBuffer;
	u32 gbufferDepthTex;
};

struct FullscreenQuadPass {
	u32 vbo;
	u32 vao;
};

struct FramebufferSize {
	i32 width;
	i32 height;
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
	u32 numTextures;
	struct Camera camera;
	struct nk_glfw nuklear;
	struct Material **materials;
	u32 numMaterials;
};

void OnFramebufferResize(GLFWwindow *window, i32 width, i32 height);

struct ModelProxy *LoadModel(const i8 *filename);
struct ModelProxy *CreateModelProxy(const struct Model *m);

f64 GetDeltaTime();

u32 CreateTexture2D(u32 width, u32 height, i32 internalFormat,
		i32 format, i32 type, i32 attachment, i32 genFB, const i8 *imagePath);

void RenderQuad(const struct FullscreenQuadPass *fsqPass);

void InitQuadPass(struct FullscreenQuadPass *fsqPass);

struct CalculateTangetData {
	struct Vertex *vertices;
	u32 numVertices;
	const u32 *indices;
	u32 numIndices;
};
void CalculateTangentArray(struct CalculateTangetData *data);

void ProcessInput(GLFWwindow* window);

void Texture2D_Load(struct Texture2D *t, const i8 *texPath, i32 internalFormat,
		i32 format, i32 type);

void Camera_Init(struct Camera *camera, const Vec3D *position,
		f32 fov, f32 aspectRatio, f32 zNear, f32 zFar);

void Game_Update(struct Game *game);

void PushRenderPassAnnotation(const i8* passName);

void PopRenderPassAnnotation(void);

void UpdateDecalTransforms(Mat4X4 *decalWorlds, Mat4X4 *decalInvWorlds,
			   const struct Transform *decalTransforms, u32 numDecals);

void InitNuklear(GLFWwindow *window);

void DeinitNuklear(GLFWwindow* window);

void LoadMaterials(struct Game *game);

#define ARRAY_COUNT(x) (u32)(sizeof(x) / sizeof(x[0]))

void GLAPIENTRY
MessageCallback(GLenum source,
                GLenum type,
                GLuint id,
                GLenum severity,
                GLsizei length,
                const GLchar* message,
                const void* userParam);

i32 InitGBuffer(struct GBuffer *gbuffer, const i32 fbWidth, const i32 fbHeight);

GLFWwindow *InitGLFW(i32 width, i32 height, const i8 *title);

struct Material *Game_FindMaterialByName(struct Game *game, const i8 *name);

i32 main(void)
{
    GLFWwindow* window = InitGLFW(640, 480, "Deferred Decals");

	struct Game game = { 0 };
	glfwGetFramebufferSize(window, &game.framebufferSize.width,
			&game.framebufferSize.height);
	glfwSetWindowUserPointer(window, &game);
    glfwSetFramebufferSizeCallback(window, OnFramebufferResize);

    struct ModelProxy *room = LoadModel("assets/room.obj");
	{
		Mat4X4 rotate90 = MathMat4X4RotateY(MathToRadians(-90.0f));
		for (u32 i = 0; i < room->numMeshes; ++i) {
			room->meshes[i].world = rotate90;
		}
	}

	LoadMaterials(&game);

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
	const f32 zNear = 0.1f;
	const f32 zFar = 1000.0f;

	Camera_Init(&game.camera, &eyePos, MathToRadians(90.0f),
			(f32)game.framebufferSize.width / (f32)game.framebufferSize.height,
			zNear, zFar);

	const Vec3D g_lightPos = { 0.0, 10.0, 0.0 };

	InitGBuffer(&game.gbuffer, game.framebufferSize.width, game.framebufferSize.height);

	struct FullscreenQuadPass fsqPass = { 0 };
	InitQuadPass(&fsqPass);
	stbi_set_flip_vertically_on_load(1);
	const i8 *albedoTexturePaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_albedo.png",
		"assets/rusty-metal-bl/rusty-metal_albedo.png",
		"assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_COL_1K_SPECULAR.png"
	};

	const i8 *normalTexturesPaths[] = {
		"assets/older-wood-flooring-bl/older-wood-flooring_normal-ogl.png",
		"assets/rusty-metal-bl/rusty-metal_normal-ogl.png",
		"assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_NRM_1K_SPECULAR.png"
	};

	const i8 *roughnessTexturePaths[] = {
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

	for (u32 i = 0; i < ARRAY_COUNT(albedoTexturePaths); ++i) {
		Texture2D_Load(game.albedoTextures + i, albedoTexturePaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
		Texture2D_Load(game.normalTextures + i, normalTexturesPaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
		Texture2D_Load(game.roughnessTextures + i, roughnessTexturePaths[i], GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
	}

	const i32 C_ALBEDO_TEX_LOC = 0;
	const i32 C_NORMAL_TEX_LOC = 1;
	const i32 C_ROUGHNESS_TEX_LOC = 2;
	const i32 C_DECAL_DEPTH_TEX_LOC = 0;
	const i32 C_DECAL_ALBEDO_TEX_LOC = 1;
	const i32 C_DECAL_NORMAL_TEX_LOC = 2;
	const u32 C_DECAL_TBN_TANGENT_TEX_LOC = 3;
	const u32 C_DECAL_TBN_BITANGENT_TEX_LOC = 4;
	const u32 C_DECAL_TBN_NORMAL_TEX_LOC = 5;
	const i32 C_WOOD_TEX_IDX = 0;
	const i32 C_RUSTY_METAL_TEX_IDX = 1;
	const i32 C_BRICKS_TEX_IDX = 2;
	const i32 DECAL_TEXTURE_INDICES[2] = { C_WOOD_TEX_IDX, C_BRICKS_TEX_IDX };

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
				struct Material *m = Game_FindMaterialByName(&game, "GBuffer");
				GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER, game.gbuffer.framebuffer));
				GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
				GLCHECK(glUseProgram(Material_GetHandle(m)));

				Material_SetUniform(m, "g_view", sizeof(Mat4X4), &game.camera.view,
					UT_MAT4);
				Material_SetUniform(m, "g_proj", sizeof(Mat4X4), &game.camera.proj,
					UT_MAT4);
				Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
				Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);

				for (u32 i = 0; i < room->numMeshes; ++i) {
					Material_SetTexture(m, "g_albedoTex", &game.albedoTextures[C_RUSTY_METAL_TEX_IDX]);
					Material_SetTexture(m, "g_normalTex", &game.normalTextures[C_RUSTY_METAL_TEX_IDX]);
					Material_SetTexture(m, "g_roughnessTex", 
						&game.roughnessTextures[C_RUSTY_METAL_TEX_IDX]);

					GLCHECK(glBindVertexArray(room->meshes[i].vao));
					Material_SetUniform(m, "g_world", sizeof(Mat4X4), 
						&room->meshes[i].world, UT_MAT4);
					GLCHECK(glDrawElements(GL_TRIANGLES, room->meshes[i].numIndices, 
						GL_UNSIGNED_INT, NULL));
				}
				PopRenderPassAnnotation();
			}

			// Decal pass
			{
				PushRenderPassAnnotation("Decal Pass");
				struct Material *m = Game_FindMaterialByName(&game, "Decal");
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
				for (u32 i = 0; i < unitCube->numMeshes; ++i) {
					GLCHECK(glUseProgram(Material_GetHandle(m)));
					// TODO: Replace with SetTexture 
					// Material_SetTexture(m, "g_depth", 
					// 	&game.gbuffer.gbufferDepthTex);
					Material_SetUniform(m, "g_depth", sizeof(i32),
						&C_DECAL_DEPTH_TEX_LOC, UT_INT);
					GLCHECK(glActiveTexture(GL_TEXTURE0));
					GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.gbufferDepthTex));									

					const Mat4X4 viewProj = MathMat4X4MultMat4X4ByMat4X4(
						&game.camera.view, &game.camera.proj);
					const Mat4X4 invViewProj = MathMat4X4Inverse(&viewProj);

					const Vec4D rtSize = { game.framebufferSize.width, 
						game.framebufferSize.height,
						1.0f / game.framebufferSize.width, 
						1.0f / game.framebufferSize.height };

					Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos,
						UT_VEC3F);
					Material_SetUniform(m, "g_rtSize", sizeof(Vec4D), &rtSize, UT_VEC4F);
					Material_SetUniform(m, "g_view", sizeof(Mat4X4), &game.camera.view,
						UT_MAT4);
					Material_SetUniform(m, "g_proj", sizeof(Mat4X4), &game.camera.proj, 
						UT_MAT4);
					Material_SetUniform(m, "g_invViewProj", sizeof(Mat4X4), &invViewProj, 
						UT_MAT4);
					Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos, 
						UT_VEC3F);
					Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D), &eyePos, 
						UT_VEC3F);
					GLCHECK(glBindVertexArray(unitCube->meshes[i].vao));
					for (u32 n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
						Material_SetUniform(m, "g_world", sizeof(Mat4X4), 
							&decalWorlds[n], UT_MAT4);
						Material_SetUniform(m, "g_decalInvWorld", sizeof(Mat4X4), 
							&decalInvWorlds[n], UT_MAT4);
						Material_SetTexture(m, "g_albedo", 
							&game.albedoTextures[DECAL_TEXTURE_INDICES[n]]);
						Material_SetTexture(m, "g_normal", 
							&game.normalTextures[DECAL_TEXTURE_INDICES[n]]);
						GLCHECK(glDrawElements(GL_TRIANGLES,
                            unitCube->meshes[i].numIndices, GL_UNSIGNED_INT, NULL));
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
			struct Material *m = Game_FindMaterialByName(&game, "Deferred");
			GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
			GLCHECK(glUseProgram(Material_GetHandle(m)));
			GLCHECK(glActiveTexture(GL_TEXTURE0));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.position));
			GLCHECK(glActiveTexture(GL_TEXTURE1));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.normal));
			GLCHECK(glActiveTexture(GL_TEXTURE2));
			GLCHECK(glBindTexture(GL_TEXTURE_2D, game.gbuffer.albedo));
			Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
			Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);
			Material_SetUniform(m, "g_gbufferDebugMode", sizeof(i32),
					&game.gbufferDebugMode, UT_INT);

			const u32 g_position = 0;
			const u32 g_normal = 1;
			const u32 g_albedo = 2;
			Material_SetUniform(m, "g_position", sizeof(u32), &g_position, UT_INT);
			Material_SetUniform(m, "g_normal", sizeof(u32), &g_normal, UT_INT);
			Material_SetUniform(m, "g_albedo", sizeof(u32), &g_albedo, UT_INT);
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
			struct Material *m = Game_FindMaterialByName(&game, "Phong");
			glUseProgram(Material_GetHandle(m));
			Material_SetUniform(m, "g_view", sizeof(Mat4X4), &game.camera.view, UT_MAT4);
			Material_SetUniform(m, "g_proj", sizeof(Mat4X4), &game.camera.proj, UT_MAT4);
			Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos, UT_VEC3F);
			Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D), &eyePos, UT_VEC3F);
			static const i32 isWireframe = 1;
			Material_SetUniform(m, "g_wireframe", sizeof(i32), &isWireframe, UT_INT);

			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			for (u32 i = 0; i < unitCube->numMeshes; ++i) {
				glBindVertexArray(unitCube->meshes[i].vao);
				for (u32 n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
					Material_SetUniform(m, "g_world", sizeof(Mat4X4), &decalWorlds[n],
						UT_MAT4);
					glDrawElements(GL_TRIANGLES, unitCube->meshes[i].numIndices, 
						GL_UNSIGNED_INT, NULL);
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
				for (u32 i = 0; i < ARRAY_COUNT(decalTransforms); ++i) {
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


void OnFramebufferResize(GLFWwindow *window, i32 width,
		i32 height)
{
	GLCHECK(glViewport(0, 0, width, height));
	struct Game *game = glfwGetWindowUserPointer(window);
	game->framebufferSize.width = width;
	game->framebufferSize.height = height;
	InitGBuffer(&game->gbuffer, width, height);
}

struct ModelProxy *LoadModel(const i8 *filename)
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

void PrintModelToFile(const struct Model *m)
{
	if (!m) {
		return;
	}

	FILE *out = fopen("model.txt", "w");
	for (u32 i = 0; i < m->NumMeshes; ++i) {
		fprintf(out, "%s\n", m->Meshes[i].Name);
		fprintf(out, "Num positions: %d, num indices: %d\n", m->Meshes[i].NumPositions,
				m->Meshes[i].NumFaces);
		for (u32 j = 0; j < m->Meshes[i].NumFaces; j += 3) {
			fprintf(out, "%d %d %d\n", m->Meshes[i].Faces[j].posIdx,
					m->Meshes[i].Faces[j + 1].posIdx,
					m->Meshes[i].Faces[j + 2].posIdx);
		}
		for (u32 j = 0; j < m->Meshes[i].NumPositions; ++j) {
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
	for (u32 i = 0; i < m->numMeshes; ++i) {
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

		free(indices);

	    SetObjectName(OI_VERTEX_ARRAY, ret->meshes[i].vao, m->Meshes[i].Name);
	    SetObjectName(OI_VERTEX_BUFFER, ret->meshes[i].vbo, m->Meshes[i].Name);
	    SetObjectName(OI_INDEX_BUFFER, ret->meshes[i].ebo, m->Meshes[i].Name);
	}

//	ValidateModelProxy(ret);

	return ret;
}

f64 GetDeltaTime()
{
	static f64 prevTime = 0.0;
	const f64 now = glfwGetTime();
	const f64 dt = now - prevTime;
	prevTime = now;
	return dt;
}

u32 CreateTexture2D(u32 width, u32 height, i32 internalFormat,
		i32 format, i32 type, i32 attachment, i32 genFB, const i8 *imagePath)
{
	u8 *data = NULL;
	if (imagePath)
	{
		i32 w = 0;
		i32 h = 0;
		i32 channelsInFile = 0;
		data = stbi_load(UtilsFormatStr("%s/%s", RES_HOME, imagePath), &w, &h,
			&channelsInFile, STBI_rgb_alpha);
		if (!data) {
			UtilsFatalError("FATAL ERROR: Failed to load %s", imagePath);
		}
		width = w;
		height = h;
	}

	u32 handle = 0;
	GLCHECK(glGenTextures(1, &handle));
	GLCHECK(glBindTexture(GL_TEXTURE_2D, handle));
	GLCHECK(glTexImage2D(GL_TEXTURE_2D, 0, internalFormat,
		width, height, 0, format, type, data));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GLCHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	if (genFB) {
		GLCHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
			handle, 0));
	}
	if (data) {
		stbi_image_free(data);
	}
	return handle;
}

void InitQuadPass(struct FullscreenQuadPass *fsqPass)
{
	const f32 quadVertices[] = {
		// positions        // texture Coords
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};
	// setup plane VAO
	GLCHECK(glGenVertexArrays(1, &fsqPass->vao));
	GLCHECK(glGenBuffers(1, &fsqPass->vbo));
	GLCHECK(glBindVertexArray(fsqPass->vao));
	GLCHECK(glBindBuffer(GL_ARRAY_BUFFER, fsqPass->vbo));
	GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW));
	GLCHECK(glEnableVertexAttribArray(0));
	GLCHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)0));
	GLCHECK(glEnableVertexAttribArray(1));
	GLCHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32), (void*)(3 * sizeof(f32))));
}

void RenderQuad(const struct FullscreenQuadPass *fsqPass)
{
    GLCHECK(glBindVertexArray(fsqPass->vao));
    GLCHECK(glDrawArrays(GL_TRIANGLE_STRIP, 0, 4));
    GLCHECK(glBindVertexArray(0));
}

i32 InitGBuffer(struct GBuffer *gbuffer, const i32 fbWidth, const i32 fbHeight)
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

	const u32 attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
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

void GetNormal(const SMikkTSpaceContext * pContext, f32 fvNormOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvNormOut[0] = data->vertices[iFace * 3 + iVert].normal.X;
	fvNormOut[1] = data->vertices[iFace * 3 + iVert].normal.Y;
	fvNormOut[2] = data->vertices[iFace * 3 + iVert].normal.Z;
}

void GetPosition(const SMikkTSpaceContext * pContext, f32 fvPosOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvPosOut[0] = data->vertices[iFace * 3 + iVert].position.X;
	fvPosOut[1] = data->vertices[iFace * 3 + iVert].position.Y;
	fvPosOut[2] = data->vertices[iFace * 3 + iVert].position.Z;
}

void GetTexCoords(const SMikkTSpaceContext * pContext, f32 fvTexcOut[], const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	fvTexcOut[0] = data->vertices[iFace * 3 + iVert].texCoords.X;
	fvTexcOut[1] = data->vertices[iFace * 3 + iVert].texCoords.Y;
}


i32 GetNumVerticesOfFace(const SMikkTSpaceContext * pContext, const i32 iFace)
{
	return 3;
}

void SetTSpaceBasic(const SMikkTSpaceContext * pContext, const f32 fvTangent[],
		const f32 fSign, const i32 iFace, const i32 iVert)
{
	struct CalculateTangetData *data = pContext->m_pUserData;
	data->vertices[iFace * 3 + iVert].tangent.X = fvTangent[0];
	data->vertices[iFace * 3 + iVert].tangent.Y = fvTangent[1];
	data->vertices[iFace * 3 + iVert].tangent.Z = fvTangent[2];
	data->vertices[iFace * 3 + iVert].tangent.W = fSign;
}

i32 GetNumFaces(const SMikkTSpaceContext * pContext)
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

i32 IsKeyPressed(GLFWwindow *window, i32 key)
{
	const i32 state = glfwGetKey(window, key);
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

void Texture2D_Load(struct Texture2D *t, const i8 *texPath, i32 internalFormat,
		i32 format, i32 type)
{
	i32 channelsInFile = 0;
	u8 *data = stbi_load(UtilsFormatStr("%s/%s", RES_HOME, texPath), &t->width, &t->height,
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

void Camera_Init(struct Camera *camera, const Vec3D *position,
		f32 fov, f32 aspectRatio, f32 zNear, f32 zFar)
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
	const f64 dt = GetDeltaTime();
	const Vec3D focusPos = MathVec3DAddition(&game->camera.position, &game->camera.front);
	const Vec3D up = { 0.0f, 1.0f, 0.0f };
	game->camera.view = MathMat4X4ViewAt(&game->camera.position, &focusPos, &up);
}

void PushRenderPassAnnotation(const i8* passName)
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
			   const struct Transform *decalTransforms, u32 numDecals)
{
  for (u32 i = 0; i < numDecals; ++i) {
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

GLFWwindow *InitGLFW(i32 width, i32 height, const i8 *title)
{
    if (!glfwInit()) {
		UtilsFatalError("FATAL ERROR: Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        glfwTerminate();
        UtilsFatalError("FATAL ERROR: Failed to create window");
    }

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    const i32 version = gladLoadGL(glfwGetProcAddress);
    if (version == 0) {
	    UtilsFatalError("FATAL ERROR: Failed to load OpenGL");
	}

	return window;
}

void LoadMaterials(struct Game *game)
{
	const struct MaterialCreateInfo materialCreateInfos[] = {
		{"shaders/vert.glsl", "shaders/frag.glsl", "Phong"},
		{"shaders/vert.glsl", "shaders/deferred_decal.glsl", "Decal",  
			{"g_depth","g_albedo", "g_normal"}, 3
		},
		{"shaders/vert.glsl", "shaders/gbuffer_frag.glsl", "GBuffer",
			{"g_albedoTex", "g_normalTex", "g_roughnessTex"}, 3
		},
		{"shaders/deferred_vert.glsl", "shaders/deferred_frag.glsl", "Deferred",
			{"g_position", "g_normal", "g_albedo"}, 3
		}
	};

	game->materials = malloc(sizeof(struct Material*) * ARRAY_COUNT(materialCreateInfos));
	game->numMaterials = ARRAY_COUNT(materialCreateInfos);

	for (u32 i = 0; i < game->numMaterials; ++i) {
		game->materials[i] = Material_Create(&materialCreateInfos[i]);
	}
}

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

struct Material *Game_FindMaterialByName(struct Game *game, const i8 *name)
{
	for (u32 i = 0; i < game->numMaterials; ++i) {
		if (strcmp(name, Material_GetName(game->materials[i])) == 0) {
			return game->materials[i];
		}
	}
	UtilsDebugPrint("WARN: Failed to find material with name %s", name);
	return NULL;
}
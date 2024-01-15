#include <glad/gl.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "myutils.h"
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

#if _WIN32 // Force descrete GPU on Windows
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
    struct Texture2D positionTex;
    struct Texture2D normalTex;
    struct Texture2D albedoTex;
    struct Texture2D depthTex;
    u32 framebuffer;
    u32 depthRenderBuffer;
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
    struct ModelProxy **models;
    u32 numModels;
    GLFWwindow *window;
};

struct Game *Game_Create();

void OnFramebufferResize(GLFWwindow *window, i32 width, i32 height);

f64 GetDeltaTime();

void RenderQuad(const struct FullscreenQuadPass *fsqPass);

void InitQuadPass(struct FullscreenQuadPass *fsqPass);

void ProcessInput(GLFWwindow* window);

void Camera_Init(struct Camera *camera, const Vec3D *position,
        f32 fov, f32 aspectRatio, f32 zNear, f32 zFar);

void Game_Update(struct Game *game);

void PushRenderPassAnnotation(const i8* passName);

void PopRenderPassAnnotation(void);

void UpdateDecalTransforms(Mat4X4 *decalWorlds, Mat4X4 *decalInvWorlds,
               const struct Transform *decalTransforms, u32 numDecals);

void InitNuklear(GLFWwindow *window);

void DeinitNuklear(GLFWwindow* window);

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length, const GLchar* message,
    const void* userParam);

i32 InitGBuffer(struct GBuffer *gbuffer, const i32 fbWidth,
    const i32 fbHeight);

struct Material *Game_FindMaterialByName(struct Game *game, const i8 *name);

i32 main(void)
{
    struct Game *game = Game_Create();

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glFrontFace(GL_CW);
    glCullFace(GL_BACK);
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(MessageCallback, 0);

    Mat4X4 decalWorlds[2] = { 0 };
    Mat4X4 decalInvWorlds[2] = { 0 };
    const struct Transform decalTransforms[] = {
        {.scale = {2.0f, 2.0f, 2.0f}},
        {
            .scale = {2.0f, 2.0f, 2.0f},
            .translation = {2.0f, 5.0f, -9.0f},
            .rotation.X = 90.0f
        }
    };
    UpdateDecalTransforms(decalWorlds, decalInvWorlds,
        decalTransforms, ARRAY_COUNT(decalTransforms));

    const Vec3D eyePos = { 4.633266f, 9.594514f, 6.876969f };
    const f32 zNear = 0.1f;
    const f32 zFar = 1000.0f;
    Camera_Init(&game->camera, &eyePos, MathToRadians(90.0f),
        (f32)game->framebufferSize.width / (f32)game->framebufferSize.height,
        zNear, zFar);

    const Vec3D g_lightPos = { 0.0, 10.0, 0.0 };

    InitGBuffer(&game->gbuffer, game->framebufferSize.width,
        game->framebufferSize.height);

    struct FullscreenQuadPass fsqPass = { 0 };
    InitQuadPass(&fsqPass);

    const i32 C_WOOD_TEX_IDX = 0;
    const i32 C_RUSTY_METAL_TEX_IDX = 1;
    const i32 C_BRICKS_TEX_IDX = 2;
    const i32 C_DEFAULT_TEX_IDX = 3;
    const i32 DECAL_TEXTURE_INDICES[2] = { C_WOOD_TEX_IDX, C_BRICKS_TEX_IDX };

#if _WIN32 // On Windows GLFW window won't start maximazed. We force it.
    glfwMaximizeWindow(game->window);
#endif

    InitNuklear(game->window);

    /* Loop until the user closes the window */
    while(!glfwWindowShouldClose(game->window))
    {
        nk_glfw3_new_frame(&game->nuklear);
        ProcessInput(game->window);
        Game_Update(game);
        // GBuffer Pass
        {
            PushRenderPassAnnotation("GBuffer Pass");
            {
                PushRenderPassAnnotation("Geometry Pass");
                struct Material *m = Game_FindMaterialByName(game, "GBuffer");
                GLCHECK(glBindFramebuffer(GL_FRAMEBUFFER,
                    game->gbuffer.framebuffer));
                GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                GLCHECK(glUseProgram(Material_GetHandle(m)));

                Material_SetUniform(m, "g_view", sizeof(Mat4X4),
                    &game->camera.view, UT_MAT4);
                Material_SetUniform(m, "g_proj", sizeof(Mat4X4),
                    &game->camera.proj, UT_MAT4);
                Material_SetUniform(m, "g_lightPos", sizeof(Vec3D),
                    &g_lightPos, UT_VEC3F);
                Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D),
                    &eyePos, UT_VEC3F);

                const struct ModelProxy *room = game->models[0];
                for (u32 i = 0; i < room->numMeshes; ++i) {
                    Material_SetTexture(m, "g_albedoTex",
                        &game->albedoTextures[C_DEFAULT_TEX_IDX]);
                    Material_SetTexture(m, "g_normalTex",
                        &game->normalTextures[C_DEFAULT_TEX_IDX]);
                    Material_SetTexture(m, "g_roughnessTex",
                        &game->roughnessTextures[C_DEFAULT_TEX_IDX]);

                    GLCHECK(glBindVertexArray(room->meshes[i].vao));
                    Material_SetUniform(m, "g_world", sizeof(Mat4X4),
                        &room->meshes[i].world, UT_MAT4);
                    GLCHECK(glDrawElements(GL_TRIANGLES,
                        room->meshes[i].numIndices, GL_UNSIGNED_INT, NULL));
                }
                PopRenderPassAnnotation();
            }

            // Decal pass
            {
                PushRenderPassAnnotation("Decal Pass");
                struct Material *m = Game_FindMaterialByName(game, "Decal");
                // Set read only depth
                glDepthFunc(GL_GREATER);
                glDepthMask(GL_FALSE);
                glCullFace(GL_FRONT);
                // Copy gbuffer depth
                {
                    GLCHECK(glBindTexture(GL_TEXTURE_2D,
                        game->gbuffer.depthTex.handle));
                    GLCHECK(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                            game->framebufferSize.width,
                            game->framebufferSize.height));
                    GLCHECK(glBindTexture(GL_TEXTURE_2D, 0));
                }
                const struct ModelProxy *unitCube = game->models[1];
                for (u32 i = 0; i < unitCube->numMeshes; ++i) {
                    GLCHECK(glUseProgram(Material_GetHandle(m)));
                    Material_SetTexture(m, "g_depth", &game->gbuffer.depthTex);
                    const Mat4X4 viewProj = MathMat4X4MultMat4X4ByMat4X4(
                        &game->camera.view, &game->camera.proj);
                    const Mat4X4 invViewProj = MathMat4X4Inverse(&viewProj);
                    const Vec4D rtSize = { game->framebufferSize.width,
                        game->framebufferSize.height,
                        1.0f / game->framebufferSize.width,
                        1.0f / game->framebufferSize.height };

                    Material_SetUniform(m, "g_lightPos", sizeof(Vec3D),
                        &g_lightPos, UT_VEC3F);
                    Material_SetUniform(m, "g_rtSize", sizeof(Vec4D),
                        &rtSize, UT_VEC4F);
                    Material_SetUniform(m, "g_view", sizeof(Mat4X4),
                        &game->camera.view, UT_MAT4);
                    Material_SetUniform(m, "g_proj", sizeof(Mat4X4),
                        &game->camera.proj, UT_MAT4);
                    Material_SetUniform(m, "g_invViewProj", sizeof(Mat4X4),
                        &invViewProj, UT_MAT4);
                    Material_SetUniform(m, "g_lightPos", sizeof(Vec3D),
                        &g_lightPos, UT_VEC3F);
                    Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D),
                        &eyePos, UT_VEC3F);
                    GLCHECK(glBindVertexArray(unitCube->meshes[i].vao));
                    for (u32 n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
                        Material_SetUniform(m, "g_world", sizeof(Mat4X4),
                            &decalWorlds[n], UT_MAT4);
                        Material_SetUniform(m, "g_decalInvWorld",
                            sizeof(Mat4X4), &decalInvWorlds[n], UT_MAT4);
                        Material_SetTexture(m, "g_albedo",
                            &game->albedoTextures[DECAL_TEXTURE_INDICES[n]]);
                        Material_SetTexture(m, "g_normal",
                            &game->normalTextures[DECAL_TEXTURE_INDICES[n]]);
                        GLCHECK(glDrawElements(GL_TRIANGLES,
                            unitCube->meshes[i].numIndices, GL_UNSIGNED_INT,
                            NULL));
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
            struct Material *m = Game_FindMaterialByName(game, "Deferred");
            GLCHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
            GLCHECK(glUseProgram(Material_GetHandle(m)));
            Material_SetTexture(m, "g_position", &game->gbuffer.positionTex);
            Material_SetTexture(m, "g_normal", &game->gbuffer.normalTex);
            Material_SetTexture(m, "g_albedo", &game->gbuffer.albedoTex);
            Material_SetUniform(m, "g_lightPos", sizeof(Vec3D), &g_lightPos,
                UT_VEC3F);
            Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D), &eyePos,
                UT_VEC3F);
            Material_SetUniform(m, "g_gbufferDebugMode", sizeof(i32),
                    &game->gbufferDebugMode, UT_INT);
            RenderQuad(&fsqPass);
            PopRenderPassAnnotation();
        }

        // Copy gbuffer depth to default framebuffer's depth
        {
            PushRenderPassAnnotation("Copy GBuffer Depth Pass");
            glBindFramebuffer(GL_READ_FRAMEBUFFER, game->gbuffer.framebuffer);
            // write to default framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            // blit to default framebuffer. Note that this may or may not work
            // as the internal formats of both the FBO and default framebuffer
            // have to match.
            // the internal formats are implementation defined. This works on
            // all of my systems, but if it doesn't on yours you'll likely have
            // to write to the depth buffer in another shader stage (or somehow
            // see to match the default framebuffer's internal format with the
            // FBO's internal format).
            glBlitFramebuffer(0, 0, game->framebufferSize.width,
                game->framebufferSize.height,
                0, 0, game->framebufferSize.width,
                game->framebufferSize.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            PopRenderPassAnnotation();
        }

        // Wireframe pass
        {
            PushRenderPassAnnotation("Wireframe Pass");
            struct Material *m = Game_FindMaterialByName(game, "Phong");
            glUseProgram(Material_GetHandle(m));
            Material_SetUniform(m, "g_view", sizeof(Mat4X4),
                &game->camera.view, UT_MAT4);
            Material_SetUniform(m, "g_proj", sizeof(Mat4X4),
                &game->camera.proj, UT_MAT4);
            Material_SetUniform(m, "g_lightPos", sizeof(Vec3D),
                &g_lightPos, UT_VEC3F);
            Material_SetUniform(m, "g_cameraPos", sizeof(Vec3D),
                &eyePos, UT_VEC3F);
            static const i32 isWireframe = 1;
            Material_SetUniform(m, "g_wireframe", sizeof(i32), &isWireframe,
                UT_INT);

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            const struct ModelProxy *unitCube = game->models[1];
            for (u32 i = 0; i < unitCube->numMeshes; ++i) {
                glBindVertexArray(unitCube->meshes[i].vao);
                for (u32 n = 0; n < ARRAY_COUNT(decalWorlds); ++n) {
                    Material_SetUniform(m, "g_world", sizeof(Mat4X4),
                        &decalWorlds[n], UT_MAT4);
                    glDrawElements(GL_TRIANGLES,
                        unitCube->meshes[i].numIndices, GL_UNSIGNED_INT, NULL);
                }
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            PopRenderPassAnnotation();
        }

        // GUI Pass
        {
            PushRenderPassAnnotation("Nuklear Pass");
            struct nk_context* ctx = &game->nuklear.ctx;
            if (nk_begin(ctx, "Options", nk_rect(50, 50, 530, 250),
                NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE |
                NK_WINDOW_MINIMIZABLE | NK_WINDOW_TITLE))
            {
                nk_layout_row_dynamic(ctx, 30, 1);
                for (u32 i = 0; i < ARRAY_COUNT(decalTransforms); ++i) {
                    nk_label(ctx, UtilsFormatStr("Decal %u:", i),
                        NK_TEXT_ALIGN_LEFT);
                    nk_layout_row_dynamic(ctx, 30, 4);
                    nk_label(ctx, "Translation:", NK_TEXT_ALIGN_LEFT);
                    nk_property_float(ctx, "#X", -10.0f,
                        &decalTransforms[i].translation.X, 10.0f, 0.1f, 0.0f);
                    nk_property_float(ctx, "#Y", -10.0f,
                        &decalTransforms[i].translation.Y, 10.0f, 0.1f, 0.0f);
                    nk_property_float(ctx, "#Z", -10.0f,
                        &decalTransforms[i].translation.Z, 10.0f, 0.1f, 0.0f);
                    nk_label(ctx, "Rotation:", NK_TEXT_ALIGN_LEFT);
                    nk_property_float(ctx, "#Pitch", -89.0f,
                        &decalTransforms[i].rotation.X, 89.0f, 1.0f, 0.0f);
                    nk_property_float(ctx, "#Yaw", -180.0f,
                        &decalTransforms[i].rotation.Y, 180.0f, 1.0f, 0.0f);
                    nk_property_float(ctx, "#Roll", -89.0f,
                        &decalTransforms[i].rotation.Z, 89.0f, 1.0f, 0.0f);
                    nk_label(ctx, "Scale:", NK_TEXT_ALIGN_LEFT);
                    nk_property_float(ctx, "#X", 1.0f,
                        &decalTransforms[i].scale.X, 10.0f, 0.5f, 0.0f);
                    nk_property_float(ctx, "#Y", 1.0f,
                        &decalTransforms[i].scale.Y, 10.0f, 0.5f, 0.0f);
                    nk_property_float(ctx, "#Z", 1.0f,
                        &decalTransforms[i].scale.Z, 10.0f, 0.5f, 0.0f);
                }
                if (nk_button_label(ctx, "Apply Transform")) {
                    UpdateDecalTransforms(decalWorlds, decalInvWorlds,
                        decalTransforms, ARRAY_COUNT(decalTransforms));
                }
                nk_layout_row_dynamic(ctx, 25, 1);
            }
            nk_end(ctx);

            nk_glfw3_render(&game->nuklear, NK_ANTI_ALIASING_ON,
                MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
            glDisable(GL_BLEND);
            glEnable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_SCISSOR_TEST);
            PopRenderPassAnnotation();
        }

        /* Swap front and back buffers */
        glfwSwapBuffers(game->window);

        /* Poll for and process events */
        glfwPollEvents();
    }

    DeinitNuklear(game->window);
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

f64 GetDeltaTime()
{
    static f64 prevTime = 0.0;
    const f64 now = glfwGetTime();
    const f64 dt = now - prevTime;
    prevTime = now;
    return dt;
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
    GLCHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices,
        GL_STATIC_DRAW));
    GLCHECK(glEnableVertexAttribArray(0));
    GLCHECK(glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(f32),
        (void*)0));
    GLCHECK(glEnableVertexAttribArray(1));
    GLCHECK(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(f32),
        (void*)(3 * sizeof(f32))));
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

    {
        const struct Texture2DCreateInfo info = {
            .format = GL_DEPTH_COMPONENT,
            .internalFormat = GL_DEPTH_COMPONENT,
            .type = GL_UNSIGNED_BYTE,
            .genFB = TRUE,
            .framebufferAttachment = GL_DEPTH_ATTACHMENT,
            .width = fbWidth,
            .height = fbHeight,
            .name = "GBuffer.Depth"
        };
        Texture2D_Init(&gbuffer->depthTex, &info);
    }
    {
        const struct Texture2DCreateInfo info = {
            .format = GL_RGBA,
            .internalFormat = GL_RGBA16F,
            .type = GL_UNSIGNED_BYTE,
            .genFB = TRUE,
            .framebufferAttachment = GL_COLOR_ATTACHMENT0,
            .width = fbWidth,
            .height = fbHeight,
            .name = "GBuffer.Position"
        };
        Texture2D_Init(&gbuffer->positionTex, &info);
    }
    {
        const struct Texture2DCreateInfo info = {
            .format = GL_RGBA,
            .internalFormat = GL_RGBA16F,
            .type = GL_UNSIGNED_BYTE,
            .genFB = TRUE,
            .framebufferAttachment = GL_COLOR_ATTACHMENT1,
            .width = fbWidth,
            .height = fbHeight,
            .name = "GBuffer.Normal"
        };
        Texture2D_Init(&gbuffer->normalTex, &info);
    }
    {
        const struct Texture2DCreateInfo info = {
            .format = GL_RGBA,
            .internalFormat = GL_RGBA,
            .type = GL_UNSIGNED_BYTE,
            .genFB = TRUE,
            .framebufferAttachment = GL_COLOR_ATTACHMENT2,
            .width = fbWidth,
            .height = fbHeight,
            .name = "GBuffer.Albedo"
        };
        Texture2D_Init(&gbuffer->albedoTex, &info);
    }
    const u32 attachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2 };
    GLCHECK(glDrawBuffers(ARRAY_COUNT(attachments), attachments));

    GLCHECK(glGenRenderbuffers(1, &gbuffer->depthRenderBuffer));
    GLCHECK(glBindRenderbuffer(GL_RENDERBUFFER, gbuffer->depthRenderBuffer));
    GLCHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT,
                fbWidth, fbHeight));
    GLCHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER, gbuffer->depthRenderBuffer));

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        UtilsDebugPrint("ERROR: Failed to create GBuffer framebuffer");
        return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return 1;
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
        game->camera.position = MathVec3DAddition(&game->camera.position,
            &offset);
    }
    else if (IsKeyPressed(window, GLFW_KEY_F)) {
        const Vec3D offset = { 0.0f, -1.0f, 0.0f };
        game->camera.position = MathVec3DAddition(&game->camera.position,
            &offset);
    }
    else if (IsKeyPressed(window, GLFW_KEY_W)) {
        game->camera.position = MathVec3DAddition(&game->camera.position,
            &game->camera.front);
    }
    else if (IsKeyPressed(window, GLFW_KEY_S)) {
        game->camera.position = MathVec3DSubtraction(&game->camera.position,
            &game->camera.front);
    }
    else if (IsKeyPressed(window, GLFW_KEY_A)) {
        game->camera.position = MathVec3DSubtraction(&game->camera.position,
            &game->camera.right);
    }
    else if (IsKeyPressed(window, GLFW_KEY_D)) {
        game->camera.position = MathVec3DAddition(&game->camera.position,
            &game->camera.right);
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
    // TODO: Fix pitch, currently front follows circle, if W is pressed
    // continuously
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
    const Vec3D focusPos = MathVec3DAddition(&game->camera.position,
        &game->camera.front);
    const Vec3D up = { 0.0f, 1.0f, 0.0f };
    game->camera.view = MathMat4X4ViewAt(&game->camera.position, &focusPos,
        &up);
}

void PushRenderPassAnnotation(const i8* passName)
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, strlen(passName),
        passName);
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
    const Mat4X4 translation = MathMat4X4TranslateFromVec3D(
        &decalTransforms[i].translation);
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

    game->materials = malloc(
        sizeof(struct Material*) * ARRAY_COUNT(materialCreateInfos));
    game->numMaterials = ARRAY_COUNT(materialCreateInfos);

    for (u32 i = 0; i < game->numMaterials; ++i) {
        game->materials[i] = Material_Create(&materialCreateInfos[i]);
    }
}

void LoadMeshes(struct Game *game)
{
    struct ModelProxy *room = ModelProxy_Create("assets/room.obj");
    {
        Mat4X4 rotate90 = MathMat4X4RotateY(MathToRadians(-90.0f));
        for (u32 i = 0; i < room->numMeshes; ++i) {
            room->meshes[i].world = rotate90;
        }
    }
    struct ModelProxy *unitCube = ModelProxy_Create("assets/unit_cube.obj");
    game->models = malloc(sizeof(struct ModelProxy*) * 2);
    game->numModels = 2;
    game->models[0] = room;
    game->models[1] = unitCube;
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
    if (source != GL_DEBUG_SOURCE_APPLICATION &&
        severity >= GL_DEBUG_SEVERITY_LOW) {
        fprintf(stderr,
            "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
            (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type,
            severity, message);
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

void LoadTextures(struct Game *game)
{
stbi_set_flip_vertically_on_load(1);
    const i8 *albedoTexturePaths[] = {
        "assets/older-wood-flooring-bl/older-wood-flooring_albedo.png",
        "assets/rusty-metal-bl/rusty-metal_albedo.png",
        "assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_COL_1K_SPECULAR.png",
        "assets/albedo_default.png",
    };

    const i8 *normalTexturesPaths[] = {
        "assets/older-wood-flooring-bl/older-wood-flooring_normal-ogl.png",
        "assets/rusty-metal-bl/rusty-metal_normal-ogl.png",
        "assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_NRM_1K_SPECULAR.png",
        "assets/normal_default.png",
    };

    const i8 *roughnessTexturePaths[] = {
        "assets/older-wood-flooring-bl/older-wood-flooring_roughness.png",
        "assets/rusty-metal-bl/rusty-metal_roughness.png",
        "assets/BricksReclaimedWhitewashedOffset001/BricksReclaimedWhitewashedOffset001_GLOSS_1K_SPECULAR.png",
        "assets/normal_default.png",
    };

    game->numTextures = ARRAY_COUNT(albedoTexturePaths);
    game->albedoTextures = malloc(sizeof *game->albedoTextures *
        game->numTextures);
    memset(game->albedoTextures, 0, sizeof *game->albedoTextures *
        game->numTextures);
    game->roughnessTextures = malloc(sizeof *game->roughnessTextures *
        game->numTextures);
    memset(game->roughnessTextures, 0, sizeof *game->roughnessTextures *
        game->numTextures);
    game->normalTextures = malloc(sizeof *game->normalTextures *
        game->numTextures);
    memset(game->normalTextures, 0, sizeof *game->normalTextures *
        game->numTextures);

    for (u32 i = 0; i < ARRAY_COUNT(albedoTexturePaths); ++i) {
        Texture2D_Load(game->albedoTextures + i, albedoTexturePaths[i],
            GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
        Texture2D_Load(game->normalTextures + i, normalTexturesPaths[i],
            GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
        Texture2D_Load(game->roughnessTextures + i, roughnessTexturePaths[i],
            GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
    }
}

struct Game *Game_Create(void)
{
    GLFWwindow* window = InitGLFW(640, 480, "Deferred Decals");
    struct Game *game = malloc(sizeof *game);
    ZERO_MEMORY(game);
    game->window = window;
    glfwGetFramebufferSize(window, &game->framebufferSize.width,
            &game->framebufferSize.height);
    glfwSetWindowUserPointer(window, game);
    glfwSetFramebufferSizeCallback(window, OnFramebufferResize);

    LoadMaterials(game);
    LoadMeshes(game);
    LoadTextures(game);

    return game;
}
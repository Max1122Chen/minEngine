// Reproduces the frame where one shared per-frame block is published by two passes.
//
// The engine library exports neither ScreenUIPass nor the per-object binder, so this probe compiles
// ScreenUIPass.cpp into its own image and reaches the pass through Tests/Access. Everything else
// (RHI, GLFW, glad) comes from the built engine, so the buffers and the GL state are real.
//
// Run it with build_and_run.sh. Before the fix it exits non-zero: the screen-space pass publishes the
// pixel ortho into the block the 3D draws of the same recorded frame point at.

#include "Access/ScreenUIPassTestAccess.h"

#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/EngineSceneBindingSets.h"
#include "Runtime/Function/Render/EngineShaderBindings.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ScreenUIPass.h"
#include "Runtime/Function/Render/RenderPipeline/ScenePerFrameData.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstring>
#include <vector>

using namespace minEngine;
using namespace minEngine::Math;

namespace
{
    constexpr uint32_t kViewportWidth = 64u;
    constexpr uint32_t kViewportHeight = 64u;

    int g_Failures = 0;

    void Report(bool condition, const char* what)
    {
        std::printf("%s %s\n", condition ? "PASS" : "FAIL", what);
        if (!condition)
        {
            ++g_Failures;
        }
    }

    /** The block the 3D passes publish: a camera projection that is not the pixel ortho. */
    PerFrameData MakeCameraBlock()
    {
        PerFrameData data{};
        data.View = Matrix4(1.0f);
        data.View[3][0] = 11.0f;
        data.View[3][1] = 22.0f;
        data.View[3][2] = -33.0f;
        data.Proj = Matrix4(1.0f);
        data.Proj[0][0] = 1.5f;
        data.Proj[1][1] = 0.75f;
        data.Proj[2][2] = -1.02f;
        data.Proj[3][2] = -2.02f;
        data.ViewProj = data.Proj * data.View;
        data.CameraPos = Vector4(11.0f, 22.0f, -33.0f, 1.0f);
        return data;
    }

    /** The block the screen-space pass is expected to publish. */
    PerFrameData MakeExpectedScreenSpaceBlock()
    {
        PerFrameData data{};
        data.View = Matrix4(1.0f);
        data.Proj = ScreenUICoords::MakePixelOrthoProjection(
            static_cast<float>(kViewportWidth),
            static_cast<float>(kViewportHeight));
        data.ViewProj = data.Proj * data.View;
        data.CameraPos = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
        return data;
    }

    std::vector<unsigned char> ReadBlock(RHIBuffer& block)
    {
        std::vector<unsigned char> bytes(sizeof(PerFrameData), 0u);
        const GLuint bufferId = static_cast<GLuint>(static_cast<OpenGLRHIBuffer&>(block).GetBufferId());
        if (bufferId == 0)
        {
            return bytes;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, bufferId);
        glGetBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(bytes.size()), bytes.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return bytes;
    }

    GLint QueryBoundUniformBuffer(uint32_t bindingPoint)
    {
        GLint bufferId = 0;
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, bindingPoint, &bufferId);
        return bufferId;
    }

    GLint BufferId(RHIBuffer& buffer)
    {
        return static_cast<GLint>(static_cast<OpenGLRHIBuffer&>(buffer).GetBufferId());
    }

    bool Holds(const std::vector<unsigned char>& bytes, const PerFrameData& expected)
    {
        return bytes.size() == sizeof(PerFrameData)
            && std::memcmp(bytes.data(), &expected, sizeof(PerFrameData)) == 0;
    }

    struct HeadlessGlContext
    {
        GLFWwindow* Window = nullptr;

        HeadlessGlContext()
        {
            if (!glfwInit())
            {
                return;
            }

            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            Window = glfwCreateWindow(1, 1, "ScreenUIPerFrameUboProbe", nullptr, nullptr);
            if (Window == nullptr)
            {
                return;
            }

            glfwMakeContextCurrent(Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(Window);
                Window = nullptr;
            }
        }

        ~HeadlessGlContext()
        {
            if (Window != nullptr)
            {
                glfwDestroyWindow(Window);
            }
            glfwTerminate();
        }

        bool IsReady() const { return Window != nullptr && glfwGetCurrentContext() != nullptr; }
    };
}

int main()
{
    HeadlessGlContext gl;
    if (!gl.IsReady())
    {
        std::printf("SKIP: no GL 4.6 context available\n");
        return 77;
    }

    OpenGLRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);

    // The renderer creates one block per consumer role.
    RHIBufferRef cameraBlock = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
    RHIBufferRef screenSpaceBlock = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
    if (!cameraBlock || !screenSpaceBlock)
    {
        std::printf("FAIL: could not create the per-frame blocks\n");
        return 1;
    }

    const PerFrameData cameraData = MakeCameraBlock();
    cameraBlock->UpdateSubresource(&cameraData, 0, sizeof(PerFrameData));
    const std::vector<unsigned char> cameraBytesBefore = ReadBlock(*cameraBlock);
    Report(Holds(cameraBytesBefore, cameraData), "the 3D passes published their camera block");

    // The screen-space pass records into the frame after the 3D passes did, and receives the block
    // the renderer gives it. Sharing one block is what makes the later recorder rewrite matrices the
    // earlier draws still have to read at replay.
    RHIBuffer* handed = Testing::TestAccess<ScreenUIPass>::BlockTheRendererHandsTheScreenSpacePass(
        cameraBlock.get(),
        screenSpaceBlock.get());

    ScreenUIPass uiPass;
    uiPass.m_ViewportWidth = kViewportWidth;
    uiPass.m_ViewportHeight = kViewportHeight;
    Testing::TestAccess<ScreenUIPass>::AttachScreenSpacePerFrameBlock(uiPass, handed);
    Testing::TestAccess<ScreenUIPass>::PrepareScreenSpaceCamera(uiPass);

    RHIBuffer* published = Testing::TestAccess<ScreenUIPass>::ScreenSpacePerFrameBlock(uiPass);
    if (published == nullptr)
    {
        std::printf("FAIL: the screen-space pass published no block\n");
        return 1;
    }

    const std::vector<unsigned char> cameraBytesAfter = ReadBlock(*cameraBlock);
    Report(
        Holds(cameraBytesAfter, cameraData),
        "the screen-space pass left the 3D camera block untouched");
    Report(
        cameraBytesAfter == cameraBytesBefore,
        "the 3D camera block kept every byte it had before the UI pass recorded");

    const PerFrameData expectedScreenSpace = MakeExpectedScreenSpaceBlock();
    Report(
        Holds(ReadBlock(*published), expectedScreenSpace),
        "the screen-space block holds the pixel ortho the UI draws with");

    Report(
        published != cameraBlock.get(),
        "the screen-space pass publishes a block of its own");

    // Recorded draws read the block their set0 views: whatever the binder routes at recording time is
    // what the draw sees at replay time.
    RHIBufferRef lights = cmdList.CreateBuffer(MakeUniformBufferDesc(256u));
    RHIBufferRef perObjectRing = cmdList.CreateBuffer(MakeUniformBufferDesc(256u * 8u));
    if (!lights || !perObjectRing)
    {
        std::printf("FAIL: could not create the frame binding buffers\n");
        return 1;
    }

    EngineSceneBindingSets bindings;
    bindings.Initialize(cmdList);
    bindings.BeginFrame(
        cmdList,
        cameraBlock.get(),
        lights.get(),
        perObjectRing.get(),
        256u);

    RHIShaderBindingSetRef sceneDraw = bindings.BindNextPerObjectModel(cmdList, Matrix4(1.0f));
    if (sceneDraw != nullptr)
    {
        cmdList.SetShaderBindingSet(EngineShaderBindings::kSetSceneObject, sceneDraw);
    }
    Report(
        sceneDraw != nullptr
            && QueryBoundUniformBuffer(EngineShaderBindings::kGL_PerFrameUBO) == BufferId(*cameraBlock),
        "a 3D draw binds the camera block");

    Testing::TestAccess<EngineSceneBindingSets>::RouteScreenSpaceBlock(bindings, published);
    RHIShaderBindingSetRef screenSpaceDraw = bindings.BindNextPerObjectModel(cmdList, Matrix4(1.0f));
    if (screenSpaceDraw != nullptr)
    {
        cmdList.SetShaderBindingSet(EngineShaderBindings::kSetSceneObject, screenSpaceDraw);
    }
    Report(
        screenSpaceDraw != nullptr
            && QueryBoundUniformBuffer(EngineShaderBindings::kGL_PerFrameUBO) == BufferId(*published),
        "a screen-space draw binds the screen-space block");

    Testing::TestAccess<EngineSceneBindingSets>::RouteScreenSpaceBlock(bindings, nullptr);
    RHIShaderBindingSetRef nextSceneDraw = bindings.BindNextPerObjectModel(cmdList, Matrix4(1.0f));
    if (nextSceneDraw != nullptr)
    {
        cmdList.SetShaderBindingSet(EngineShaderBindings::kSetSceneObject, nextSceneDraw);
    }
    Report(
        nextSceneDraw != nullptr
            && QueryBoundUniformBuffer(EngineShaderBindings::kGL_PerFrameUBO) == BufferId(*cameraBlock),
        "the next scene draw is back on the camera block");

    bindings.Shutdown();

    std::printf("%s: %d failing check(s)\n", g_Failures == 0 ? "RESULT GREEN" : "RESULT RED", g_Failures);
    return g_Failures == 0 ? 0 : 1;
}

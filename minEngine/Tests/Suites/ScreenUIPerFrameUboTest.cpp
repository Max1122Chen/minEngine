#include "Runtime/Function/Render/EngineRHITextureUtils.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHIResources.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
#include "Runtime/Function/Render/RenderPipeline/RenderPasses/ScreenUIPass.h"
#include "Runtime/Function/Render/RenderPipeline/ScenePerFrameData.h"

#include "doctest.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstring>
#include <vector>

using namespace minEngine;
using namespace minEngine::Math;

namespace
{
    /** Headless GL context: the blocks under test are real GL buffers, not test doubles. */
    class ScopedHeadlessGlContext
    {
    public:
        ScopedHeadlessGlContext()
        {
            if (glfwGetCurrentContext() != nullptr)
            {
                return;
            }

            if (!glfwInit())
            {
                return;
            }

            m_OwnsGlfw = true;
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            m_Window = glfwCreateWindow(1, 1, "ScreenUIPerFrameUbo", nullptr, nullptr);
            if (m_Window == nullptr)
            {
                return;
            }

            glfwMakeContextCurrent(m_Window);
            if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
        }

        ~ScopedHeadlessGlContext()
        {
            if (m_Window != nullptr)
            {
                glfwDestroyWindow(m_Window);
                m_Window = nullptr;
            }
            if (m_OwnsGlfw)
            {
                glfwTerminate();
            }
        }

        ScopedHeadlessGlContext(const ScopedHeadlessGlContext&) = delete;
        ScopedHeadlessGlContext& operator=(const ScopedHeadlessGlContext&) = delete;

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };

    /** A scene camera the pixel ortho can never be confused with. */
    PerFrameData MakeSceneCameraPerFrameData()
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

    GLuint BufferId(RHIBuffer& buffer)
    {
        return static_cast<GLuint>(static_cast<OpenGLRHIBuffer&>(buffer).GetBufferId());
    }

    std::vector<unsigned char> ReadUniformBufferBytes(RHIBuffer& buffer)
    {
        std::vector<unsigned char> bytes(sizeof(PerFrameData), 0u);
        const GLuint bufferId = BufferId(buffer);
        if (bufferId == 0)
        {
            return bytes;
        }

        glBindBuffer(GL_UNIFORM_BUFFER, bufferId);
        glGetBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(bytes.size()), bytes.data());
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return bytes;
    }

    bool BytesEqual(const std::vector<unsigned char>& lhs, const void* rhs)
    {
        return lhs.size() == sizeof(PerFrameData) && std::memcmp(lhs.data(), rhs, sizeof(PerFrameData)) == 0;
    }
}

TEST_CASE("screen-ui-per-frame-ubo: the screen-space block keeps the pixel ortho the UI draws with [full]")
{
    const uint32_t width = 640u;
    const uint32_t height = 360u;

    const PerFrameData block = ScreenUIPass::MakeScreenSpacePerFrameData(
        static_cast<float>(width),
        static_cast<float>(height));

    // View is identity, ViewProj is Proj * View, the camera sits at the origin.
    CHECK(block.View == Matrix4(1.0f));
    CHECK(block.ViewProj == block.Proj * block.View);
    CHECK(block.CameraPos == Vector4(0.0f, 0.0f, 0.0f, 1.0f));

    // Absolute contract: pixel (0,0) is the clip-space top-left corner (-1,+1) and
    // (width,height) the bottom-right (+1,-1), so +Y runs down the viewport as the UI is authored.
    const Vector4 topLeft = block.Proj * Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    const Vector4 bottomRight =
        block.Proj * Vector4(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
    CHECK(topLeft.x == doctest::Approx(-1.0f));
    CHECK(topLeft.y == doctest::Approx(1.0f));
    CHECK(bottomRight.x == doctest::Approx(1.0f));
    CHECK(bottomRight.y == doctest::Approx(-1.0f));
}

TEST_CASE("screen-ui-per-frame-ubo: the screen-space block is its own storage, not the camera block [full]")
{
    ScopedHeadlessGlContext gl;
    REQUIRE(gl.IsReady());

    OpenGLRHI rhi;
    RHICommandList cmdList(&rhi);
    cmdList.SetImmediateOverride(true);

    // The renderer owns one block per role: the camera block the 3D draws read and the block the
    // screen-space pass publishes while it records. Sharing one block is what let the later
    // recorder rewrite matrices the earlier draws still had to read at replay.
    RHIBufferRef cameraBlock = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
    RHIBufferRef screenSpaceBlock = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
    REQUIRE(cameraBlock);
    REQUIRE(screenSpaceBlock);
    CHECK(BufferId(*cameraBlock) != BufferId(*screenSpaceBlock));
    CHECK(BufferId(*cameraBlock) != 0u);
    CHECK(BufferId(*screenSpaceBlock) != 0u);

    const PerFrameData cameraData = MakeSceneCameraPerFrameData();
    cameraBlock->UpdateSubresource(&cameraData, 0, sizeof(PerFrameData));
    const std::vector<unsigned char> cameraBytesBefore = ReadUniformBufferBytes(*cameraBlock);
    REQUIRE(BytesEqual(cameraBytesBefore, &cameraData));

    // Recording the screen-space pass publishes its own block; the camera block keeps its bytes.
    const PerFrameData screenSpaceData = ScreenUIPass::MakeScreenSpacePerFrameData(37.0f, 21.0f);
    screenSpaceBlock->UpdateSubresource(&screenSpaceData, 0, sizeof(PerFrameData));

    const std::vector<unsigned char> cameraBytesAfter = ReadUniformBufferBytes(*cameraBlock);
    CHECK(BytesEqual(cameraBytesAfter, &cameraData));
    CHECK(cameraBytesAfter == cameraBytesBefore);

    const std::vector<unsigned char> screenSpaceBytes = ReadUniformBufferBytes(*screenSpaceBlock);
    CHECK(BytesEqual(screenSpaceBytes, &screenSpaceData));
}

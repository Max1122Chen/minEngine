#include "ShaderCompilerTest.h"

#include "Runtime/Function/Render/EngineShaderUtils.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <filesystem>

namespace
{
    class ScopedHeadlessGl46Context
    {
    public:
        ScopedHeadlessGl46Context()
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
            m_Window = glfwCreateWindow(1, 1, "ShaderCompilerSpirvLoad", nullptr, nullptr);
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

        ~ScopedHeadlessGl46Context()
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

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };
}

TEST_CASE("shader-compiler: discover glslang and compile Present to VK+GL SPIR-V [full]")
{
    using namespace minEngine;

    ShaderCompiler& compiler = ShaderCompiler::Get();
    std::string discoverError;
    REQUIRE(compiler.DiscoverGlslangValidator(&discoverError));

    const std::filesystem::path cacheDir =
        std::filesystem::temp_directory_path() / "minEngineShaderCompilerTestCache";
    compiler.SetCacheDirectory(cacheDir);

    const std::filesystem::path presentVert = EngineShaderUtils::EngineShaderPath("Present.vert");
    const std::filesystem::path presentFrag = EngineShaderUtils::EngineShaderPath("Present.frag");
    REQUIRE(std::filesystem::exists(presentVert));
    REQUIRE(std::filesystem::exists(presentFrag));

    const ShaderCompileResult vertVk =
        compiler.CompileFile(presentVert, ShaderCompilerStage::Vertex, ShaderSpirvTarget::Vulkan);
    CHECK(vertVk.Success);
    CHECK(vertVk.SpirvWords.size() > 0);

    const ShaderCompileResult fragVk =
        compiler.CompileFile(presentFrag, ShaderCompilerStage::Fragment, ShaderSpirvTarget::Vulkan);
    CHECK(fragVk.Success);
    CHECK(fragVk.SpirvWords.size() > 0);

    const ShaderCompileResult vertGl =
        compiler.CompileFile(presentVert, ShaderCompilerStage::Vertex, ShaderSpirvTarget::OpenGL);
    CHECK(vertGl.Success);
    CHECK(vertGl.SpirvWords.size() > 0);

    const ShaderCompileResult fragGl =
        compiler.CompileFile(presentFrag, ShaderCompilerStage::Fragment, ShaderSpirvTarget::OpenGL);
    CHECK(fragGl.Success);
    CHECK(fragGl.SpirvWords.size() > 0);

    // Cache hit path (second compile should succeed and stay non-empty).
    const ShaderCompileResult vertVkCached =
        compiler.CompileFile(presentVert, ShaderCompilerStage::Vertex, ShaderSpirvTarget::Vulkan);
    CHECK(vertVkCached.Success);
    CHECK(vertVkCached.SpirvWords.size() == vertVk.SpirvWords.size());
}

TEST_CASE("shader-compiler: Present OpenGL SPIR-V loads via RHICreateShader [full]")
{
    using namespace minEngine;

    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());
    REQUIRE(GLAD_GL_VERSION_4_6);

    OpenGLRHI rhi;

    std::string loadError;
    const RHIShaderRef shader = EngineShaderUtils::CreateShaderFromSpirvFiles(
        rhi,
        EngineShaderUtils::EngineShaderPath("Present.vert"),
        EngineShaderUtils::EngineShaderPath("Present.frag"),
        &loadError);
    CHECK(shader != nullptr);
    if (shader == nullptr)
    {
        FAIL(loadError.c_str());
    }
    else
    {
        CHECK(shader->IsValid());
    }
}

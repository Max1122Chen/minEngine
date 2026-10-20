#include "MaterialCompileCacheTest.h"

#include "Access/ObjectManagerTestAccess.h"
#include "EngineTestFixture.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/MaterialRenderProxy.h"
#include "Runtime/Function/Render/EngineShaderUtils.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/ShaderCompiler/ShaderCompiler.h"
#include "Runtime/Function/Render/Sprite/SpriteQuadMesh.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/SpriteMaterialFactory.h"
#include "Runtime/Function/Render/Texture.h"

#include "doctest.h"

#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
            m_Window = glfwCreateWindow(1, 1, "MaterialCompileCache", nullptr, nullptr);
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
            // Release process-lifetime GPU statics while this context is still current, mirroring
            // production order (RenderSystem::Shutdown runs before WindowSystem::Shutdown). Without
            // this, the next test case would inherit GL names that died with this context.
            if (glfwGetCurrentContext() != nullptr)
            {
                minEngine::EngineShaderUtils::ClearProgramCache();
                minEngine::SpriteMaterialFactory::ClearTemplateCache();
                minEngine::ScreenUIMaterialFactory::ClearCachedResources();
                minEngine::SpriteQuadMesh::Get().Shutdown();
                minEngine::ShaderCompiler::Get().ClearMemoryCache();
            }

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

        ScopedHeadlessGl46Context(const ScopedHeadlessGl46Context&) = delete;
        ScopedHeadlessGl46Context& operator=(const ScopedHeadlessGl46Context&) = delete;

        bool IsReady() const { return glfwGetCurrentContext() != nullptr; }

    private:
        GLFWwindow* m_Window = nullptr;
        bool m_OwnsGlfw = false;
    };

    /** Tests run from bin/ without a loaded EngineConfig; point the shader template root explicitly. */
    class ScopedEngineDefaultAssetsRoot
    {
    public:
        ScopedEngineDefaultAssetsRoot()
        {
            const std::filesystem::path candidates[] = {
                std::filesystem::path("../Assets/EngineDefault"),
                std::filesystem::path("Assets/EngineDefault"),
                std::filesystem::path("../../minEngine/Assets/EngineDefault"),
            };
            for (const std::filesystem::path& candidate : candidates)
            {
                if (std::filesystem::exists(candidate / "Shaders" / "Template"))
                {
                    minEngine::PathRegistry::Get().SetEngineDefaultAssetsRootOverride(
                        std::filesystem::absolute(candidate));
                    m_bApplied = true;
                    return;
                }
            }
        }

        ~ScopedEngineDefaultAssetsRoot()
        {
            if (m_bApplied)
            {
                minEngine::PathRegistry::Get().ClearEngineDefaultAssetsRootOverride();
            }
        }

        ScopedEngineDefaultAssetsRoot(const ScopedEngineDefaultAssetsRoot&) = delete;
        ScopedEngineDefaultAssetsRoot& operator=(const ScopedEngineDefaultAssetsRoot&) = delete;

        bool IsApplied() const { return m_bApplied; }

    private:
        bool m_bApplied = false;
    };

    class ScopedObjectManager
    {
    public:
        ScopedObjectManager()
        {
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(&m_ObjectManager);
            minEngine::ObjectManager::Initialize();
        }

        ~ScopedObjectManager()
        {
            minEngine::ObjectManager::Shutdown();
            minEngine::Testing::TestAccess<minEngine::ObjectManager>::SetInstance(nullptr);
        }

        ScopedObjectManager(const ScopedObjectManager&) = delete;
        ScopedObjectManager& operator=(const ScopedObjectManager&) = delete;

    private:
        minEngine::ObjectManager m_ObjectManager;
    };

    /** Compile-invocation counters derived from the production log lines (single source of truth). */
    struct CompileCounters
    {
        /** "ShaderCompiler: compiled <label>" — one glslang SPIR-V artifact (vert or frag). */
        int SpirvArtifacts = 0;
        /** "OpenGLRHIShader: loaded SPIR-V program ..." — one GPU program link. */
        int ProgramLinks = 0;
    };

    CompileCounters& GlobalCompileCounters()
    {
        static CompileCounters counters;
        return counters;
    }

    class CompileCountingSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            CompileCounters& counters = GlobalCompileCounters();
            if (record.message.rfind("ShaderCompiler: compiled ", 0) == 0)
            {
                ++counters.SpirvArtifacts;
            }
            else if (record.message.rfind("OpenGLRHIShader: loaded SPIR-V program", 0) == 0)
            {
                ++counters.ProgramLinks;
            }
        }
    };

    void InstallCompileCountingSinkOnce()
    {
        static std::shared_ptr<CompileCountingSink> s_Sink;
        if (!s_Sink)
        {
            s_Sink = std::make_shared<CompileCountingSink>();
            minEngine::LogSystem::AddSink(s_Sink);
        }
    }

    constexpr int kBatchWidgets = 32;
    constexpr int kHudWidgets = 512;

    /** Cold-cache baseline so each case measures a full first load, independent of case order. */
    void ResetCompileCachesForTest()
    {
        minEngine::SpriteMaterialFactory::ClearTemplateCache();
        minEngine::ShaderCompiler::Get().ClearMemoryCache();
        minEngine::EngineShaderUtils::ClearProgramCache();
    }

    std::shared_ptr<minEngine::Texture2D> BatchTexture(minEngine::RHI& rhi)
    {
        return minEngine::ScreenUIMaterialFactory::GetOrCreateWhiteTexture(rhi);
    }

    std::vector<std::shared_ptr<minEngine::Material>> CreateWidgetBatch(
        minEngine::RHI& rhi,
        const minEngine::LinearColor& color)
    {
        const std::shared_ptr<minEngine::Texture2D> texture = BatchTexture(rhi);
        std::vector<std::shared_ptr<minEngine::Material>> materials;
        materials.reserve(kBatchWidgets);
        for (int index = 0; index < kBatchWidgets; ++index)
        {
            std::shared_ptr<minEngine::Material> material =
                minEngine::ScreenUIMaterialFactory::CreateInstance(rhi);
            REQUIRE(material != nullptr);
            minEngine::ScreenUIMaterialFactory::ApplyColorAndTexture(*material, color, texture, rhi);
            materials.push_back(std::move(material));
        }
        return materials;
    }
}

TEST_CASE("material-compile-cache: one widget batch compiles each stage once [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());
    REQUIRE(GLAD_GL_VERSION_4_6);

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const CompileCounters before = GlobalCompileCounters();
    const auto startTime = std::chrono::steady_clock::now();
    const std::vector<std::shared_ptr<Material>> materials =
        CreateWidgetBatch(rhi, LinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    const std::chrono::duration<double, std::milli> elapsed =
        std::chrono::steady_clock::now() - startTime;

    const int spirvArtifacts = GlobalCompileCounters().SpirvArtifacts - before.SpirvArtifacts;
    const int programLinks = GlobalCompileCounters().ProgramLinks - before.ProgramLinks;
    std::printf(
        "[material-compile-cache] widgets=%d spirvArtifacts=%d programLinks=%d elapsedMs=%.1f\n",
        kBatchWidgets,
        spirvArtifacts,
        programLinks,
        elapsed.count());

    for (const std::shared_ptr<Material>& material : materials)
    {
        REQUIRE(material != nullptr);
        CHECK(material->IsCompiledForDraw());
    }

    CHECK(spirvArtifacts == 2);
    CHECK(programLinks == 1);

    RHIShader* sharedProgram = materials.front()->GetGPUShader();
    REQUIRE(sharedProgram != nullptr);
    for (const std::shared_ptr<Material>& material : materials)
    {
        CHECK(material->GetGPUShader() == sharedProgram);
    }
}

TEST_CASE("material-compile-cache: second load of the same UI compiles zero times [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const CompileCounters beforeFirstLoad = GlobalCompileCounters();
    const std::vector<std::shared_ptr<Material>> firstLoad =
        CreateWidgetBatch(rhi, LinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    REQUIRE(firstLoad.size() == static_cast<size_t>(kBatchWidgets));
    const int firstLoadArtifacts =
        GlobalCompileCounters().SpirvArtifacts - beforeFirstLoad.SpirvArtifacts;
    CHECK(firstLoadArtifacts == 2);

    const CompileCounters beforeReload = GlobalCompileCounters();
    const auto startTime = std::chrono::steady_clock::now();
    const std::vector<std::shared_ptr<Material>> secondLoad =
        CreateWidgetBatch(rhi, LinearColor(1.0f, 1.0f, 1.0f, 1.0f));
    const std::chrono::duration<double, std::milli> elapsed =
        std::chrono::steady_clock::now() - startTime;

    const int spirvArtifacts =
        GlobalCompileCounters().SpirvArtifacts - beforeReload.SpirvArtifacts;
    const int programLinks = GlobalCompileCounters().ProgramLinks - beforeReload.ProgramLinks;
    std::printf(
        "[material-compile-cache] reload widgets=%d spirvArtifacts=%d programLinks=%d elapsedMs=%.1f\n",
        kBatchWidgets,
        spirvArtifacts,
        programLinks,
        elapsed.count());

    CHECK(spirvArtifacts == 0);
    CHECK(programLinks == 0);

    for (const std::shared_ptr<Material>& material : secondLoad)
    {
        REQUIRE(material != nullptr);
        CHECK(material->IsCompiledForDraw());
        CHECK(material->GetGPUShader() == firstLoad.front()->GetGPUShader());
    }
}

TEST_CASE("material-compile-cache: runtime tint change is compile-free and isolated [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const std::shared_ptr<Texture2D> texture = BatchTexture(rhi);
    std::shared_ptr<Material> red = ScreenUIMaterialFactory::CreateInstance(rhi);
    std::shared_ptr<Material> green = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(red != nullptr);
    REQUIRE(green != nullptr);

    const CompileCounters beforeTint = GlobalCompileCounters();
    ScreenUIMaterialFactory::ApplyColorAndTexture(*red, LinearColor(1.0f, 0.0f, 0.0f, 1.0f), texture, rhi);
    ScreenUIMaterialFactory::ApplyColorAndTexture(*green, LinearColor(0.0f, 1.0f, 0.0f, 1.0f), texture, rhi);

    float redTint = 0.0f;
    float greenTint = 0.0f;
    REQUIRE(red->GetRenderProxy() != nullptr);
    REQUIRE(green->GetRenderProxy() != nullptr);
    CHECK(red->GetRenderProxy()->GetScalarParameterValue("TintR", redTint));
    CHECK(green->GetRenderProxy()->GetScalarParameterValue("TintR", greenTint));

    const int spirvArtifacts = GlobalCompileCounters().SpirvArtifacts - beforeTint.SpirvArtifacts;
    std::printf(
        "[material-compile-cache] runtimeTintChange spirvArtifacts=%d redTintR=%.3f greenTintR=%.3f\n",
        spirvArtifacts,
        redTint,
        greenTint);

    CHECK(spirvArtifacts == 0);
    CHECK(redTint == doctest::Approx(1.0f));
    CHECK(greenTint == doctest::Approx(0.0f));
    CHECK(red->GetGPUShader() == green->GetGPUShader());
}

TEST_CASE("material-compile-cache: opaque and translucent variants each compile once [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const std::shared_ptr<Texture2D> texture = BatchTexture(rhi);
    const CompileCounters beforeVariants = GlobalCompileCounters();

    RHIShader* opaqueProgram = nullptr;
    RHIShader* translucentProgram = nullptr;
    for (int index = 0; index < 3; ++index)
    {
        std::shared_ptr<Material> opaque = SpriteMaterialFactory::CreateInstance(rhi, false);
        std::shared_ptr<Material> translucent = SpriteMaterialFactory::CreateInstance(rhi, true);
        REQUIRE(opaque != nullptr);
        REQUIRE(translucent != nullptr);
        SpriteMaterialFactory::ApplyColorAndTexture(*opaque, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), texture);
        SpriteMaterialFactory::ApplyColorAndTexture(*translucent, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), texture);

        if (opaqueProgram == nullptr)
        {
            opaqueProgram = opaque->GetGPUShader();
            translucentProgram = translucent->GetGPUShader();
        }
        else
        {
            CHECK(opaque->GetGPUShader() == opaqueProgram);
            CHECK(translucent->GetGPUShader() == translucentProgram);
        }
    }

    const int spirvArtifacts = GlobalCompileCounters().SpirvArtifacts - beforeVariants.SpirvArtifacts;
    std::printf("[material-compile-cache] mixedVariants spirvArtifacts=%d\n", spirvArtifacts);

    REQUIRE(opaqueProgram != nullptr);
    REQUIRE(translucentProgram != nullptr);
    CHECK(opaqueProgram != translucentProgram);
    // Both variants from a cold cache: translucent vert+frag, then opaque frag only
    // (the opaque fragment graph differs; the vertex stage is byte-identical and hits the cache).
    CHECK(spirvArtifacts == 3);
    CHECK(EngineShaderUtils::GetProgramCacheEntryCount() == 2);
}

TEST_CASE("material-compile-cache: warm derivation and batch tint stay compile-free [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const std::shared_ptr<Texture2D> texture = BatchTexture(rhi);
    std::shared_ptr<Material> warmup = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(warmup != nullptr);

    const CompileCounters beforeDerive = GlobalCompileCounters();
    const auto deriveStart = std::chrono::steady_clock::now();
    std::vector<std::shared_ptr<Material>> widgets;
    widgets.reserve(kHudWidgets);
    for (int index = 0; index < kHudWidgets; ++index)
    {
        std::shared_ptr<Material> material = ScreenUIMaterialFactory::CreateInstance(rhi);
        REQUIRE(material != nullptr);
        ScreenUIMaterialFactory::ApplyColorAndTexture(
            *material, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), texture, rhi);
        widgets.push_back(std::move(material));
    }
    const std::chrono::duration<double, std::milli> deriveElapsed =
        std::chrono::steady_clock::now() - deriveStart;

    const CompileCounters beforeTint = GlobalCompileCounters();
    const auto tintStart = std::chrono::steady_clock::now();
    for (int index = 0; index < kHudWidgets; ++index)
    {
        ScreenUIMaterialFactory::ApplyColorAndTexture(
            *widgets[static_cast<size_t>(index)],
            LinearColor(1.0f, 0.5f, 0.25f, 1.0f),
            texture,
            rhi);
    }
    const std::chrono::duration<double, std::milli> tintElapsed =
        std::chrono::steady_clock::now() - tintStart;

    const int deriveArtifacts =
        GlobalCompileCounters().SpirvArtifacts - beforeDerive.SpirvArtifacts;
    const int tintArtifacts = GlobalCompileCounters().SpirvArtifacts - beforeTint.SpirvArtifacts;
    std::printf(
        "[material-compile-cache] warmDerive widgets=%d elapsedMs=%.2f perWidgetUs=%.2f artifacts=%d; batchTint widgets=%d elapsedMs=%.2f perWidgetUs=%.2f artifacts=%d\n",
        kHudWidgets,
        deriveElapsed.count(),
        deriveElapsed.count() * 1000.0 / static_cast<double>(kHudWidgets),
        deriveArtifacts,
        kHudWidgets,
        tintElapsed.count(),
        tintElapsed.count() * 1000.0 / static_cast<double>(kHudWidgets),
        tintArtifacts);

    CHECK(deriveArtifacts == 0);
    CHECK(tintArtifacts == 0);

    float tintR = 0.0f;
    REQUIRE(widgets.front()->GetRenderProxy() != nullptr);
    CHECK(widgets.front()->GetRenderProxy()->GetScalarParameterValue("TintR", tintR));
    CHECK(tintR == doctest::Approx(1.0f));
}

TEST_CASE("material-compile-cache: 512 widget HUD first load stays in budget [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    OpenGLRHI rhi;
    ResetCompileCachesForTest();

    const std::shared_ptr<Texture2D> texture = BatchTexture(rhi);
    const CompileCounters before = GlobalCompileCounters();
    const auto startTime = std::chrono::steady_clock::now();

    std::vector<std::shared_ptr<Material>> widgets;
    widgets.reserve(kHudWidgets);
    for (int index = 0; index < kHudWidgets; ++index)
    {
        std::shared_ptr<Material> material = ScreenUIMaterialFactory::CreateInstance(rhi);
        REQUIRE(material != nullptr);
        ScreenUIMaterialFactory::ApplyColorAndTexture(
            *material,
            LinearColor(1.0f, 1.0f, 1.0f, 1.0f),
            texture,
            rhi);
        widgets.push_back(std::move(material));
    }

    const std::chrono::duration<double, std::milli> elapsed =
        std::chrono::steady_clock::now() - startTime;
    const int spirvArtifacts = GlobalCompileCounters().SpirvArtifacts - before.SpirvArtifacts;
    const int programLinks = GlobalCompileCounters().ProgramLinks - before.ProgramLinks;
    const SpriteMaterialCacheStats stats = SpriteMaterialFactory::GetCacheStats();
    std::printf(
        "[material-compile-cache] hudWidgets=%d spirvArtifacts=%d programLinks=%d elapsedMs=%.1f perWidgetUs=%.1f templateCompiles=%llu derivations=%llu\n",
        kHudWidgets,
        spirvArtifacts,
        programLinks,
        elapsed.count(),
        elapsed.count() * 1000.0 / static_cast<double>(kHudWidgets),
        static_cast<unsigned long long>(stats.TemplateCompiles),
        static_cast<unsigned long long>(stats.InstanceDerivations));

    CHECK(spirvArtifacts == 2);
    CHECK(programLinks == 1);
    CHECK(stats.TemplateCompiles == 1);
    CHECK(stats.InstanceDerivations == static_cast<uint64_t>(kHudWidgets));

    RHIShader* sharedProgram = widgets.front()->GetGPUShader();
    for (const std::shared_ptr<Material>& material : widgets)
    {
        REQUIRE(material != nullptr);
        CHECK(material->IsCompiledForDraw());
        CHECK(material->GetGPUShader() == sharedProgram);
    }
}

namespace
{
    void DrainGlErrors()
    {
        while (glGetError() != GL_NO_ERROR)
        {
        }
    }
}

TEST_CASE("material-compile-cache: RenderSystem shutdown releases GPU caches while the context is alive [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    // Baseline isolation happens BEFORE anything is created; from that point on, nothing clears
    // the caches except the production path under test. The RHI and the GL context outlive
    // RenderSystem::Shutdown() inside this scope, so a GPU destructor running after context
    // teardown would surface as a GL error / dead handle.
    ResetCompileCachesForTest();
    OpenGLRHI rhi;

    const std::shared_ptr<Texture2D> texture = ScreenUIMaterialFactory::GetOrCreateWhiteTexture(rhi);
    REQUIRE(texture != nullptr);
    std::shared_ptr<Material> survivor = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(survivor != nullptr);
    ScreenUIMaterialFactory::ApplyColorAndTexture(
        *survivor, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), texture, rhi);
    RHIShader* survivorProgram = survivor->GetGPUShader();
    REQUIRE(survivorProgram != nullptr);

    // Pre-existing static quad mesh holding GPU buffers (SpriteQuadMesh.cpp:25) is released by the
    // same shutdown point, so it is part of this oracle too.
    SpriteQuadMesh::Get().EnsureInitialized(rhi);
    REQUIRE(SpriteQuadMesh::Get().IsReady());

    REQUIRE(EngineShaderUtils::GetProgramCacheEntryCount() >= 1);
    REQUIRE(SpriteMaterialFactory::GetTemplateCacheEntryCount() >= 1);
    REQUIRE(ScreenUIMaterialFactory::HasCachedWhiteTexture());

    DrainGlErrors();

    // Production shutdown path (no test-only cleanup of the statics before it).
    RenderSystem renderSystem;
    renderSystem.Shutdown();

    std::printf(
        "[material-compile-cache] afterRenderSystemShutdown programCache=%llu templateCache=%llu whiteTexture=%d quadMeshReady=%d\n",
        static_cast<unsigned long long>(EngineShaderUtils::GetProgramCacheEntryCount()),
        static_cast<unsigned long long>(SpriteMaterialFactory::GetTemplateCacheEntryCount()),
        ScreenUIMaterialFactory::HasCachedWhiteTexture() ? 1 : 0,
        SpriteQuadMesh::Get().IsReady() ? 1 : 0);

    CHECK(EngineShaderUtils::GetProgramCacheEntryCount() == 0);
    CHECK(SpriteMaterialFactory::GetTemplateCacheEntryCount() == 0);
    CHECK_FALSE(ScreenUIMaterialFactory::HasCachedWhiteTexture());
    CHECK_FALSE(SpriteQuadMesh::Get().IsReady());

    // The release step must have run on a live, undamaged context.
    CHECK(glfwGetCurrentContext() != nullptr);
    CHECK(glGetError() == GL_NO_ERROR);

    // Refcount semantics: a live Material keeps its own RHIShaderRef, so clearing the caches can
    // never leave a dangling program behind.
    CHECK(survivor->IsCompiledForDraw());
    CHECK(survivor->GetGPUShader() == survivorProgram);
    CHECK(survivorProgram->IsValid());

    // Last owner released here, on a live context: this is the glDeleteProgram path that must not
    // be reached after context teardown.
    survivor.reset();
    CHECK(glGetError() == GL_NO_ERROR);
}
TEST_CASE("material-compile-cache: shutdown is idempotent and GPU caches rebuild lazily [full]")
{
    using namespace minEngine;

    InstallCompileCountingSinkOnce();
    EngineReflectionFixture reflectionFixture;
    REQUIRE(reflectionFixture.IsReflectionReady());
    ScopedEngineDefaultAssetsRoot engineAssets;
    REQUIRE(engineAssets.IsApplied());
    ScopedObjectManager objectManager;
    ScopedHeadlessGl46Context glContext;
    REQUIRE(glContext.IsReady());

    ResetCompileCachesForTest();
    OpenGLRHI rhi;

    // Warm every process-lifetime GPU holder the shutdown point touches.
    const std::shared_ptr<Texture2D> texture = ScreenUIMaterialFactory::GetOrCreateWhiteTexture(rhi);
    std::shared_ptr<Material> material = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(texture != nullptr);
    REQUIRE(material != nullptr);
    ScreenUIMaterialFactory::ApplyColorAndTexture(
        *material, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), texture, rhi);
    SpriteQuadMesh::Get().EnsureInitialized(rhi);
    REQUIRE(SpriteQuadMesh::Get().IsReady());

    RenderSystem renderSystem;
    renderSystem.Shutdown();
    renderSystem.Shutdown(); // idempotent: repeated shutdown must not crash or resurrect caches

    CHECK(EngineShaderUtils::GetProgramCacheEntryCount() == 0);
    CHECK(SpriteMaterialFactory::GetTemplateCacheEntryCount() == 0);
    CHECK_FALSE(ScreenUIMaterialFactory::HasCachedWhiteTexture());
    CHECK_FALSE(SpriteQuadMesh::Get().IsReady());

    // Lazy rebuild after release: a future device/Swapchain rebuild must not lose UI geometry.
    const std::shared_ptr<Texture2D> rebuiltTexture =
        ScreenUIMaterialFactory::GetOrCreateWhiteTexture(rhi);
    REQUIRE(rebuiltTexture != nullptr);
    REQUIRE(rebuiltTexture->GetRHITexture() != nullptr);

    SpriteQuadMesh::Get().EnsureInitialized(rhi);
    REQUIRE(SpriteQuadMesh::Get().IsReady());

    std::shared_ptr<Material> revived = ScreenUIMaterialFactory::CreateInstance(rhi);
    REQUIRE(revived != nullptr);
    ScreenUIMaterialFactory::ApplyColorAndTexture(
        *revived, LinearColor(1.0f, 1.0f, 1.0f, 1.0f), rebuiltTexture, rhi);
    CHECK(revived->IsCompiledForDraw());
    CHECK(revived->GetGPUShader() != nullptr);

    std::printf(
        "[material-compile-cache] afterRebuild programCache=%llu templateCache=%llu whiteTexture=%d quadMeshReady=%d materialCompiled=%d\n",
        static_cast<unsigned long long>(EngineShaderUtils::GetProgramCacheEntryCount()),
        static_cast<unsigned long long>(SpriteMaterialFactory::GetTemplateCacheEntryCount()),
        ScreenUIMaterialFactory::HasCachedWhiteTexture() ? 1 : 0,
        SpriteQuadMesh::Get().IsReady() ? 1 : 0,
        revived->IsCompiledForDraw() ? 1 : 0);

    CHECK(EngineShaderUtils::GetProgramCacheEntryCount() >= 1);
    CHECK(SpriteMaterialFactory::GetTemplateCacheEntryCount() >= 1);
    CHECK(glGetError() == GL_NO_ERROR);
    // The fixture releases these statics before the context dies (production order).
}

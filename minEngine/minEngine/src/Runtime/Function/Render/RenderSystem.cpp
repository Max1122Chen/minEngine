#include "RenderSystem.h"

#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "EngineShaderUtils.h"
#include "OpenGL/OpenGLRHI.h"
#include "ScreenUI/ScreenUIMaterialFactory.h"
#include "ShaderCompiler/ShaderCompiler.h"
#include "Sprite/SpriteMaterialFactory.h"
#include "Sprite/SpriteQuadMesh.h"
#include "Vulkan/VulkanRHI.h"
#include "GLFWWindowSystem.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"

#include "glm/gtc/type_ptr.hpp"

#include "Runtime/Core/Paths/PathRegistry.h"
#include "RenderPipeline/ForwardRenderer.h"
#include "RenderPipeline/ManualRenderer.h"
#include "RenderScene.h"

#include <filesystem>

namespace minEngine
{
    RenderSystem* RenderSystem::s_Instance = nullptr;

    void RenderSystem::SetInstance(RenderSystem* instance)
    {
        s_Instance = instance;
    }

    bool RenderSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    RenderSystem& RenderSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "RenderSystem is not initialized");
        return *s_Instance;
    }

    void RenderSystem::Initialize(SceneRendererKind sceneRenderer)
    {
        const auto createSceneRenderer = [sceneRenderer]() -> std::unique_ptr<SceneRenderer>
        {
            switch (sceneRenderer)
            {
            case SceneRendererKind::Manual:
                ME_LOG(LogRender, Info, "RenderSystem: using ManualRenderer (diagnostic; no RenderGraph).");
                return std::make_unique<ManualRenderer>();
            case SceneRendererKind::Forward:
            default:
                return std::make_unique<ForwardRenderer>();
            }
        };

        if (RHIBackendSelection::IsVulkan())
        {
            m_RHI = std::make_shared<VulkanRHI>();
            m_RHI->Initialize();
            m_RHI->RHISetBackbufferClearColor(Vector3(0.1f, 0.1f, 0.1f));

            m_SceneRenderer = createSceneRenderer();
            m_SceneRenderer->Initialize();

            ME_LOG(LogRender, Info, "RenderSystem Initialized (Vulkan).");
            return;
        }

        m_RHI = std::make_shared<OpenGLRHI>();
        m_RHI->Initialize();

        m_RHI->RHISetBackbufferClearColor(Vector3(0.1f, 0.1f, 0.1f));

        m_SceneRenderer = createSceneRenderer();
        m_SceneRenderer->Initialize();

        ME_LOG(LogRender, Info, "RenderSystem Initialized (OpenGL)");
    }

    void RenderSystem::LoadEngineRenderingAssets()
    {
        if (m_EngineRenderingAssetsLoaded)
        {
            return;
        }

        const std::filesystem::path& assetsRoot = PathRegistry::Get().GetEngineDefaultAssetsRoot();
        if (assetsRoot.empty())
        {
            ME_LOG(LogRender, Warn, 
                "RenderSystem: EngineDefaultAssetsRoot is empty; skipping IBL / SkyBox load (check EngineConfig).");
            return;
        }

        if (m_SceneRenderer)
        {
            m_SceneRenderer->LoadEngineRenderingAssets(assetsRoot.string());
        }
        m_EngineRenderingAssetsLoaded = true;
        ME_LOG(LogRender, Info, "RenderSystem: engine rendering assets loaded.");
    }

    void RenderSystem::ReloadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot)
    {
        if (engineDefaultAssetsRoot.empty())
        {
            return;
        }

        if (m_SceneRenderer)
        {
            m_SceneRenderer->LoadEngineRenderingAssets(engineDefaultAssetsRoot);
        }
        m_EngineRenderingAssetsLoaded = true;
    }

    void RenderSystem::Shutdown()
    {
        // These caches are process-lifetime statics that hold GPU objects (RHIShader,
        // Material, Texture2D). Their destructors call glDeleteProgram/glDeleteTextures, so they
        // must be released while the graphics context is still alive. This function runs before
        // WindowSystem::Shutdown() (the only context destroyer), and before m_RHI->Shutdown().
        // Clearing is refcount-safe: live Materials keep their own RHIShaderRef and stay valid.
        EngineShaderUtils::ClearProgramCache();
        SpriteMaterialFactory::ClearTemplateCache();
        ScreenUIMaterialFactory::ClearCachedResources();
        ShaderCompiler::Get().ClearMemoryCache();

        m_EngineRenderingAssetsLoaded = false;

        if (m_SceneRenderer)
        {
            m_SceneRenderer->Shutdown();
            m_SceneRenderer.reset();
        }

        // Pre-existing static quad mesh (SpriteQuadMesh.cpp:25) also owns GPU buffers and has no
        // caller for its Shutdown(); release it here (after draw consumers are gone, before the
        // context dies). The SpriteQuadMesh files themselves are outside this work item's boundary.
        SpriteQuadMesh::Get().Shutdown();

        if (m_RHI)
        {
            // Anything whose last reference dropped above - and anything the world released on its
            // way here - queued its native handle for a batch tail that is not coming. The device is
            // still alive, so take them now: a handle left queued here is one the backend can no
            // longer destroy, and the release that follows reports it as a leak.
            DiscardOrphanedRenderScenesForTeardown();

            const size_t destroyedHandles = m_RHI->RHIDrainAllDeferredDestroys();
            const size_t leakedHandles = m_RHI->GetDeferredDestroyQueue().GetPendingCount();
            if (leakedHandles != 0)
            {
                ME_LOG(
                    LogRHI,
                    Error,
                    "RenderSystem::Shutdown: {} native handle(s) are still queued for destruction "
                    "and cannot be destroyed once the device goes; the batch tail that would have "
                    "freed them never ran.",
                    leakedHandles);
            }
            else if (destroyedHandles != 0)
            {
                ME_LOG(
                    LogRHI,
                    Info,
                    "RenderSystem::Shutdown: destroyed {} native handle(s) that were waiting for a "
                    "batch tail.",
                    destroyedHandles);
            }

            m_RHI->Shutdown();
            m_RHI.reset();
        }

        m_PendingDraws.clear();

        ME_LOG(LogRender, Info, "RenderSystem Shutdown");
    }

    void RenderSystem::SubmitSceneDraw(const SceneDrawDesc& desc)
    {
        if (RenderCommandPipe::HasInstance())
        {
            RenderCommandPipe::Get().SubmitViewIntent(desc);
            return;
        }

        // Headless helpers that never started Engine frame pipe.
        m_PendingDraws.push_back(desc);
    }

    void RenderSystem::ReplacePendingDraws(std::vector<SceneDrawDesc> draws)
    {
        m_PendingDraws = std::move(draws);
    }

    void RenderSystem::PresentFrame()
    {
        if (m_RHI)
        {
            m_RHI->RHIPresent();
        }
    }

    void RenderSystem::Tick(float deltaTime)
    {
        (void)deltaTime;

        m_RHI->RHIClearBackbuffer();

        if (m_SceneRenderer)
        {
            for (const SceneDrawDesc& drawDesc : m_PendingDraws)
            {
                m_SceneRenderer->Execute(drawDesc);
            }
        }
        m_PendingDraws.clear();
    }

    void RenderSystem::SetPresentPassEnabled(bool enabled)
    {
        if (m_SceneRenderer)
        {
            m_SceneRenderer->SetPresentPassEnabled(enabled);
        }
    }
}
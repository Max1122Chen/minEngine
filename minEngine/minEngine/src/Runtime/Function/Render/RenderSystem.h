#pragma once

#include "Core.h"
#include "Runtime/Function/Render/EngineRenderLimits.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/SceneRenderer.h"
#include "Runtime/Function/Render/SceneRendererKind.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class Engine;
    class RHI;

    class RenderSystem
    {
    public:
        RenderSystem() = default;
        ~RenderSystem() = default;

        void Initialize(SceneRendererKind sceneRenderer = SceneRendererKind::Forward);
        void Shutdown();

        static bool HasInstance();
        static RenderSystem& Get();

        void Tick(float deltaTime);

        /** Swapchain/backbuffer present (RND-F05-S05 neutral frame boundary). */
        void PresentFrame();

        void SubmitSceneDraw(const SceneDrawDesc& desc);

        void SetPresentPassEnabled(bool enabled);

        /** Load IBL + SkyBox from PathRegistry (once). Requires Initialize() and valid EngineDefaultAssetsRoot. */
        void LoadEngineRenderingAssets();

        /** Force reload after path override (editor / hot reload). */
        void ReloadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot);

        bool AreEngineRenderingAssetsLoaded() const { return m_EngineRenderingAssetsLoaded; }

        RHI* GetRHI() const { return m_RHI.get(); }

    public:
        static constexpr uint32_t MAX_POINT_LIGHTS = ::minEngine::MAX_POINT_LIGHTS;
        static constexpr uint32_t MAX_SPOT_LIGHTS = ::minEngine::MAX_SPOT_LIGHTS;

    private:
        friend class Engine;

        static void SetInstance(RenderSystem* instance);
        static RenderSystem* s_Instance;

        std::shared_ptr<RHI> m_RHI;

        std::unique_ptr<SceneRenderer> m_SceneRenderer;
        std::vector<SceneDrawDesc> m_PendingDraws;

        bool m_EngineRenderingAssetsLoaded = false;
    };
}

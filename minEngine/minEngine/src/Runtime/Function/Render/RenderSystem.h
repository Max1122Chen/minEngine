#pragma once
#include "Core.h"
#include "RenderPipeline/RenderPipeline.h"
#include "SceneDrawDesc.h"

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

        void Initialize();
        void Shutdown();

        static RenderSystem& Get();


        void Tick(float deltaTime);

        void SubmitSceneDraw(const SceneDrawDesc& desc);

        void SetPresentPassEnabled(bool enabled);
        void ReloadEngineIBLEnvironment(const std::string& engineDefaultAssetsRoot);
        RHI* GetRHI() const { return m_RHI.get(); }

    public:
        static constexpr uint32_t MAX_POINT_LIGHTS = ::minEngine::MAX_POINT_LIGHTS;
        static constexpr uint32_t MAX_SPOT_LIGHTS = ::minEngine::MAX_SPOT_LIGHTS;



    private:
        friend class Engine;

        static void SetInstance(RenderSystem* instance);
        static RenderSystem* s_Instance;

        std::shared_ptr<RHI> m_RHI;

        RenderPipeline m_RenderPipeline;
        std::vector<SceneDrawDesc> m_PendingDraws;
    };
}

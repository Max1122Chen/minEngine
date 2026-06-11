#pragma once

#include "Core.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIPipelineLayout.h"

#include <unordered_map>

namespace minEngine
{
    class EngineSceneBindingSets;
    class RHICommandList;

    /** Engine-owned pipeline layouts (RND-F04-S01). */
    class EnginePipelineLayouts
    {
    public:
        void Initialize(RHICommandList& cmdList, const EngineSceneBindingSets& sceneBindings);
        void Shutdown();

        RHIPipelineLayout* GetShadowDepthPipelineLayout() const { return m_ShadowDepthPipelineLayout.get(); }
        RHIBindingLayout* GetShadowBindingLayout() const { return m_ShadowBindingLayout.get(); }

        RHIPipelineLayoutRef GetOrCreateSceneMeshPipelineLayout(
            RHICommandList& cmdList,
            const EngineSceneBindingSets& sceneBindings,
            RHIBindingLayout* materialSetLayout) const;

        RHIPipelineLayoutRef GetOrCreatePassLocalPipelineLayout(
            RHICommandList& cmdList,
            RHIBindingLayout* passSetLayout) const;

    private:
        RHIBindingLayoutRef m_ShadowBindingLayout;
        RHIPipelineLayoutRef m_ShadowDepthPipelineLayout;

        mutable std::unordered_map<RHIBindingLayout*, RHIPipelineLayoutRef> m_SceneMeshPipelineByMaterialLayout;
        mutable std::unordered_map<RHIBindingLayout*, RHIPipelineLayoutRef> m_PassLocalPipelineBySetLayout;
    };
}

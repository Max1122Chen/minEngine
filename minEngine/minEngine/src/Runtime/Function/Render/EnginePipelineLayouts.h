#pragma once

#include "Core.h"
#include "Render/RHI/RHIBinding.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"

#include <unordered_map>

namespace minEngine
{
    class EngineSceneBindingSets;
    class RHICommandList;
    class RHIShader;
    class RHIVertexInputLayout;

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

        RHIGraphicsPipelineStateRef GetOrCreateSceneMeshGraphicsPipelineState(
            RHICommandList& cmdList,
            const EngineSceneBindingSets& sceneBindings,
            RHIBindingLayout* materialSetLayout,
            RHIShader* shader,
            RHIVertexInputLayout* vertexInputLayout,
            bool translucentPass) const;

    private:
        struct SceneMeshPSOKey
        {
            RHIPipelineLayout* PipelineLayout = nullptr;
            RHIVertexInputLayout* VertexInputLayout = nullptr;
            RHIShader* Shader = nullptr;
            bool bTranslucentPass = false;

            bool operator==(const SceneMeshPSOKey& other) const
            {
                return PipelineLayout == other.PipelineLayout && VertexInputLayout == other.VertexInputLayout
                    && Shader == other.Shader && bTranslucentPass == other.bTranslucentPass;
            }
        };

        struct SceneMeshPSOKeyHash
        {
            size_t operator()(const SceneMeshPSOKey& key) const
            {
                const size_t layoutHash = std::hash<RHIPipelineLayout*>()(key.PipelineLayout);
                const size_t vilHash = std::hash<RHIVertexInputLayout*>()(key.VertexInputLayout);
                const size_t shaderHash = std::hash<RHIShader*>()(key.Shader);
                const size_t passHash = key.bTranslucentPass ? 1u : 0u;
                return layoutHash ^ (vilHash << 1) ^ (shaderHash << 2) ^ (passHash << 3);
            }
        };

        RHIBindingLayoutRef m_ShadowBindingLayout;
        RHIPipelineLayoutRef m_ShadowDepthPipelineLayout;

        mutable std::unordered_map<RHIBindingLayout*, RHIPipelineLayoutRef> m_SceneMeshPipelineByMaterialLayout;
        mutable std::unordered_map<RHIBindingLayout*, RHIPipelineLayoutRef> m_PassLocalPipelineBySetLayout;
        mutable std::unordered_map<SceneMeshPSOKey, RHIGraphicsPipelineStateRef, SceneMeshPSOKeyHash> m_SceneMeshPsoCache;
    };
}

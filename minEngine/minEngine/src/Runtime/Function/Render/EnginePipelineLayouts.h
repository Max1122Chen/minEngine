#pragma once

#include "Core.h"
#include "Render/RHI/RHIShaderBinding.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIPipelineLayout.h"
#include "Runtime/Function/Render/RenderPipeline/SceneMeshDrawUtils.h"

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
        RHIShaderBindingSetLayout* GetShadowShaderBindingSetLayout() const { return m_ShadowShaderBindingSetLayout.get(); }

        RHIPipelineLayoutRef GetOrCreateSceneMeshPipelineLayout(
            RHICommandList& cmdList,
            const EngineSceneBindingSets& sceneBindings,
            RHIShaderBindingSetLayout* materialSetLayout) const;

        RHIPipelineLayoutRef GetOrCreatePassLocalPipelineLayout(
            RHICommandList& cmdList,
            RHIShaderBindingSetLayout* passSetLayout) const;

        RHIGraphicsPipelineStateRef GetOrCreateSceneMeshGraphicsPipelineState(
            RHICommandList& cmdList,
            const EngineSceneBindingSets& sceneBindings,
            RHIShaderBindingSetLayout* materialSetLayout,
            RHIShader* shader,
            RHIVertexInputLayout* vertexInputLayout,
            MeshPassKind passKind) const;

    private:
        struct SceneMeshPSOKey
        {
            RHIPipelineLayout* PipelineLayout = nullptr;
            RHIVertexInputLayout* VertexInputLayout = nullptr;
            RHIShader* Shader = nullptr;
            MeshPassKind PassKind = MeshPassKind::Opaque;

            bool operator==(const SceneMeshPSOKey& other) const
            {
                return PipelineLayout == other.PipelineLayout && VertexInputLayout == other.VertexInputLayout
                    && Shader == other.Shader && PassKind == other.PassKind;
            }
        };

        struct SceneMeshPSOKeyHash
        {
            size_t operator()(const SceneMeshPSOKey& key) const
            {
                const size_t layoutHash = std::hash<RHIPipelineLayout*>()(key.PipelineLayout);
                const size_t vilHash = std::hash<RHIVertexInputLayout*>()(key.VertexInputLayout);
                const size_t shaderHash = std::hash<RHIShader*>()(key.Shader);
                const size_t passHash = static_cast<size_t>(key.PassKind);
                return layoutHash ^ (vilHash << 1) ^ (shaderHash << 2) ^ (passHash << 3);
            }
        };

        RHIShaderBindingSetLayoutRef m_ShadowShaderBindingSetLayout;
        RHIPipelineLayoutRef m_ShadowDepthPipelineLayout;

        mutable std::unordered_map<RHIShaderBindingSetLayout*, RHIPipelineLayoutRef> m_SceneMeshPipelineByMaterialLayout;
        mutable std::unordered_map<RHIShaderBindingSetLayout*, RHIPipelineLayoutRef> m_PassLocalPipelineByShaderBindingSetLayout;
        mutable std::unordered_map<SceneMeshPSOKey, RHIGraphicsPipelineStateRef, SceneMeshPSOKeyHash> m_SceneMeshPsoCache;
    };
}

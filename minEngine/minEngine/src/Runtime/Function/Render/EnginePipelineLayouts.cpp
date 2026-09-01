#include "EnginePipelineLayouts.h"

#include "EngineSceneBindingSets.h"
#include "EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    void EnginePipelineLayouts::Initialize(RHICommandList& cmdList, const EngineSceneBindingSets& sceneBindings)
    {
        (void)sceneBindings;

        m_ShadowShaderBindingSetLayout = cmdList.CreateShaderBindingSetLayout({
            {EngineShaderBindings::kShadowPass_LightViewProj,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_ShadowPassLightViewProjUBO,
             RHIGraphicsShaderStage::Vertex},
            {EngineShaderBindings::kShadowPass_PerObject,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_PerObjectUBO,
             RHIGraphicsShaderStage::Vertex},
            {EngineShaderBindings::kShadowPass_Params,
             RHIShaderBindingType::UniformBuffer,
             EngineShaderBindings::kGL_ShadowPassParamsUBO,
             RHIGraphicsShaderStage::Pixel},
        });

        m_ShadowDepthPipelineLayout = cmdList.CreatePipelineLayout({m_ShadowShaderBindingSetLayout.get()});
    }

    void EnginePipelineLayouts::Shutdown()
    {
        m_SceneMeshPsoCache.clear();
        m_SceneMeshPipelineByMaterialLayout.clear();
        m_PassLocalPipelineByShaderBindingSetLayout.clear();
        m_ShadowDepthPipelineLayout.reset();
        m_ShadowShaderBindingSetLayout.reset();
    }

    RHIPipelineLayoutRef EnginePipelineLayouts::GetOrCreateSceneMeshPipelineLayout(
        RHICommandList& cmdList,
        const EngineSceneBindingSets& sceneBindings,
        RHIShaderBindingSetLayout* materialSetLayout) const
    {
        if (!materialSetLayout)
        {
            return nullptr;
        }

        const auto existing = m_SceneMeshPipelineByMaterialLayout.find(materialSetLayout);
        if (existing != m_SceneMeshPipelineByMaterialLayout.end())
        {
            return existing->second;
        }

        RHIShaderBindingSetLayout* sceneSet0Layout = sceneBindings.GetSceneSet0Layout();
        RHIShaderBindingSetLayout* sceneSet1Layout = sceneBindings.GetSceneSet1Layout();
        if (!sceneSet0Layout || !sceneSet1Layout)
        {
            return nullptr;
        }

        RHIPipelineLayoutRef pipelineLayout = cmdList.CreatePipelineLayout(
            {sceneSet0Layout, sceneSet1Layout, materialSetLayout});
        m_SceneMeshPipelineByMaterialLayout.emplace(materialSetLayout, pipelineLayout);
        return pipelineLayout;
    }

    RHIPipelineLayoutRef EnginePipelineLayouts::GetOrCreatePassLocalPipelineLayout(
        RHICommandList& cmdList,
        RHIShaderBindingSetLayout* passSetLayout) const
    {
        if (!passSetLayout)
        {
            return nullptr;
        }

        const auto existing = m_PassLocalPipelineByShaderBindingSetLayout.find(passSetLayout);
        if (existing != m_PassLocalPipelineByShaderBindingSetLayout.end())
        {
            return existing->second;
        }

        RHIPipelineLayoutRef pipelineLayout = cmdList.CreatePipelineLayout({passSetLayout});
        m_PassLocalPipelineByShaderBindingSetLayout.emplace(passSetLayout, pipelineLayout);
        return pipelineLayout;
    }

    RHIGraphicsPipelineStateRef EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState(
        RHICommandList& cmdList,
        const EngineSceneBindingSets& sceneBindings,
        RHIShaderBindingSetLayout* materialSetLayout,
        RHIShader* shader,
        RHIVertexInputLayout* vertexInputLayout,
        bool translucentPass) const
    {
        if (!materialSetLayout || !shader || !vertexInputLayout)
        {
            return nullptr;
        }

        RHIPipelineLayoutRef pipelineLayout = GetOrCreateSceneMeshPipelineLayout(
            cmdList,
            sceneBindings,
            materialSetLayout);
        if (!pipelineLayout)
        {
            return nullptr;
        }

        const SceneMeshPSOKey key{
            pipelineLayout.get(),
            vertexInputLayout,
            shader,
            translucentPass};
        const auto existing = m_SceneMeshPsoCache.find(key);
        if (existing != m_SceneMeshPsoCache.end())
        {
            return existing->second;
        }

        RHIGraphicsPSODesc psoDesc;
        psoDesc.PipelineLayout = pipelineLayout.get();
        psoDesc.VertexShader = shader;
        psoDesc.PixelShader = shader;
        psoDesc.VertexInputLayout = vertexInputLayout;
        psoDesc.DepthStencilState.bDepthTestEnabled = true;
        psoDesc.DepthStencilState.bDepthWriteEnabled = true;
        psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::LessEqual;

        if (translucentPass)
        {
            psoDesc.BlendState.bBlendEnabled = true;
        }
        else
        {
            psoDesc.BlendState.bBlendEnabled = false;
        }

        RHIGraphicsPipelineStateRef pipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
        if (!pipelineState)
        {
            return nullptr;
        }

        m_SceneMeshPsoCache.emplace(key, pipelineState);
        return pipelineState;
    }
}

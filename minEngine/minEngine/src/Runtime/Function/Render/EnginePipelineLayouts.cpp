#include "EnginePipelineLayouts.h"

#include "EngineSceneBindingSets.h"
#include "EngineShaderBindings.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

namespace minEngine
{
    void EnginePipelineLayouts::Initialize(RHICommandList& cmdList, const EngineSceneBindingSets& sceneBindings)
    {
        (void)sceneBindings;

        m_ShadowBindingLayout = cmdList.CreateBindingLayout({
            {EngineShaderBindings::kShadowPass_LightViewProj,
             RHIBindingType::UniformBuffer,
             EngineShaderBindings::kGL_ShadowPassLightViewProjUBO,
             RHIGraphicsShaderStage::Vertex},
            {EngineShaderBindings::kShadowPass_PerObject,
             RHIBindingType::UniformBuffer,
             EngineShaderBindings::kGL_PerObjectUBO,
             RHIGraphicsShaderStage::Vertex},
            {EngineShaderBindings::kShadowPass_Params,
             RHIBindingType::UniformBuffer,
             EngineShaderBindings::kGL_ShadowPassParamsUBO,
             RHIGraphicsShaderStage::Pixel},
        });

        m_ShadowDepthPipelineLayout = cmdList.CreatePipelineLayout({m_ShadowBindingLayout.get()});
    }

    void EnginePipelineLayouts::Shutdown()
    {
        m_SceneMeshPsoCache.clear();
        m_SceneMeshPipelineByMaterialLayout.clear();
        m_PassLocalPipelineBySetLayout.clear();
        m_ShadowDepthPipelineLayout.reset();
        m_ShadowBindingLayout.reset();
    }

    RHIPipelineLayoutRef EnginePipelineLayouts::GetOrCreateSceneMeshPipelineLayout(
        RHICommandList& cmdList,
        const EngineSceneBindingSets& sceneBindings,
        RHIBindingLayout* materialSetLayout) const
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

        RHIBindingLayout* sceneSet0Layout = sceneBindings.GetSceneSet0Layout();
        RHIBindingLayout* sceneSet1Layout = sceneBindings.GetSceneSet1Layout();
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
        RHIBindingLayout* passSetLayout) const
    {
        if (!passSetLayout)
        {
            return nullptr;
        }

        const auto existing = m_PassLocalPipelineBySetLayout.find(passSetLayout);
        if (existing != m_PassLocalPipelineBySetLayout.end())
        {
            return existing->second;
        }

        RHIPipelineLayoutRef pipelineLayout = cmdList.CreatePipelineLayout({passSetLayout});
        m_PassLocalPipelineBySetLayout.emplace(passSetLayout, pipelineLayout);
        return pipelineLayout;
    }

    RHIGraphicsPipelineStateRef EnginePipelineLayouts::GetOrCreateSceneMeshGraphicsPipelineState(
        RHICommandList& cmdList,
        const EngineSceneBindingSets& sceneBindings,
        RHIBindingLayout* materialSetLayout,
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
        psoDesc.DepthStencilState.DepthCompare = RHIDepthCompareFunc::Less;

        if (translucentPass)
        {
            psoDesc.BlendState.bBlendEnabled = true;
            psoDesc.DepthStencilState.bDepthWriteEnabled = false;
        }
        else
        {
            psoDesc.BlendState.bBlendEnabled = false;
            psoDesc.DepthStencilState.bDepthWriteEnabled = true;
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

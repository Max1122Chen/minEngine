#include "ShadowPass.h"
#include "Render/EnginePipelineLayouts.h"
#include "Render/EngineShaderUtils.h"
#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/RenderPipeline/ForwardRenderer.h"
#include "Render/RHI/RHI.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/DrawCommands/MeshDrawCommand.h"
#include "Render/DrawCommands/MeshDrawPacket.h"

namespace minEngine
{
    void ShadowPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();

        m_DepthShader = EngineShaderUtils::CreateShaderFromFiles(
            *rhi,
            EngineShaderUtils::EngineShaderPath("ShadowPass.vert"),
            EngineShaderUtils::EngineShaderPath("ShadowPass.frag"));

        if (m_DepthShader)
        {
            RHICommandList cmdList(rhi);
            m_ShadowPSODescTemplate.VertexShader = m_DepthShader.get();
            m_ShadowPSODescTemplate.PixelShader = m_DepthShader.get();
            m_ShadowPSODescTemplate.DepthStencilState.bDepthTestEnabled = true;
            m_ShadowPSODescTemplate.DepthStencilState.bDepthWriteEnabled = true;
            m_ShadowPSODescTemplate.BlendState.bBlendEnabled = false;

            if (pipeline)
            {
                m_ShadowPSODescTemplate.PipelineLayout = pipeline->GetPipelineLayouts().GetShadowDepthPipelineLayout();
            }

            RHIBufferCreateDesc paramsDesc;
            paramsDesc.Usage = RHIBufferUsage::Uniform;
            paramsDesc.ByteSize = sizeof(ShadowPassParamsUBO);
            m_ShadowParamsUniformBuffer = cmdList.CreateBuffer(paramsDesc, nullptr);
        }
    }

    void ShadowPass::EnsureShadowShaderBindingSet(RHICommandList& cmdList)
    {
        if (m_ShadowShaderBindingSet || !pipeline || !m_LightViewProjUniformBuffer || !m_PerObjectUniformBuffer ||
            !m_ShadowParamsUniformBuffer)
        {
            return;
        }

        RHIShaderBindingSetLayout* shadowBindingLayout = pipeline->GetPipelineLayouts().GetShadowShaderBindingSetLayout();
        if (!shadowBindingLayout)
        {
            return;
        }

        m_ShadowShaderBindingSet = cmdList.CreateShaderBindingSet(
            shadowBindingLayout,
            {
                {RHIShaderBindingType::UniformBuffer, m_LightViewProjUniformBuffer, nullptr},
                {RHIShaderBindingType::UniformBuffer, m_PerObjectUniformBuffer, nullptr},
                {RHIShaderBindingType::UniformBuffer, m_ShadowParamsUniformBuffer.get(), nullptr},
            });
    }

    void ShadowPass::UpdateShadowParams(RHICommandList& cmdList, const ShadowPassParamsUBO& params)
    {
        EnsureShadowShaderBindingSet(cmdList);
        if (m_ShadowParamsUniformBuffer)
        {
            m_ShadowParamsUniformBuffer->UpdateSubresource(&params, 0, sizeof(ShadowPassParamsUBO));
        }
    }

    void ShadowPass::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Execute(cmdList);
    }

    void ShadowPass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        RHICommandList cmdList(rhi);
        Render(cmdList);
    }

    void ShadowPass::Execute(RHICommandList& cmdList)
    {
        Render(cmdList);
    }

    RHIGraphicsPipelineStateRef ShadowPass::GetOrCreateShadowPipelineForLayout(
        RHIVertexInputLayout* vertexInputLayout,
        RHICommandList& cmdList)
    {
        if (!vertexInputLayout || !m_DepthShader)
        {
            return nullptr;
        }

        const auto existing = m_ShadowPipelineByLayout.find(vertexInputLayout);
        if (existing != m_ShadowPipelineByLayout.end())
        {
            return existing->second;
        }

        RHIGraphicsPSODesc psoDesc = m_ShadowPSODescTemplate;
        psoDesc.VertexInputLayout = vertexInputLayout;
        RHIGraphicsPipelineStateRef pipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);
        if (!pipelineState)
        {
            return nullptr;
        }

        m_ShadowPipelineByLayout.emplace(vertexInputLayout, pipelineState);
        return pipelineState;
    }

    void ShadowPass::PrepareShadowPass(RHICommandList& cmdList)
    {
        if (!m_DepthShader)
        {
            return;
        }

        EnsureShadowShaderBindingSet(cmdList);
    }

    void ShadowPass::RenderSingleDrawCommand(RHICommandList& cmdList, const ShadowDrawCommand& command)
    {
        if (!m_DepthShader || !command.Handle.IsValid())
        {
            return;
        }

        switch (command.Type)
        {
        case LightType::Directional:
            RenderDirectionalShadow(cmdList, command);
            break;
        case LightType::Spot:
            RenderSpotShadow(cmdList, command);
            break;
        case LightType::Point:
            RenderPointShadow(cmdList, command);
            break;
        default:
            ME_CORE_ERROR("Unsupported light type in ShadowPass::RenderSingleDrawCommand");
            break;
        }
    }

    void ShadowPass::Render(RHICommandList& cmdList)
    {
        PrepareShadowPass(cmdList);

        for (const ShadowDrawCommand& command : m_ShadowDrawCommands)
        {
            RenderSingleDrawCommand(cmdList, command);
        }
    }

    void ShadowPass::UpdateLightViewProjBuffer(const Matrix4& inMatrix)
    {
        if (m_LightViewProjUniformBuffer)
        {
            m_LightViewProjUniformBuffer->UpdateSubresource(&inMatrix, 0, sizeof(Matrix4));
        }
    }

    void ShadowPass::DrawOpaqueMeshes(RHICommandList& cmdList)
    {
        if (!m_ShadowShaderBindingSet || !m_PerObjectUniformBuffer)
        {
            return;
        }

        for (auto& drawCommand : m_OpaqueQueue)
        {
            if (!drawCommand.m_CastShadow)
            {
                continue;
            }

            RHIGraphicsPipelineStateRef pipelineState =
                GetOrCreateShadowPipelineForLayout(drawCommand.m_VertexInputLayout, cmdList);
            if (!pipelineState || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            m_PerObjectUniformBuffer->UpdateSubresource(&drawCommand.m_ModelMatrix, 0, sizeof(Matrix4));

            MeshDrawPacket packet;
            packet.PipelineState = pipelineState;
            packet.ShaderBindingSets[EngineShaderBindings::kSetShadowPass] = m_ShadowShaderBindingSet.get();
            packet.VertexBuffer = drawCommand.m_VertexBuffer;
            packet.IndexBuffer = drawCommand.m_IndexBuffer;
            cmdList.SubmitMeshDrawPacket(packet);
        }
    }

    void ShadowPass::RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& command)
    {
        if (!command.Handle.IsValid() || !command.Handle.HasBoundTexture())
        {
            return;
        }

        RHIRenderPassInfo passInfo;
        passInfo.DepthStencil.DepthStencilTarget = command.Handle.Texture.get();
        passInfo.DepthStencil.ArraySlice = command.Target.TargetLayer;
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
        passInfo.ClearValue.Depth = 1.0f;

        const ShadowResolution& resolution = command.Handle.Resolution;
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);
        UpdateLightViewProjBuffer(command.ViewProj);
        ShadowPassParamsUBO params{};
        params.UseLinearDepth = 0;
        UpdateShadowParams(cmdList, params);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid() || !shadowCommand.Handle.HasBoundTexture())
        {
            return;
        }

        RHIRenderPassInfo passInfo;
        passInfo.DepthStencil.DepthStencilTarget = shadowCommand.Handle.Texture.get();
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
        passInfo.ClearValue.Depth = 1.0f;

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        ShadowPassParamsUBO params{};
        params.UseLinearDepth = 0;
        UpdateShadowParams(cmdList, params);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid() || !shadowCommand.Handle.HasBoundTexture())
        {
            return;
        }

        int face = shadowCommand.Target.TargetFace;
        if (face < 0 || face >= 6)
        {
            return;
        }

        RHIRenderPassInfo passInfo;
        passInfo.DepthStencil.DepthStencilTarget = shadowCommand.Handle.Texture.get();
        passInfo.DepthStencil.ArraySlice = face;
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
        passInfo.ClearValue.Depth = 1.0f;

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        ShadowPassParamsUBO params{};
        params.UseLinearDepth = 1;
        params.LightPos[0] = shadowCommand.LightPosition.x;
        params.LightPos[1] = shadowCommand.LightPosition.y;
        params.LightPos[2] = shadowCommand.LightPosition.z;
        params.FarPlane = shadowCommand.FarPlane;
        UpdateShadowParams(cmdList, params);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }
}

#include "ShadowPass.h"
#include "Render/EngineShaderUtils.h"
#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/RHI/RHI.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/DrawCommands/MeshDrawCommand.h"

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
            RHIGraphicsPSODesc psoDesc;
            psoDesc.VertexShader = m_DepthShader.get();
            psoDesc.PixelShader = m_DepthShader.get();
            psoDesc.DepthStencilState.bDepthTestEnabled = true;
            psoDesc.DepthStencilState.bDepthWriteEnabled = true;
            psoDesc.BlendState.bBlendEnabled = false;
            m_ShadowPipelineState = cmdList.CreateGraphicsPipelineState(psoDesc);

            RHIBufferCreateDesc paramsDesc;
            paramsDesc.Usage = RHIBufferUsage::Uniform;
            paramsDesc.ByteSize = sizeof(ShadowPassParamsUBO);
            m_ShadowParamsUniformBuffer = cmdList.CreateBuffer(paramsDesc, nullptr);
        }
    }

    void ShadowPass::EnsureShadowBindingSet(RHICommandList& cmdList)
    {
        if (m_ShadowBindingSet || !m_LightViewProjUniformBuffer || !m_PerObjectUniformBuffer || !m_ShadowParamsUniformBuffer)
        {
            return;
        }

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

        m_ShadowBindingSet = cmdList.CreateBindingSet(
            m_ShadowBindingLayout.get(),
            {
                {RHIBindingType::UniformBuffer, m_LightViewProjUniformBuffer, nullptr},
                {RHIBindingType::UniformBuffer, m_PerObjectUniformBuffer, nullptr},
                {RHIBindingType::UniformBuffer, m_ShadowParamsUniformBuffer.get(), nullptr},
            });
    }

    void ShadowPass::UpdateShadowParams(RHICommandList& cmdList, const ShadowPassParamsUBO& params)
    {
        EnsureShadowBindingSet(cmdList);
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

    void ShadowPass::Render(RHICommandList& cmdList)
    {
        if (!m_DepthShader || !m_ShadowPipelineState)
        {
            return;
        }

        EnsureShadowBindingSet(cmdList);

        for (const auto& command : m_ShadowDrawCommands)
        {
            if (!command.Handle.IsValid())
            {
                continue;
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
                ME_CORE_ERROR("Unsupported light type in ShadowPass::Render");
                break;
            }
        }
    }

    void ShadowPass::UpdateLightViewProjBuffer(Matrix4 inMatrix)
    {
        if (m_LightViewProjUniformBuffer)
        {
            m_LightViewProjUniformBuffer->UpdateSubresource(&inMatrix, 0, sizeof(Matrix4));
        }
    }

    void ShadowPass::DrawOpaqueMeshes(RHICommandList& cmdList)
    {
        if (!m_ShadowBindingSet || !m_PerObjectUniformBuffer)
        {
            return;
        }

        for (auto& drawCommand : m_OpaqueQueue)
        {
            if (!drawCommand.m_CastShadow)
            {
                continue;
            }

            m_PerObjectUniformBuffer->UpdateSubresource(&drawCommand.m_ModelMatrix, 0, sizeof(Matrix4));
            cmdList.SetBindingSet(EngineShaderBindings::kSetShadowPass, m_ShadowBindingSet.get());

            if (drawCommand.m_VertexInputLayout)
            {
                cmdList.SetVertexInputLayout(drawCommand.m_VertexInputLayout);
            }
            if (drawCommand.m_VertexBuffer)
            {
                cmdList.SetVertexBuffer(drawCommand.m_VertexBuffer);
            }

            if (drawCommand.m_IndexBuffer)
            {
                cmdList.SetIndexBuffer(drawCommand.m_IndexBuffer);
                cmdList.DrawIndexed(drawCommand.m_IndexBuffer->GetDesc().ElementCount, 0, 0);
            }
            else if (drawCommand.m_VertexBuffer)
            {
                cmdList.Draw(drawCommand.m_VertexBuffer->GetDesc().ElementCount, 0);
            }
        }
    }

    void ShadowPass::RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& command)
    {
        if (!command.Handle.IsValid() || !command.Handle.Texture)
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
        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

        UpdateLightViewProjBuffer(command.ViewProj);
        ShadowPassParamsUBO params{};
        params.UseLinearDepth = 0;
        UpdateShadowParams(cmdList, params);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid() || !shadowCommand.Handle.Texture)
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
        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        ShadowPassParamsUBO params{};
        params.UseLinearDepth = 0;
        UpdateShadowParams(cmdList, params);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid() || !shadowCommand.Handle.Texture)
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
        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

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

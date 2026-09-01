#include "ShadowPass.h"
#include "Render/EnginePipelineLayouts.h"
#include "Render/EngineShaderUtils.h"
#include "Render/EngineShaderBindings.h"
#include "Render/EnginePassUniforms.h"
#include "Render/RenderPipeline/ForwardRenderer.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"
#include "Render/RHI/RHIBackend.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHITexture.h"
#include "Render/DrawCommands/MeshDrawCommand.h"
#include "Render/DrawCommands/MeshDrawPacket.h"
#include "Runtime/Core/Log/LogSystem.h"

namespace minEngine
{
    void ShadowPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();

        m_DepthShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
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
            // Cull light-facing faces so receivers (especially large ground planes) do not
            // write their own depth into the map — primary fix for directional self-shadow acne.
            m_ShadowPSODescTemplate.RasterizerState.bCullEnabled = true;
            m_ShadowPSODescTemplate.RasterizerState.CullMode = GetShadowPassCapabilities().GetEffectiveCullMode();
            m_ShadowPSODescTemplate.RasterizerState.DepthBiasSlopeScale =
                GetShadowPassCapabilities().DepthBiasSlopeScale;
            m_ShadowPSODescTemplate.RasterizerState.DepthBiasConstant =
                GetShadowPassCapabilities().DepthBiasConstant;

            const RHICullMode effectiveCull = GetShadowPassCapabilities().GetEffectiveCullMode();
            const char* cullLabel = "None";
            if (effectiveCull == RHICullMode::Front)
            {
                cullLabel = "Front";
            }
            else if (effectiveCull == RHICullMode::Back)
            {
                cullLabel = "Back";
            }
            ME_CORE_INFO(
                "ShadowPass: raster cull={} (enabled={}), depthBias slope={} constant={}",
                cullLabel,
                m_ShadowPSODescTemplate.RasterizerState.bCullEnabled,
                m_ShadowPSODescTemplate.RasterizerState.DepthBiasSlopeScale,
                m_ShadowPSODescTemplate.RasterizerState.DepthBiasConstant);

            // Extra depth push for remaining two-sided / grazing casters.
            // Depth-only pass: tell Vulkan PSO creation to use zero color attachments.
            m_ShadowPSODescTemplate.RenderTargetsEnabled = 0;
            m_ShadowPSODescTemplate.DepthStencilTargetFormat = TextureFormat::DEPTH32;

            if (pipeline)
            {
                m_ShadowPSODescTemplate.PipelineLayout = pipeline->GetPipelineLayouts().GetShadowDepthPipelineLayout();
            }
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
        m_PendingShadowBindingSets.clear();
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

    void ShadowPass::DrawOpaqueMeshes(RHICommandList& cmdList, const ShadowPassUniformBinding& uniformBinding)
    {
        if (!pipeline || !m_PerObjectUniformBuffer || !uniformBinding.IsValid())
        {
            return;
        }

        RHIShaderBindingSetLayout* shadowBindingLayout = pipeline->GetPipelineLayouts().GetShadowShaderBindingSetLayout();
        if (!shadowBindingLayout)
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

            const uint32_t perObjectOffset = pipeline->GetSceneBindings().WriteNextPerObjectModel(drawCommand.m_ModelMatrix);
            std::vector<RHIShaderBinding> resources(3);
            resources[0] = {
                RHIShaderBindingType::UniformBuffer,
                uniformBinding.ViewProjBuffer,
                nullptr,
                uniformBinding.ViewProjByteOffset,
                static_cast<uint32_t>(sizeof(Matrix4))};
            resources[1] = {
                RHIShaderBindingType::UniformBuffer,
                m_PerObjectUniformBuffer,
                nullptr,
                perObjectOffset,
                static_cast<uint32_t>(sizeof(Matrix4))};
            resources[2] = {
                RHIShaderBindingType::UniformBuffer,
                uniformBinding.ParamsBuffer,
                nullptr,
                uniformBinding.ParamsByteOffset,
                static_cast<uint32_t>(sizeof(ShadowPassParamsUBO))};

            RHIShaderBindingSetRef shadowSet = cmdList.CreateShaderBindingSet(shadowBindingLayout, resources);
            if (!shadowSet)
            {
                continue;
            }
            m_PendingShadowBindingSets.push_back(shadowSet);

            MeshDrawPacket packet;
            packet.PipelineState = pipelineState;
            packet.ShaderBindingSets[EngineShaderBindings::kSetShadowPass] = shadowSet.get();
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
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height, RHIViewportConvention::ShadowMap2D);
        ShadowPassUniformBinding uniformBinding{};
        uniformBinding.ViewProjBuffer = command.ViewProjUniformBuffer;
        uniformBinding.ViewProjByteOffset = command.ViewProjUniformOffset;
        uniformBinding.ParamsBuffer = command.ParamsUniformBuffer;
        uniformBinding.ParamsByteOffset = command.ParamsUniformOffset;
        DrawOpaqueMeshes(cmdList, uniformBinding);
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
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height, RHIViewportConvention::ShadowMap2D);
        ShadowPassUniformBinding uniformBinding{};
        uniformBinding.ViewProjBuffer = shadowCommand.ViewProjUniformBuffer;
        uniformBinding.ViewProjByteOffset = shadowCommand.ViewProjUniformOffset;
        uniformBinding.ParamsBuffer = shadowCommand.ParamsUniformBuffer;
        uniformBinding.ParamsByteOffset = shadowCommand.ParamsUniformOffset;
        DrawOpaqueMeshes(cmdList, uniformBinding);
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
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height, RHIViewportConvention::CubeMapFace);
        ShadowPassUniformBinding uniformBinding{};
        uniformBinding.ViewProjBuffer = shadowCommand.ViewProjUniformBuffer;
        uniformBinding.ViewProjByteOffset = shadowCommand.ViewProjUniformOffset;
        uniformBinding.ParamsBuffer = shadowCommand.ParamsUniformBuffer;
        uniformBinding.ParamsByteOffset = shadowCommand.ParamsUniformOffset;
        DrawOpaqueMeshes(cmdList, uniformBinding);
        cmdList.EndRenderPass();
    }
}

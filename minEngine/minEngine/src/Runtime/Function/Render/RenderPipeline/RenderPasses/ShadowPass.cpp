#include "ShadowPass.h"
#include "Render/RHI/RHI.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHIGraphicsPipelineState.h"
#include "Render/RHI/RHIRenderPass.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/Shader.h"
#include "Render/DrawCommands/MeshDrawCommand.h"

#include "Runtime/Function/Render/OpenGL/OpenGLRHIModern.h"
#include "Runtime/Function/Render/OpenGL/OpenGLShader.h"

namespace minEngine
{
    void ShadowPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();

        if (std::shared_ptr<Shader> depthShader = Shader::CreateFromFiles(
                *rhi,
                Shader::EngineShaderPath("ShadowPass.vert"),
                Shader::EngineShaderPath("ShadowPass.frag")))
        {
            m_DepthOnlyShader = depthShader->GetRHIShader();
            auto glShader = std::dynamic_pointer_cast<OpenGLShader>(m_DepthOnlyShader);
            if (glShader)
            {
                m_DepthShader = std::make_shared<OpenGLRHIShader>(glShader);
            }
        }

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
        if (!m_DepthOnlyShader || !m_ShadowPipelineState)
        {
            return;
        }

        m_DepthOnlyShader->BindUniformBlock("LightViewProj", 8);

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
        m_LightViewProjUniformBuffer->UpdateData(&inMatrix, 0, sizeof(Matrix4));
    }

    void ShadowPass::DrawOpaqueMeshes(RHICommandList& cmdList)
    {
        for (auto& drawCommand : m_OpaqueQueue)
        {
            if (!drawCommand.m_CastShadow)
            {
                continue;
            }

            m_DepthOnlyShader->UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);

            auto layout = OpenGLRHIVertexInputLayout::WrapLegacyVertexDefinition(drawCommand.m_VertexDefinition);
            auto vertexBuffer = OpenGLRHIBuffer::WrapLegacyVertexBuffer(drawCommand.m_VertexBuffer);

            if (layout)
            {
                cmdList.SetVertexInputLayout(layout.get());
            }
            if (vertexBuffer)
            {
                cmdList.SetVertexBuffer(vertexBuffer.get());
            }

            if (drawCommand.m_IndexBuffer)
            {
                auto indexBuffer = OpenGLRHIBuffer::WrapLegacyIndexBuffer(drawCommand.m_IndexBuffer);
                if (indexBuffer)
                {
                    cmdList.SetIndexBuffer(indexBuffer.get());
                    cmdList.DrawIndexed(drawCommand.m_IndexBuffer->GetNumIndices(), 0, 0);
                }
            }
            else if (drawCommand.m_VertexBuffer)
            {
                cmdList.Draw(drawCommand.m_VertexBuffer->GetNumVertices(), 0);
            }
        }
    }

    void ShadowPass::RenderDirectionalShadow(RHICommandList& cmdList, const ShadowDrawCommand& command)
    {
        if (!command.Handle.IsValid())
        {
            return;
        }

        auto depthArray = command.Handle.GetAs2DArray();
        if (!depthArray)
        {
            return;
        }

        std::shared_ptr<RHITexture> depthRHI = OpenGLRHITexture::WrapLegacy2DArray(
            depthArray,
            static_cast<uint32_t>(command.Target.TargetLayer));

        RHIRenderPassInfo passInfo;
        passInfo.DepthStencil.DepthStencilTarget = depthRHI.get();
        passInfo.DepthStencil.ArraySlice = command.Target.TargetLayer;
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
        passInfo.ClearValue.Depth = 1.0f;

        const ShadowResolution& resolution = command.Handle.Resolution;
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);
        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

        UpdateLightViewProjBuffer(command.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8);
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 0);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderSpotShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid())
        {
            return;
        }

        auto depthTexture = shadowCommand.Handle.GetAs2D();
        if (!depthTexture)
        {
            return;
        }

        std::shared_ptr<RHITexture> depthRHI = OpenGLRHITexture::WrapLegacy2D(depthTexture);

        RHIRenderPassInfo passInfo;
        passInfo.DepthStencil.DepthStencilTarget = depthRHI.get();
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;
        passInfo.ClearValue.Depth = 1.0f;

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);
        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8);
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 0);

        DrawOpaqueMeshes(cmdList);
        cmdList.EndRenderPass();
    }

    void ShadowPass::RenderPointShadow(RHICommandList& cmdList, const ShadowDrawCommand& shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid())
        {
            return;
        }

        auto depthCube = shadowCommand.Handle.GetAsCube();
        if (!depthCube)
        {
            return;
        }

        int face = shadowCommand.Target.TargetFace;
        if (face < 0 || face >= 6)
        {
            return;
        }

        RHIRenderPassInfo passInfo;
        passInfo.ClearValue.Depth = 1.0f;
        passInfo.DepthStencil.Action = RHIDepthStencilTargetActions::ClearDepthStencilStoreDepthStencil;

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        cmdList.SetViewport(0, 0, resolution.Width, resolution.Height);

        m_FrameBuffer->AttachDepthCubeFace(depthCube, static_cast<uint32_t>(face));
        m_FrameBuffer->Bind();
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi)
        {
            rhi->Clear();
        }

        cmdList.SetGraphicsPipelineState(m_ShadowPipelineState.get());

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8);
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 1);
        m_DepthOnlyShader->UploadUniformFloat3("u_LightPos", shadowCommand.LightPosition);
        m_DepthOnlyShader->UploadUniformFloat("u_FarPlane", shadowCommand.FarPlane);

        DrawOpaqueMeshes(cmdList);
        m_FrameBuffer->Unbind();
    }
}

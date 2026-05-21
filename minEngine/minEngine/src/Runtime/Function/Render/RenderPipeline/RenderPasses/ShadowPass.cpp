#include "ShadowPass.h"
#include "Render/RHI/RHI.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIShader.h"
#include "Render/Shader.h"
#include "Render/DrawCommands/MeshDrawCommand.h"

#include "Render/OpenGL/OpenGLVertexArrayObject.h"
#include <glad/glad.h>

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
        }
    }

    void minEngine::ShadowPass::Execute()
    {
        Render();
    }

    void ShadowPass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        rhi->SetDrawBuffer(-1); // No color output for shadow pass
        rhi->SetReadBuffer(-1);
        rhi->EnableDepthTest();

        // For simplicity, we will render all opaque objects in the shadow pass.
        // In a real implementation, you would want to cull objects that are outside the light's frustum to improve performance.

        m_DepthOnlyShader->Use();
        m_DepthOnlyShader->BindUniformBlock("LightViewProj", 8); // Bind the light view projection uniform buffer to the shader

        for(const auto& command : m_ShadowDrawCommands)
        {
            if (!command.Handle.IsValid())
            {
                continue;
            }

            switch (command.Type)
            {
            case LightType::Directional:
                RenderDirectionalShadow(*rhi, command);
                break;
            case LightType::Spot:
                RenderSpotShadow(*rhi, command);
                break;
            case LightType::Point:
                RenderPointShadow(*rhi, command);
                break;
            default:
                ME_CORE_ERROR("Unsupported light type in ShadowPass::Render");
                break;
            }
        }

        m_FrameBuffer->Unbind();
    }

    void ShadowPass::UpdateLightViewProjBuffer(Matrix4 inMatrix)
    {
        m_LightViewProjUniformBuffer->UpdateData(&inMatrix, 0, sizeof(Matrix4));
    }

    void ShadowPass::RenderDirectionalShadow(RHI& rhi, const ShadowDrawCommand& command)
    {
        if (!command.Handle.IsValid())
        {
            ME_CORE_ERROR("Directional shadow resource is not valid in ShadowPass::RenderDirectionalShadow");
            return;
        }

        const ShadowResolution& resolution = command.Handle.Resolution;
        rhi.SetViewport(0, 0, resolution.Width, resolution.Height);

        // Bind the shadow array layer to the framebuffer.
        m_FrameBuffer->AttachDepthBufferLayer(
            command.Handle.GetAs2DArray(),
            static_cast<uint32_t>(command.Target.TargetLayer));
        // AttachDepthBuffer currently unbinds FBO internally, so bind again before clear/draw.
        m_FrameBuffer->Bind();
        rhi.Clear();

        UpdateLightViewProjBuffer(command.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8); // Binding point 8 for light view projection matrix in shadow pass
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 0);

        // Render all opaque objects for this shadow entry
        for(auto& command : m_OpaqueQueue)
        {
            if(!command.m_CastShadow)
            {
                continue;
            }

            m_DepthOnlyShader->UploadUniformMat4("u_Model", command.m_ModelMatrix);
            command.m_VertexDefinition->Bind();
            if(command.m_IndexBuffer)
            {
                command.m_IndexBuffer->Bind();
                glDrawElements(GL_TRIANGLES, command.m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, command.m_VertexBuffer->GetNumVertices());
            }
        }
    }

    void ShadowPass::RenderSpotShadow(RHI &rhi, const ShadowDrawCommand &shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid())
        {
            ME_CORE_ERROR("Spot shadow resource is not valid in ShadowPass::RenderSpotShadow");
            return;
        }

        auto depthTexture = shadowCommand.Handle.GetAs2D();
        if (!depthTexture)
        {
            ME_CORE_ERROR("Spot shadow depth texture is missing in ShadowPass::RenderSpotShadow");
            return;
        }

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        rhi.SetViewport(0, 0, resolution.Width, resolution.Height);

        m_FrameBuffer->AttachDepthBuffer(depthTexture);
        m_FrameBuffer->Bind();
        rhi.Clear();

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8);
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 0);

        for(auto& command : m_OpaqueQueue)
        {
            if(!command.m_CastShadow)
            {
                continue;
            }

            m_DepthOnlyShader->UploadUniformMat4("u_Model", command.m_ModelMatrix);
            command.m_VertexDefinition->Bind();
            if(command.m_IndexBuffer)
            {
                command.m_IndexBuffer->Bind();
                glDrawElements(GL_TRIANGLES, command.m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, command.m_VertexBuffer->GetNumVertices());
            }
        }
    }

    void ShadowPass::RenderPointShadow(RHI &rhi, const ShadowDrawCommand &shadowCommand)
    {
        if (!shadowCommand.Handle.IsValid())
        {
            ME_CORE_ERROR("Point shadow resource is not valid in ShadowPass::RenderPointShadow");
            return;
        }

        auto depthCube = shadowCommand.Handle.GetAsCube();
        if (!depthCube)
        {
            ME_CORE_ERROR("Point shadow cube texture is missing in ShadowPass::RenderPointShadow");
            return;
        }

        int face = shadowCommand.Target.TargetFace;
        if (face < 0 || face >= 6)
        {
            ME_CORE_ERROR("Invalid point shadow cube face index in ShadowPass::RenderPointShadow");
            return;
        }

        const ShadowResolution& resolution = shadowCommand.Handle.Resolution;
        rhi.SetViewport(0, 0, resolution.Width, resolution.Height);

        m_FrameBuffer->AttachDepthCubeFace(depthCube, static_cast<uint32_t>(face));
        m_FrameBuffer->Bind();
        rhi.Clear();

        UpdateLightViewProjBuffer(shadowCommand.ViewProj);
        m_LightViewProjUniformBuffer->BindToBindingPoint(8);
        m_DepthOnlyShader->UploadUniformInt("u_UseLinearDepth", 1);
        m_DepthOnlyShader->UploadUniformFloat3("u_LightPos", shadowCommand.LightPosition);
        m_DepthOnlyShader->UploadUniformFloat("u_FarPlane", shadowCommand.FarPlane);

        for(auto& command : m_OpaqueQueue)
        {
            if(!command.m_CastShadow)
            {
                continue;
            }

            m_DepthOnlyShader->UploadUniformMat4("u_Model", command.m_ModelMatrix);
            command.m_VertexDefinition->Bind();
            if(command.m_IndexBuffer)
            {
                command.m_IndexBuffer->Bind();
                glDrawElements(GL_TRIANGLES, command.m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, command.m_VertexBuffer->GetNumVertices());
            }
        }
    }
}

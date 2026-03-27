#include "ShadowPass.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIShader.h"
#include "Render/DrawCommands/MeshDrawCommand.h"

#include "Render/OpenGL/OpenGLVertexArrayObject.h"
#include <glad/glad.h>

namespace minEngine
{
    void ShadowPass::Initialize()
    {
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();

        m_DepthOnlyShader = rhi->CreateShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/ShadowPass.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/ShadowPass.frag");
    }

    void minEngine::ShadowPass::Execute()
    {
        Render();
    }

    void ShadowPass::Render()
    {
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();
        rhi->SetDrawBuffer(-1); // No color output for shadow pass
        rhi->SetReadBuffer(-1);
        rhi->EnableDepthTest();

        // For simplicity, we will render all opaque objects in the shadow pass.
        // In a real implementation, you would want to cull objects that are outside the light's frustum to improve performance.

        RenderScene* renderScene = RenderSystem::GetRenderSystem().m_RenderScene.get();

        m_DepthOnlyShader->Use();
        m_DepthOnlyShader->BindUniformBlock("LightViewProj", 8); // Bind the light view projection uniform buffer to the shader

        for(auto& shadowEntry : m_DirLightShadowEntries)
        {
            // Set the viewport to the shadow map resolution
            rhi->SetViewport(0, 0, shadowEntry.Resolution, shadowEntry.Resolution);

            // Bind the shadow map to the framebuffer
            m_FrameBuffer->AttachDepthBuffer(shadowEntry.CascadeShadowMaps[0]); // For now we only have one cascade
            // AttachDepthBuffer currently unbinds FBO internally, so bind again before clear/draw.
            m_FrameBuffer->Bind();
            rhi->Clear();

            UpdateLightViewProjBuffer(shadowEntry.LightViewProjMatrix);
            m_LightViewProjUniformBuffer->BindToBindingPoint(8); // Binding point 8 for light view projection matrix in shadow pass

            // Render all opaque objects for this shadow entry
            for(auto& command : m_OpaqueQueue)
            {
                if(!command.m_CastShadow)
                {
                    continue;
                }

                m_DepthOnlyShader->UploadUniformMat4("u_Model", command.m_ModelMatrix);
                static_cast<OpenGLVertexArrayObject*>(command.m_VertexDefinition)->Bind();
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

        m_FrameBuffer->Unbind();
    }

    void ShadowPass::UpdateLightViewProjBuffer(Matrix4 inMatrix)
    {
        m_LightViewProjUniformBuffer->UpdateData(&inMatrix, 0, sizeof(Matrix4));
    }
}

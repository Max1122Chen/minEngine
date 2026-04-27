#include "PostProcessPass.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RenderSystem.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace minEngine
{
    void PostProcessPass::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
    }

    void PostProcessPass::Execute()
    {
        Render();
    }

    void PostProcessPass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if(!rhi)
        {
            return;
        }
        if (!m_ScreenQuadVertexDefinition || !m_PostProcessShader || !m_SceneColorTexture)
        {
            ME_CORE_ERROR("PostProcessPass resources are not ready");
            return;
        }
        
        m_SceneColorTexture->Bind(0);
        m_ScreenQuadVertexDefinition->Bind();

        m_PostProcessShader->Use();
        m_PostProcessShader->UploadUniformInt("u_SceneColor", 0);
        m_PostProcessShader->UploadUniformFloat2("u_InvResolution", Vector2(1.0f / m_SceneColorTexture->GetWidth(), 1.0f / m_SceneColorTexture->GetHeight()));
        
        // For FXAA
        m_PostProcessShader->UploadUniformFloat("u_ReduceMin", 1.0f / 128.0f);
        m_PostProcessShader->UploadUniformFloat("u_ReduceMul", 1.0f / 8.0f);
        m_PostProcessShader->UploadUniformFloat("u_SpanMax", 8.0f);

        // For Sharpen
        m_PostProcessShader->UploadUniformFloat("u_Strength", 0.3f);
        m_PostProcessShader->UploadUniformFloat("u_EdgeThreshold", 0.1f);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}

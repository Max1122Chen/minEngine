#include "PresentPass.h"
#include "Function/RuntimeGlobalContext.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHIShader.h"
#include "Render/RHI/RHITexture.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace minEngine
{

    void PresentPass::Initialize()
    {
        float quadVertices[] = {
        // pos      // uv
        -1, -1,     0, 0,
        1, -1,     1, 0,
        1,  1,     1, 1,

        -1, -1,     0, 0,
        1,  1,     1, 1,
        -1,  1,     0, 1
        };

        RHI* rhi = RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->GetRHI();
        m_ScreenQuadVertexBuffer = rhi->CreateVertexBuffer(quadVertices, sizeof(quadVertices), 6);
        m_ScreenQuadVertexDefinition = rhi->CreateVertexDefinition({
            { "a_Position", VertexElementType::Float2, false },
            { "a_TexCoord", VertexElementType::Float2, false }
        });

        m_ScreenQuadShader = rhi->CreateRHIShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Present.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Present.frag");

    }

    void PresentPass::Execute()
    {
        Render();
    }

    void PresentPass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        if (!m_ScreenQuadVertexDefinition || !m_ScreenQuadShader || !m_SceneColorTexture)
        {
            ME_CORE_ERROR("PresentPass resources are not ready");
            return;
        }

        rhi->DisableDepthTest();
        rhi->DisableBlend();

        m_ScreenQuadVertexDefinition->Bind();
        m_SceneColorTexture->Bind(0); // Bind the scene color texture to texture unit 0

        m_ScreenQuadShader->Use();
        m_ScreenQuadShader->UploadUniformInt("u_SceneColor", 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
}
#include "TranslucencyPass.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/OpenGL/OpenGLRHI.h"
#include "Runtime/Function/Render/OpenGL/OpenGLVertexArrayObject.h"
#include "Runtime/Function/Render/OpenGL/OpenGLBuffers.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Render/RHI/RHIShader.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Runtime/Function/Render/LightSceneProxies/SpotLightSceneProxy.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace minEngine
{
    void TranslucencyPass::Execute()
    {
        SortDrawCommands();
        Render();
    }

    void TranslucencyPass::Render()
    {
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();
        if (!rhi)
        {
            return;
        }

        rhi->EnableBlend();
        // rhi->SetBlendFunc(BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha);
        rhi->EnableDepthTest();
        rhi->SetDepthMask(false); // Disable depth write for translucency

        // render all primitives but only static mesh for now
        RenderScene* renderScene = RenderSystem::GetRenderSystem().m_RenderScene.get();
        RenderCamera* mainCamera = RenderSystem::GetRenderSystem().GetMainCamera();

        for(auto& drawCommand : m_DrawCommands)
        {
            auto material = drawCommand.m_Material;
            if (!material || !drawCommand.m_VertexDefinition || !drawCommand.m_VertexBuffer)
            {
                continue;
            }

            material->BindTextures();
            auto shader = material->m_Shader;
            if (!shader)
            {
                continue;
            }

            shader->Use();
            shader->UploadUniformInt("u_DiffuseMap", 0);

            shader->BindUniformBlock("PerFrameData", 0); // Bind the per-frame uniform buffer to the shader
            shader->BindUniformBlock("LightsData", 1); // Bind the light uniform buffer to the shader

            shader->UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);


            static_cast<OpenGLVertexArrayObject*>(drawCommand.m_VertexDefinition)->Bind();

            if(drawCommand.m_IndexBuffer)
            {
                static_cast<OpenGLIndexBuffer*>(drawCommand.m_IndexBuffer)->Bind();
                glDrawElements(GL_TRIANGLES, drawCommand.m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
                static_cast<OpenGLIndexBuffer*>(drawCommand.m_IndexBuffer)->Unbind();
            }
            else
            {
                glDrawArrays(GL_TRIANGLES, 0, drawCommand.m_VertexBuffer->GetNumVertices());
            }
        }

        rhi->SetDepthMask(true);
        rhi->DisableBlend();
    }

    void TranslucencyPass::SortDrawCommands()
    {
        RenderCamera* mainCamera = RenderSystem::GetRenderSystem().GetMainCamera();
        if (!mainCamera)
        {
            return;
        }

        std::sort(m_DrawCommands.begin(), m_DrawCommands.end(), [](const MeshDrawCommand& a, const MeshDrawCommand& b) {
            // Sort by distance from camera (back to front)
            RenderCamera* mainCamera = RenderSystem::GetRenderSystem().GetMainCamera();
            if (!mainCamera)
            {
                return false;
            }
            float distanceA = glm::length(mainCamera->m_Position - glm::vec3(a.m_ModelMatrix[3]));
            float distanceB = glm::length(mainCamera->m_Position - glm::vec3(b.m_ModelMatrix[3]));
            return distanceA > distanceB; // Sort back to front
        });
    }
}
#include "BasePass.h"
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
#include "Render/RHI/RHITexture.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace minEngine
{
    void BasePass::Execute()
    {
        // For now, we directly call Render here, but in the future, we can have more complex logic here, such as sorting draw commands, etc.
        Render();
    }

    void BasePass::Render()
    {
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();
        rhi->EnableDepthTest();

        // render all primitives but only static mesh for now
        RenderScene* renderScene = RenderSystem::GetRenderSystem().m_RenderScene.get();
        RenderCamera* mainCamera = RenderSystem::GetRenderSystem().GetMainCamera();

        for(auto& drawCommand : m_DrawCommands)
        {
            auto material = drawCommand.m_Material;
            material->BindTextures();
            auto shader = material->m_Shader;

            shader->Use();
            // shader->UploadUniformInt("u_DiffuseMap", 0);

            shader->BindUniformBlock("PerFrameData", 0); // Bind the per-frame uniform buffer to the shader
            shader->BindUniformBlock("LightsData", 1); // Bind the light uniform buffer to the shader

            if(m_DirLightShadowEntries.size() > 0)
            {
                DirLightShadowEntry& shadowEntry = m_DirLightShadowEntries[0];
                shadowEntry.CascadeShadowMaps[0]->Bind(); // Bind the shadow map to texture unit 8
                shader->UploadUniformMat4("u_LightViewProj", shadowEntry.LightViewProjMatrix); // Upload the light view projection matrix for shadow mapping
            }
            shader->UploadUniformInt("u_DirLightShadowMap", 8); // Bind the shadow map to texture unit 8 in the shader
            
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
    }
}
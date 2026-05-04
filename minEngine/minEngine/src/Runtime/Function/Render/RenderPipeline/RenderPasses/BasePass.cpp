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
#include "Render/Shader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <string>

namespace minEngine
{
    void BasePass::Execute()
    {
        // For now, we directly call Render here, but in the future, we can have more complex logic here, such as sorting draw commands, etc.
        Render();
    }

    void BasePass::Render()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }
        rhi->EnableDepthTest();

        // render all primitives but only static mesh for now
        RenderScene* renderScene = RenderSystem::Get().m_RenderScene.get();
        RenderCamera* mainCamera = RenderSystem::Get().GetMainCamera();

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

            shader->GetRHIShader()->Use();
            shader->GetRHIShader()->UploadUniformInt("u_Material.DiffuseMap", 0);

            shader->GetRHIShader()->BindUniformBlock("PerFrameData", 0); // Bind the per-frame uniform buffer to the shader
            shader->GetRHIShader()->BindUniformBlock("LightsData", 1); // Bind the light uniform buffer to the shader

            if(m_DirectionalShadowHandle.IsValid())
            {
                m_DirectionalShadowHandle.GetAs2DArray()->Bind(8); // Bind shadow array to texture unit 8
                shader->GetRHIShader()->UploadUniformInt("u_DirLightShadowMap", 8); // Bind the shadow map to texture unit 8 in the shader

                shader->GetRHIShader()->BindUniformBlock("DirLightViewProjs", 9); // Bind the directional light view projection uniform buffer to the shader for CSM
                shader->GetRHIShader()->BindUniformBlock("CascadeFarPlanes", 10); // Bind the CSM cascade far plane uniform buffer to the shader for CSM
            }

            if (!m_SpotShadowHandles.empty())
            {
                shader->GetRHIShader()->BindUniformBlock("SpotLightViewProjs", 11);
                for (const auto& handle : m_SpotShadowHandles)
                {
                    if (!handle.IsValid())
                    {
                        continue;
                    }

                    auto texture = handle.GetAs2D();
                    if (!texture)
                    {
                        continue;
                    }

                    int slot = handle.TextureUnit - SPOT_SHADOW_MAP_BASE_UNIT;
                    if (slot < 0 || slot >= MAX_SPOT_SHADOW_MAPS)
                    {
                        continue;
                    }

                    texture->Bind(handle.TextureUnit);
                    shader->GetRHIShader()->UploadUniformInt("u_SpotShadowMaps[" + std::to_string(slot) + "]", handle.TextureUnit);
                }
            }

            if (!m_PointShadowHandles.empty())
            {
                for (const auto& handle : m_PointShadowHandles)
                {
                    if (!handle.IsValid())
                    {
                        continue;
                    }

                    auto texture = handle.GetAsCube();
                    if (!texture)
                    {
                        continue;
                    }

                    int slot = handle.TextureUnit - POINT_SHADOW_MAP_BASE_UNIT;
                    if (slot < 0 || slot >= MAX_POINT_SHADOW_MAPS)
                    {
                        continue;
                    }

                    texture->Bind(handle.TextureUnit);
                    shader->GetRHIShader()->UploadUniformInt("u_PointShadowMaps[" + std::to_string(slot) + "]", handle.TextureUnit);
                }
            }
            

            shader->GetRHIShader()->UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);
            

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
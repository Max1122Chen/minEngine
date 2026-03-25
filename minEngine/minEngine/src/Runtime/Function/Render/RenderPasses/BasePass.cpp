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
            shader->UploadUniformInt("u_DiffuseMap", 0);

            shader->BindUniformBlock("PerFrameData", 0); // Bind the per-frame uniform buffer to the shader

            shader->UploadUniformMat4("u_Model", drawCommand.m_ModelMatrix);


            for(auto& dirLight : renderScene->m_DirectionalLightSceneProxies)
            {
                shader->UploadUniformFloat3("u_DirLight.Direction", dirLight->m_Direction);
                shader->UploadUniformFloat3("u_DirLight.Color", dirLight->m_LightColor);
            }

            for(auto& pointLight : renderScene->m_PointLightSceneProxies)
            {
                // For simplicity, only upload the first point light
                shader->UploadUniformFloat3("u_PointLight.Position", pointLight->m_Position);
                shader->UploadUniformFloat3("u_PointLight.Color", pointLight->m_LightColor);
                break;
            }

            for(auto& spotLight : renderScene->m_SpotLightSceneProxies)
            {
                // For simplicity, only upload the first spot light
                shader->UploadUniformFloat3("u_SpotLight.Position", spotLight->m_Position);
                shader->UploadUniformFloat3("u_SpotLight.Direction", spotLight->m_Direction);
                shader->UploadUniformFloat3("u_SpotLight.Color", spotLight->m_LightColor);
                shader->UploadUniformFloat("u_SpotLight.InnerConeAngleCos", cos(glm::radians(spotLight->m_InnerConeAngle)));
                shader->UploadUniformFloat("u_SpotLight.OuterConeAngleCos", cos(glm::radians(spotLight->m_OuterConeAngle)));
                break;
            }
            

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

        // for(auto& primitiveProxy : renderScene->m_PrimitiveSceneProxies)
        // {
        //     StaticMeshSceneProxy* meshProxy = dynamic_cast<StaticMeshSceneProxy*>(primitiveProxy);
        //     if(meshProxy)
        //     {
        //         // set up model matrix
        //         Matrix4 model = meshProxy->m_Transform.ToMatrix();

        //         auto material = meshProxy->m_Material;
        //         material->BindTextures();
        //         auto shader = material->m_Shader;

        //         shader->Use();
        //         shader->UploadUniformInt("u_DiffuseMap", 0);
        //         shader->UploadUniformMat4("u_Model", model);
        //         shader->UploadUniformMat4("u_View", mainCamera->GetViewMatrix());
        //         shader->UploadUniformMat4("u_Projection", mainCamera->GetProjectionMatrix());
        //         shader->UploadUniformMat4("u_MVP", mainCamera->GetProjectionMatrix() * mainCamera->GetViewMatrix() * model);
        //         shader->UploadUniformFloat3("u_ViewPosition", mainCamera->m_Position);

        //         for(auto& dirLight : renderScene->m_DirectionalLightSceneProxies)
        //         {
        //             shader->UploadUniformFloat3("u_DirLight.Direction", dirLight->m_Direction);
        //             shader->UploadUniformFloat3("u_DirLight.Color", dirLight->m_LightColor);
        //         }

        //         for(auto& pointLight : renderScene->m_PointLightSceneProxies)
        //         {
        //             // For simplicity, only upload the first point light
        //             shader->UploadUniformFloat3("u_PointLight.Position", pointLight->m_Position);
        //             shader->UploadUniformFloat3("u_PointLight.Color", pointLight->m_LightColor);
        //             break;
        //         }

        //         for(auto& spotLight : renderScene->m_SpotLightSceneProxies)
        //         {
        //             // For simplicity, only upload the first spot light
        //             shader->UploadUniformFloat3("u_SpotLight.Position", spotLight->m_Position);
        //             shader->UploadUniformFloat3("u_SpotLight.Direction", spotLight->m_Direction);
        //             shader->UploadUniformFloat3("u_SpotLight.Color", spotLight->m_LightColor);
        //             shader->UploadUniformFloat("u_SpotLight.InnerConeAngleCos", cos(glm::radians(spotLight->m_InnerConeAngle)));
        //             shader->UploadUniformFloat("u_SpotLight.OuterConeAngleCos", cos(glm::radians(spotLight->m_OuterConeAngle)));
        //             break;
        //         }
                

        //         static_cast<OpenGLVertexArrayObject*>(meshProxy->m_VertexDefinition)->Bind();

        //         if(meshProxy->m_IndexBuffer)
        //         {
        //             static_cast<OpenGLIndexBuffer*>(meshProxy->m_IndexBuffer)->Bind();
        //             glDrawElements(GL_TRIANGLES, meshProxy->m_IndexBuffer->GetNumIndices(), GL_UNSIGNED_INT, nullptr);
        //             static_cast<OpenGLIndexBuffer*>(meshProxy->m_IndexBuffer)->Unbind();
        //         }
        //         else
        //         {
        //             glDrawArrays(GL_TRIANGLES, 0, meshProxy->m_VertexBuffer->GetNumVertices());
        //         }
        // }
        }
    }
}
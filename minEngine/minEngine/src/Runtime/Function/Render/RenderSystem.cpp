#include "RenderSystem.h"

#include "OpenGL/OpenGLRHI.h"
#include "GLFWWindowSystem.h"
#include "RenderCamera.h"


#include "OpenGL/OpenGLShader.h"
#include "OpenGL/OpenGLVertexArrayObject.h"
#include "OpenGL/OpenGLBuffer.h"
#include "OpenGL/OpenGLTexture.h"

#include "Runtime/Function/Render/StaticMeshSceneProxy.h"
#include "Runtime/Function/Render/Material.h"

#include "PointLightSceneProxy.h"
#include "DirectionalLightSceneProxy.h"
#include "SpotLightSceneProxy.h"

#include "RuntimeGlobalContext.h"

#include "RenderScene.h"

#include "glm/gtc/type_ptr.hpp"

namespace minEngine
{
    void RenderSystem::Initialize()
    {   
        // TODO : Create RHI based on configuration
        m_RHI = std::make_shared<OpenGLRHI>();
        m_RHI->Initialize();

        // Create RenderCamera
        m_Camera = std::make_shared<RenderCamera>();
        m_Camera->Initialize();

        // Create RenderScene
        m_RenderScene = std::make_shared<RenderScene>();

        // TODO: set up default render states
        // enable depth testing
        static_cast<OpenGLRHI*>(m_RHI.get())->EnableDepthTest();

        // set clear color
        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SetClearColor(Vector3(0.1f, 0.1f, 0.1f));


        // Finished Initialization
        ME_CORE_INFO("RenderSystem Initialized");
    }

    void RenderSystem::Shutdown()
    {
        // TODO: implement shutdown logic
        ME_CORE_INFO("RenderSystem Shutdown");
    }

    void RenderSystem::Tick(float deltaTime)
    {
        // Clear the window
        static_cast<OpenGLRHI*>(m_RHI.get())-> m_WindowSystem->Clear();

        // Update camera position based on velocity. TODO: move this to camera update function
        m_Camera->m_Position += m_Camera->m_CameraVelocity.z * m_Camera->m_Forward * deltaTime;
        m_Camera->m_Position += m_Camera->m_CameraVelocity.y * m_Camera->m_Up * deltaTime;
        m_Camera->m_Position += m_Camera->m_CameraVelocity.x * m_Camera->m_Right * deltaTime;

        // render all primitives but only static mesh for now
        for(auto& primitiveProxy : m_RenderScene->m_PrimitiveSceneProxies)
        {
            StaticMeshSceneProxy* meshProxy = dynamic_cast<StaticMeshSceneProxy*>(primitiveProxy);
            if(meshProxy)
            {
                Matrix4 model = meshProxy->m_Transform.ToMatrix();

                auto shader = meshProxy->m_Material->m_Shader;

                shader->Use();
                shader->UploadUniformInt("u_DiffuseMap", 0);
                shader->UploadUniformMat4("u_Model", glm::value_ptr(model));
                shader->UploadUniformMat4("u_View", glm::value_ptr(m_Camera->GetViewMatrix()));
                shader->UploadUniformMat4("u_Projection", glm::value_ptr(m_Camera->GetProjectionMatrix()));
                shader->UploadUniformMat4("u_MVP", glm::value_ptr(m_Camera->GetProjectionMatrix() * m_Camera->GetViewMatrix() * model));
                shader->UploadUniformFloat3("u_ViewPosition", m_Camera->m_Position);

                for(auto& dirLight : m_RenderScene->m_DirectionalLightSceneProxies)
                {
                    shader->UploadUniformFloat3("u_DirLight.Direction", dirLight->m_Direction);
                    shader->UploadUniformFloat3("u_DirLight.Color", dirLight->m_LightColor);
                }

                for(auto& pointLight : m_RenderScene->m_PointLightSceneProxies)
                {
                    // For simplicity, only upload the first point light
                    shader->UploadUniformFloat3("u_PointLight.Position", pointLight->m_Position);
                    shader->UploadUniformFloat3("u_PointLight.Color", pointLight->m_LightColor);
                    break;
                }

                for(auto& spotLight : m_RenderScene->m_SpotLightSceneProxies)
                {
                    // For simplicity, only upload the first spot light
                    shader->UploadUniformFloat3("u_SpotLight.Position", spotLight->m_Position);
                    shader->UploadUniformFloat3("u_SpotLight.Direction", spotLight->m_Direction);
                    shader->UploadUniformFloat3("u_SpotLight.Color", spotLight->m_LightColor);
                    shader->UploadUniformFloat("u_SpotLight.InnerConeAngleCos", cos(glm::radians(spotLight->m_InnerConeAngle)));
                    shader->UploadUniformFloat("u_SpotLight.OuterConeAngleCos", cos(glm::radians(spotLight->m_OuterConeAngle)));
                    break;
                }
                

                static_cast<OpenGLVertexArrayObject*>(meshProxy->m_VertexDefinition)->Bind();

                glDrawArrays(GL_TRIANGLES, 0, 36);

                
            }
        }

        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SwapBuffers();
    }

}
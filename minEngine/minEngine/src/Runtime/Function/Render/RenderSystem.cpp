#include "RenderSystem.h"

// Include RHI implementations
#include "OpenGL/OpenGLRHI.h"
#include "GLFWWindowSystem.h"
#include "RenderCamera.h"
#include "RenderScene.h"



#include "OpenGL/OpenGLShader.h"
#include "OpenGL/OpenGLVertexArrayObject.h"
#include "OpenGL/OpenGLBuffer.h"
#include "OpenGL/OpenGLTexture.h"
#include "StaticMesh.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Render/Material.h"

#include "RuntimeGlobalContext.h"
#include "Runtime/Function/Framework/World/WorldManager.h"

#include "Runtime/Function/Render/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/StaticMeshSceneProxy.h"


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

        m_Camera->m_Position += m_Camera->m_CameraVelocity.z * m_Camera->m_Forward * deltaTime;
        m_Camera->m_Position += m_Camera->m_CameraVelocity.y * m_Camera->m_Up * deltaTime;
        m_Camera->m_Position += m_Camera->m_CameraVelocity.x * m_Camera->m_Right * deltaTime;


        for(auto& primitiveProxy : m_RenderScene->m_PrimitiveSceneProxies)
        {
            StaticMeshSceneProxy* meshProxy = dynamic_cast<StaticMeshSceneProxy*>(primitiveProxy);
            if(meshProxy)
            {
                Matrix4 model = glm::mat4(1.0f);
                model = glm::translate(model, meshProxy->m_Transform.Position);
                model = glm::rotate(model, glm::radians(meshProxy->m_Transform.Rotation.x), Vector3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(meshProxy->m_Transform.Rotation.y), Vector3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, glm::radians(meshProxy->m_Transform.Rotation.z), Vector3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, meshProxy->m_Transform.Scale);

                auto shader = meshProxy->m_Material->m_Shader;
                shader->Use();
                shader->UploadUniformInt("u_Texture1", 0);
                shader->UploadUniformFloat3("u_LigthPosition", Vector3(1.2f, 1.0f, 2.0f));
                shader->UploadUniformFloat3("u_LightColor", Vector3(1.0f, 1.0f, 1.0f));
                shader->UploadUniformMat4("u_Model", glm::value_ptr(model));
                shader->UploadUniformMat4("u_View", glm::value_ptr(m_Camera->GetViewMatrix()));
                shader->UploadUniformMat4("u_Projection", glm::value_ptr(m_Camera->GetProjectionMatrix()));
                shader->UploadUniformFloat3("u_ViewPosition", m_Camera->m_Position);

                static_cast<OpenGLVertexArrayObject*>(meshProxy->m_VertexDefinition)->Bind();
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SwapBuffers();

        // WorldManager* worldManager = RuntimeGlobalContext::GetInstance().m_WorldManager.get();
        // for(auto& component : worldManager->m_ComponentsThatNeedEndOfFrameUpdate)
        // {
        //     StaticMeshComponent* meshComponent = dynamic_cast<StaticMeshComponent*>(component);
        //     if(meshComponent)
        //     {
        //         Matrix4 model = glm::mat4(1.0f);
        //         model = glm::translate(model, meshComponent->m_Transform.Position);
        //         model = glm::rotate(model, glm::radians(meshComponent->m_Transform.Rotation.x), Vector3(1.0f, 0.0f, 0.0f));
        //         model = glm::rotate(model, glm::radians(meshComponent->m_Transform.Rotation.y), Vector3(0.0f, 1.0f, 0.0f));
        //         model = glm::rotate(model, glm::radians(meshComponent->m_Transform.Rotation.z), Vector3(0.0f, 0.0f, 1.0f));
        //         model = glm::scale(model, meshComponent->m_Transform.Scale);
        //         auto shader = meshComponent->GetMaterial()->m_Shader;
        //         shader->Use();
        //         shader->UploadUniformInt("u_Texture1", 0);
        //         shader->UploadUniformFloat3("u_LigthPosition", Vector3(1.2f, 1.0f, 2.0f));
        //         shader->UploadUniformFloat3("u_LightColor", Vector3(1.0f, 1.0f, 1.0f));
        //         shader->UploadUniformMat4("u_Model", glm::value_ptr(model));
        //         shader->UploadUniformMat4("u_View", glm::value_ptr(m_Camera->GetViewMatrix()));
        //         shader->UploadUniformMat4("u_Projection", glm::value_ptr(m_Camera->GetProjectionMatrix()));
        //         shader->UploadUniformFloat3("u_ViewPosition", m_Camera->m_Position);

        //         static_cast<OpenGLVertexArrayObject*>(meshComponent->GetMesh()->m_VertexDefinition.get())->Bind();
        //         glDrawArrays(GL_TRIANGLES, 0, 36);

        //     }
        // }
        // static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SwapBuffers();
    }

}
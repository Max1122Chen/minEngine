#include "RenderSystem.h"

// Include RHI implementations
#include "OpenGL/OpenGLRHI.h"
#include "GLFWWindowSystem.h"

#include "RenderCamera.h"
#include "OpenGL/OpenGLShader.h"
#include "OpenGL/OpenGLVertexArrayObject.h"
#include "OpenGL/OpenGLBuffer.h"
#include "OpenGL/OpenGLTexture.h"
#include "Mesh.h"


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
        // just a test rendering call

        // create shader
        minEngine::OpenGLShader modelShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Phong.frag");
        minEngine::OpenGLShader lightShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/light.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/light.frag");
        
        // model textures
        minEngine::OpenGLTexture texture1("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/container.jpg", 0);
        minEngine::OpenGLTexture texture2("D:/Dev/GitRepo/minEngine/minEngine/Assets/Textures/awesomeface.png", 1);
        
        // set model texture units
        modelShader.Use();
        modelShader.UploadUniformInt("u_Texture1", 0);
        modelShader.UploadUniformInt("u_Texture2", 1);


        // cube vertex data
        float modelVertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f, 0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,  0.0f, 0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,  0.0f, 0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, 0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  -1.0f, 0.0f, 0.0f,
        -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  -1.0f, 0.0f, 0.0f,

        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  1.0f, 0.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f, -0.5f,  1.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        0.5f,  0.5f,  0.5f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,  0.0f,  1.0f,  0.0f,
        };

        // light cube vertices
        float lightVertices[] = {
        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,  
         0.5f,  0.5f, -0.5f,  
         0.5f,  0.5f, -0.5f,  
        -0.5f,  0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 

        -0.5f, -0.5f,  0.5f, 
         0.5f, -0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f, 
        -0.5f, -0.5f,  0.5f, 

        -0.5f,  0.5f,  0.5f, 
        -0.5f,  0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 
        -0.5f, -0.5f, -0.5f, 
        -0.5f, -0.5f,  0.5f, 
        -0.5f,  0.5f,  0.5f, 

         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f, -0.5f,  
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  

        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,  
         0.5f, -0.5f,  0.5f,  
         0.5f, -0.5f,  0.5f,  
        -0.5f, -0.5f,  0.5f, 
        -0.5f, -0.5f, -0.5f, 

        -0.5f,  0.5f, -0.5f, 
         0.5f,  0.5f, -0.5f,  
         0.5f,  0.5f,  0.5f,  
         0.5f,  0.5f,  0.5f,  
        -0.5f,  0.5f,  0.5f, 
        -0.5f,  0.5f, -0.5f, 
    };

        // create cube
        Mesh cubeMesh(modelVertices, sizeof(modelVertices), {
            VertexElement("a_Position", VertexElementType::Float3),
            VertexElement("a_TexCoord", VertexElementType::Float2),
            VertexElement("a_Normal", VertexElementType::Float3)
        });
        
        // make transformation matrices for cube
        Vector3 cubePosition = Vector3(0.0f, 0.0f, 0.0f);
        Vector3 cubeRotation = Vector3(-45.0f, 0.0f, 0.0f);
        Matrix4 cubeModel = glm::mat4(1.0f);
        cubeModel = glm::translate(cubeModel, cubePosition);
        cubeModel = glm::rotate(cubeModel, glm::radians(cubeRotation.x), Vector3(1.0f, 0.0f, 0.0f));
        cubeModel = glm::rotate(cubeModel, glm::radians(cubeRotation.y), Vector3(0.0f, 1.0f, 0.0f));
        cubeModel = glm::rotate(cubeModel, glm::radians(cubeRotation.z), Vector3(0.0f, 0.0f, 1.0f));

        // create light cube
        Mesh lightMesh(lightVertices, sizeof(lightVertices), {
            VertexElement("a_Position", VertexElementType::Float3)
        });

        // make transformation matrices for light cube
        Vector3 lightColor = Vector3(1.0f, 1.0f, 1.0f);

        Vector3 lightPosition = Vector3(1.2f, 1.0f, 2.0f);
        Vector3 lightRotation = Vector3(0.0f, 0.0f, 0.0f);
        Matrix4 lightModel = glm::mat4(1.0f);
        lightModel = glm::translate(lightModel, lightPosition);
        lightModel = glm::rotate(lightModel, glm::radians(lightRotation.x), Vector3(1.0f, 0.0f, 0.0f));
        lightModel = glm::rotate(lightModel, glm::radians(lightRotation.y), Vector3(0.0f, 1.0f, 0.0f));
        lightModel = glm::rotate(lightModel, glm::radians(lightRotation.z), Vector3(0.0f, 0.0f, 1.0f));


        // enable depth testing
        static_cast<OpenGLRHI*>(m_RHI.get())->EnableDepthTest();

        // set clear color
        static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SetClearColor(Vector3(0.1f, 0.1f, 0.1f));

        modelShader.Use();
        modelShader.UploadUniformFloat3("u_LightColor", lightColor);
        modelShader.UploadUniformFloat3("u_LigthPosition", lightPosition);
        modelShader.UploadUniformFloat3("u_ViewPosition", m_Camera->m_Position);


        // Main loop
        while (!static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->ShouldClose())
        {
            m_Camera->m_Position += m_Camera->m_CameraVelocity.z * m_Camera->m_Forward * 0.01f;
            m_Camera->m_Position += m_Camera->m_CameraVelocity.y * m_Camera->m_Up * 0.01f;
            m_Camera->m_Position += m_Camera->m_CameraVelocity.x * m_Camera->m_Right * 0.01f;

            // Clear the window
            static_cast<OpenGLRHI*>(m_RHI.get())-> m_WindowSystem->Clear();

            // Rendering commands here
            // render the cube
            modelShader.Use();
            modelShader.UploadUniformMat4("u_Model", glm::value_ptr(cubeModel));
            modelShader.UploadUniformMat4("u_View", glm::value_ptr(m_Camera->GetViewMatrix()));
            modelShader.UploadUniformMat4("u_Projection", glm::value_ptr(m_Camera->GetProjectionMatrix()));

            static_cast<OpenGLVertexArrayObject*>(cubeMesh.m_VertexDefinition)->Bind();
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // render the light cube
            lightShader.Use();
            lightShader.UploadUniformMat4("u_Model", glm::value_ptr(lightModel));
            lightShader.UploadUniformMat4("u_View", glm::value_ptr(m_Camera->GetViewMatrix()));
            lightShader.UploadUniformMat4("u_Projection", glm::value_ptr(m_Camera->GetProjectionMatrix()));
            lightShader.UploadUniformFloat3("u_ViewPosition", m_Camera->m_Position);
            static_cast<OpenGLVertexArrayObject*>(lightMesh.m_VertexDefinition)->Bind();
            glDrawArrays(GL_TRIANGLES, 0, 36);

            
            // Swap buffers and poll events
            static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->SwapBuffers();
            static_cast<OpenGLRHI*>(m_RHI.get())->m_WindowSystem->PollEvents();
        }
    }

}
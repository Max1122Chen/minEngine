#include "RenderPipeline.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "RHI/RHIBuffers.h"
#include <glad/glad.h>

namespace minEngine
{
    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
        uint32_t width = windowSystem->GetWidth();
        uint32_t height = windowSystem->GetHeight();

        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();


        RHITextureDesc colorDesc{
                .Width = width,
                .Height = height,
                .Format = TextureFormat::RGBA8,
                .Usage = TextureUsage::Color
        };

        RHITextureDesc depthDesc{
                .Width = width,
                .Height = height,
                .Format = TextureFormat::DEPTH24STENCIL8,
                .Usage = TextureUsage::DepthStencil
        };

        m_SceneColorTexture = rhi->CreateRHITexture2D(nullptr, colorDesc, 0);
        m_SceneDepthTexture = rhi->CreateRHITexture2D(nullptr, depthDesc, 0);

        m_SceneBuffer = rhi->CreateFrameBuffer(width, height);

        m_BasePass.m_FrameBuffer = m_SceneBuffer.get();
        m_TranslucentPass.m_FrameBuffer = m_SceneBuffer.get();

        m_SceneBuffer->AttachColorBuffer(m_SceneColorTexture);
        m_SceneBuffer->AttachDepthStencilBuffer(m_SceneDepthTexture);

        // Set up PresentPass
        m_PresentPass.Initialize();
        m_PresentPass.m_SceneColorTexture = m_SceneColorTexture;
    }

    void RenderPipeline::Shutdown()
    {
    }

    void RenderPipeline::Execute()
    {
        BuildRenderQueue();
        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_TranslucentPass.m_DrawCommands = m_TranslucentQueue;

        m_SceneBuffer->Bind();  // Bind the scene framebuffer before executing the render passes
        
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);


        m_BasePass.Execute();
        m_TranslucentPass.Execute();

        m_SceneBuffer->Unbind();

        m_PresentPass.Execute();
        
    }

    void RenderPipeline::BuildRenderQueue()
    {
        m_OpaqueQueue.clear();
        m_TranslucentQueue.clear();

        RenderScene* renderScene = RenderSystem::GetRenderSystem().m_RenderScene.get();
        if(!renderScene)
        {
            return;
        }

        for(auto& primitiveProxy : renderScene->m_PrimitiveSceneProxies)
        {
            StaticMeshSceneProxy* staticMeshProxy = dynamic_cast<StaticMeshSceneProxy*>(primitiveProxy);
            if(staticMeshProxy)
            {
                MeshDrawCommand command;
                command.m_VertexBuffer = staticMeshProxy->m_VertexBuffer;
                command.m_VertexDefinition = staticMeshProxy->m_VertexDefinition;
                command.m_IndexBuffer = staticMeshProxy->m_IndexBuffer;
                command.m_Material = staticMeshProxy->m_Material;
                command.m_ModelMatrix = staticMeshProxy->m_Transform.ToMatrix(); 
                  
                if(command.m_Material->IsTranslucent())
                {
                    m_TranslucentQueue.push_back(command);
                }
                else
                {
                    m_OpaqueQueue.push_back(command);
                }
            }
        }
    }

    
}

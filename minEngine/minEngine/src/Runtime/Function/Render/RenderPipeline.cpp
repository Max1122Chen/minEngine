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

namespace minEngine
{
    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
        uint32_t width = windowSystem->GetWidth();
        uint32_t height = windowSystem->GetHeight();
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();

        m_sceneBuffer = rhi->CreateFrameBuffer(width, height);

        m_BasePass.m_FrameBuffer = m_sceneBuffer.get();
        m_TranslucentPass.m_FrameBuffer = m_sceneBuffer.get();


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

        m_sceneColorTexture = rhi->CreateRHITexture2D(nullptr, colorDesc, 0);
        m_sceneDepthTexture = rhi->CreateRHITexture2D(nullptr, depthDesc, 0);

        m_sceneBuffer->AttachColorBuffer(m_sceneColorTexture);
        m_sceneBuffer->AttachDepthStencilBuffer(m_sceneDepthTexture);
    }

    void RenderPipeline::Shutdown()
    {
    }

    void RenderPipeline::Execute()
    {
        BuildRenderQueue();
        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_TranslucentPass.m_DrawCommands = m_TranslucentQueue;

        m_BasePass.Render();
        m_TranslucentPass.Render();
        
    }

    void RenderPipeline::BuildRenderQueue()
    {
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

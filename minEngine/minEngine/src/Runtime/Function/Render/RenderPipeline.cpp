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
#include "RenderCamera.h"
#include "LightSceneProxies/DirectionalLightSceneProxy.h"
#include "LightSceneProxies/PointLightSceneProxy.h"
#include "LightSceneProxies/SpotLightSceneProxy.h"
#include <glad/glad.h>

namespace minEngine
{
    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
        uint32_t width = windowSystem->GetWidth();
        uint32_t height = windowSystem->GetHeight();

        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();

        rhi->SetClearColor(Vector4(0.1f, 0.1f, 0.1f, 1.0f));

        // Create per-frame uniform buffer
        m_PerFrameUniformBuffer = rhi->CreateUniformBuffer(sizeof(PerFrameData), 0);

        // Create light uniform buffer
        m_LightUniformBuffer = rhi->CreateUniformBuffer(sizeof(LightsData), 1); // Binding point 1 for light data

        // Create Framebuffer and its attachments
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
        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();
        
        UpdatePerFrameUBO();
        UpdateLightUBO();


        // Build render queue for this frame
        BuildRenderQueue();
        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_TranslucentPass.m_DrawCommands = m_TranslucentQueue;

        m_SceneBuffer->Bind();  // Bind the scene framebuffer before executing the render passes
        
        // Clear the framebuffer at the beginning of the render pipeline execution
        // Dont change the order
        rhi->Clear();

        m_BasePass.Execute();
        m_TranslucentPass.Execute();

        m_SceneBuffer->Unbind();

        m_PresentPass.Execute();
        
    }

    void RenderPipeline::UpdatePerFrameUBO()
    {
        // Update per-frame uniform buffer
        RenderCamera* mainCamera = RenderSystem::GetRenderSystem().GetMainCamera();
        PerFrameData perFrameData;
        perFrameData.View = mainCamera->GetViewMatrix();
        perFrameData.Proj = mainCamera->GetProjectionMatrix();
        perFrameData.ViewProj = perFrameData.Proj * perFrameData.View;
        perFrameData.CameraPos = Vector4(mainCamera->m_Position, 1.0f);
        m_PerFrameUniformBuffer->UpdateData(&perFrameData, 0, sizeof(PerFrameData));
        m_PerFrameUniformBuffer->BindToBindingPoint(0); // Bind the uniform buffer to the binding point for this frame
    }

    void RenderPipeline::UpdateLightUBO()
    {
        RenderScene* renderScene = RenderSystem::GetRenderSystem().m_RenderScene.get();

        // Update light uniform buffer
        LightsData lightsData;
        // memset(&lightsData, 0, sizeof(LightsData)); // Zero initialization to avoid garbage data

        // ... populate lightsData with actual light information ...

        // Support only one directional light for now, we can extend this to support multiple lights later
        if(renderScene->m_DirectionalLightSceneProxies.size() > 0)
        {
            DirectionalLightSceneProxy* dirLightProxy = renderScene->m_DirectionalLightSceneProxies[0];
            lightsData.DirectionalLight.Direction = Vector4(dirLightProxy->m_Direction, 0.0f);
            lightsData.DirectionalLight.Color = Vector4(dirLightProxy->m_LightColor, dirLightProxy->m_Intensity);
        }

        uint32_t pLightCount = 0;
        for(size_t i = 0; i < renderScene->m_PointLightSceneProxies.size() && i < RenderSystem::MAX_POINT_LIGHTS; ++i)
        {
            PointLightSceneProxy* pointLightProxy = renderScene->m_PointLightSceneProxies[i];
            lightsData.PointLights[i].Position = Vector4(pointLightProxy->m_Position, 1.0f); // w can be used for radius if needed
            lightsData.PointLights[i].Color = Vector4(pointLightProxy->m_LightColor, pointLightProxy->m_Intensity);
            pLightCount++;
        }
        lightsData.PointLightsCount = pLightCount;

        uint32_t sLightCount = 0;
        for(size_t i = 0; i < renderScene->m_SpotLightSceneProxies.size() && i < RenderSystem::MAX_SPOT_LIGHTS; ++i)
        {
            SpotLightSceneProxy* spotLightProxy = renderScene->m_SpotLightSceneProxies[i];
            lightsData.SpotLights[i].Position = Vector4(spotLightProxy->m_Position, 1.0f);
            lightsData.SpotLights[i].Direction = Vector4(spotLightProxy->m_Direction, 0.0f);
            lightsData.SpotLights[i].Color = Vector4(spotLightProxy->m_LightColor, spotLightProxy->m_Intensity);
            
            lightsData.SpotLights[i].ConeAngles = Vector4(spotLightProxy->m_InnerConeAngle, spotLightProxy->m_OuterConeAngle, 0.0f, 0.0f); // inner cone angle, outer cone angle
            sLightCount++;
        }
        lightsData.SpotLightsCount = sLightCount;


        m_LightUniformBuffer->UpdateData(&lightsData, 0, sizeof(LightsData));
        m_LightUniformBuffer->BindToBindingPoint(1); // Bind the uniform buffer to the binding point for light data
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

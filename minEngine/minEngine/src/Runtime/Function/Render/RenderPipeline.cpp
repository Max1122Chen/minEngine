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
    Matrix4 DirLightShadowEntry::CalculateLightViewProjMatrix() const
    {
        if(!LightProxy)
        {
            return Matrix4(1.0f);
        }

        // For simplicity, we will use an orthographic projection for directional light shadow mapping
        // In a real implementation, you would want to calculate the orthographic bounds based on the scene's bounding box and the light direction
        float orthoSize = 20.0f; // This should be large enough to cover the scene, you can make this configurable later
        Matrix4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);

        // The light's view matrix is calculated based on its direction and a target point (we'll use the origin for simplicity)
        Vector3 lightDir = LightProxy->m_Direction;
        Vector3 target = Vector3(0.0f); // You can calculate this based on the scene's bounding box later
        Vector3 up = Vector3(0.0f, 1.0f, 0.0f); // Y-up world

        Matrix4 lightView = glm::lookAt(-lightDir * 10.0f, target, up); // Position the light far away in the opposite direction of its direction

        return lightProj * lightView;
    }

    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
        uint32_t width = windowSystem->GetWidth();
        uint32_t height = windowSystem->GetHeight();

        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();

        rhi->SetClearColor(Vector4(0.1f, 0.1f, 0.1f, 1.0f));

        // Create uniform buffers
        m_LightViewProjUniformBuffer = rhi->CreateUniformBuffer(sizeof(Matrix4), 8); // Binding point 8 for light view projection matrix in shadow pass
        m_PerFrameUniformBuffer = rhi->CreateUniformBuffer(sizeof(PerFrameData), 0);
        m_LightUniformBuffer = rhi->CreateUniformBuffer(sizeof(LightsData), 1); // Binding point 1 for light data

        // Create framebuffers
        m_ShadowBuffer = rhi->CreateFrameBuffer(2048, 2048); // Shadow map framebuffer, we will use a fixed size for now
        m_SceneBuffer = rhi->CreateFrameBuffer(width, height);

        // Assign framebuffers to render passes
        m_ShadowPass.m_FrameBuffer = m_ShadowBuffer.get();

        m_BasePass.m_FrameBuffer = m_SceneBuffer.get();
        m_TranslucentPass.m_FrameBuffer = m_SceneBuffer.get();

        // Create SceneBuffer's attachments
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

        m_SceneBuffer->AttachColorBuffer(m_SceneColorTexture);
        m_SceneBuffer->AttachDepthStencilBuffer(m_SceneDepthTexture);

        // Set up ShadowPass
        m_ShadowPass.Initialize();
        m_ShadowPass.m_LightViewProjUniformBuffer = m_LightViewProjUniformBuffer.get();

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
        
        // Build render queue for this frame
        // Build RenderQueue before shadow entries because we need to get all opauque objects to generate shadow maps
        BuildRenderQueue();

        // Build shadow entries for this frame
        BuildShadowEntries();

        // For simplicity, we will render all opaque objects in the shadow pass.
        m_ShadowPass.m_OpaqueQueue = m_OpaqueQueue; 
        m_ShadowPass.m_DirLightShadowEntries = m_DirLightShadowEntries;

        m_ShadowBuffer->Bind();
        m_ShadowPass.Execute();

        UpdatePerFrameUBO();
        UpdateLightUBO();



        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_BasePass.m_DirLightShadowEntries = m_DirLightShadowEntries;
        m_TranslucentPass.m_DrawCommands = m_TranslucentQueue;

        // Shadow pass disables color output; restore state for scene pass.
        rhi->SetDrawBuffer(0);
        rhi->SetReadBuffer(0);
        rhi->SetViewport(0, 0, m_SceneColorTexture->GetWidth(), m_SceneColorTexture->GetHeight());
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
        LightsData lightsData{};

        // ... populate lightsData with actual light information ...

        // Support only one directional light for now, we can extend this to support multiple lights later
        if(renderScene->m_DirectionalLightSceneProxies.size() > 0)
        {
            DirectionalLightSceneProxy* dirLightProxy = renderScene->m_DirectionalLightSceneProxies[0];
            lightsData.DirectionalLight.Direction = Vector4(dirLightProxy->m_Direction, 0.0f);
            lightsData.DirectionalLight.Color = Vector4(dirLightProxy->m_LightColor, dirLightProxy->m_Intensity);
            int shadowMapIndex = m_DirLightShadowEntries.size() > 0 ? 0 : -1; // If we have generated a shadow map for this directional light, set the index to 0, otherwise set it to -1
            lightsData.DirectionalLight.Params = Vector4(0.0f, 0.0f, 0.0f, static_cast<float>(shadowMapIndex));
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
            
            lightsData.SpotLights[i].Params = Vector4(spotLightProxy->m_InnerConeAngle, spotLightProxy->m_OuterConeAngle, 0.0f, 0.0f); // inner cone angle, outer cone angle
            sLightCount++;
        }
        lightsData.SpotLightsCount = sLightCount;


        m_LightUniformBuffer->UpdateData(&lightsData, 0, sizeof(LightsData));
        m_LightUniformBuffer->BindToBindingPoint(1); // Bind the uniform buffer to the binding point for light data
    }

    void RenderPipeline::BuildShadowEntries()
    {
        // Clear previous shadow entries
        m_DirLightShadowEntries.clear();

        for(auto& dirLightProxy : RenderSystem::GetRenderSystem().m_RenderScene->m_DirectionalLightSceneProxies)
        {
            if(dirLightProxy->m_CastsShadow)
            {
                DirLightShadowEntry shadowEntry;
                shadowEntry.LightProxy = dirLightProxy;
                shadowEntry.Resolution = 2048; // TODO: make this configurable later
                shadowEntry.LightViewProjMatrix = shadowEntry.CalculateLightViewProjMatrix();

                // Create shadow map for this directional light
                RHITextureDesc shadowMapDesc{
                    .Width = shadowEntry.Resolution,
                    .Height = shadowEntry.Resolution,
                    .Format = TextureFormat::DEPTH32,
                    .Usage = TextureUsage::Depth
                };
                auto shadowMap = RenderSystem::GetRenderSystem().GetRHI()->CreateRHITexture2D(nullptr, shadowMapDesc, 8); // set the texture unit to 8 for shadow map in shadow pass shader
                shadowEntry.CascadeShadowMaps.push_back(shadowMap);

                m_DirLightShadowEntries.push_back(shadowEntry);
            }
        }
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
                command.m_CastShadow = staticMeshProxy->m_CastShadow;
                  
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

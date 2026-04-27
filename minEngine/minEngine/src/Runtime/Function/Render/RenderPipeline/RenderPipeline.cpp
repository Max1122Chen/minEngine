#include "RenderPipeline.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Render/WindowSystem.h"
#include "Render/RenderSystem.h"
#include "Render/RenderScene.h"
#include "Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Render/DrawCommands/MeshDrawCommand.h"
#include "Render/Material.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RenderCamera.h"
#include "Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Render/LightSceneProxies/SpotLightSceneProxy.h"
#include <glad/glad.h>

namespace minEngine
{
    namespace
    {
        Matrix4 CalculateDirectionalLightViewProjMatrix(const DirectionalLightSceneProxy* lightProxy)
        {
            if(!lightProxy)
            {
                return Matrix4(1.0f);
            }

            // For simplicity, we will use an orthographic projection for directional light shadow mapping.
            float orthoSize = 10.0f; // TODO: make this configurable later.
            Matrix4 lightProj = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 0.1f, 100.0f);

            // The light's view matrix is calculated based on its direction and a target point.
            Vector3 lightDir = Vector3(lightProxy->m_Direction.z, lightProxy->m_Direction.y, -lightProxy->m_Direction.x); // Light direction points from the scene to the light
            Vector3 up = Vector3(0.0f, 1.0f, 0.0f);

            Matrix4 lightView = glm::lookAt(-lightDir * 10.0f, lightDir, up);

            return lightProj * lightView;
        }
    }

    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::Get().m_WindowSystem.get();

        // TODO: When render in Editor, we will use a resolution that may not same as the window's size
        // When render in the game, we should use window's size as the scene color buffer's resolution.
        // Now we just hardcode it as 1080p for simplicity.
        uint32_t width = 1920; // windowSystem->GetWidth();
        uint32_t height = 1080; // windowSystem->GetHeight();

        RHI* rhi = RenderSystem::Get().GetRHI();
        m_ShadowResourceManager.Initialize(rhi);
        m_FrameIndex = 0;

        rhi->SetClearColor(Vector4(0.1f, 0.1f, 0.1f, 1.0f));

        // Create uniform buffers
        m_LightViewProjUniformBuffer = rhi->CreateUniformBuffer(sizeof(Matrix4), 8); // Binding point 8 for light view projection matrix in shadow pass
        m_PerFrameUniformBuffer = rhi->CreateUniformBuffer(sizeof(PerFrameData), 0);
        m_LightUniformBuffer = rhi->CreateUniformBuffer(sizeof(LightsData), 1); // Binding point 1 for light data

        // Create framebuffers
        m_ShadowBuffer = rhi->CreateFrameBuffer(512, 512); // Shadow map framebuffer, we will use a fixed size for now

        // Assign framebuffers to render passes
        m_ShadowPass.m_FrameBuffer = m_ShadowBuffer.get();
        ResizeSceneTargets(width, height);

        // No set up needed for BasePass and TranslucentPass for now, we will set their resources in Execute() when we have the actual data.

        // Set up ShadowPass
        m_ShadowPass.Initialize();
        m_ShadowPass.m_LightViewProjUniformBuffer = m_LightViewProjUniformBuffer.get();

        // Prepare a screen quad for post-processing and presenting
        float quadVertices[] = {
        // pos      // uv
        -1, -1,     0, 0,
        1, -1,     1, 0,
        1,  1,     1, 1,

        -1, -1,     0, 0,
        1,  1,     1, 1,
        -1,  1,     0, 1
        };
        std::shared_ptr<minEngine::VertexBuffer> screenQuadVertexBuffer = rhi->CreateVertexBuffer(quadVertices, sizeof(quadVertices), 6);
        std::shared_ptr<minEngine::VertexDefinition> screenQuadVertexDefinition = rhi->CreateVertexDefinition({
            { "a_Position", VertexElementType::Float2, false },
            { "a_TexCoord", VertexElementType::Float2, false }
        });

        // Set up PostProcessPasses
        
        // Add a FXAA post-process pass first, we can add more post-process passes later if needed.
        std::shared_ptr<RHIShader> FXAAShader = rhi->CreateRHIShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Present.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/FXAA.frag");
        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_SceneColorTexture = m_SceneColorTexture;
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = screenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexDefinition = screenQuadVertexDefinition;
        m_PostProcessPasses.back().m_PostProcessShader = FXAAShader;

        // Add a sharpen pass after FXAA
        std::shared_ptr<RHIShader> SharpenShader = rhi->CreateRHIShader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/Present.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/Sharpen.frag");
        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_SceneColorTexture = m_SceneColorTexture;
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = screenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexDefinition = screenQuadVertexDefinition;
        m_PostProcessPasses.back().m_PostProcessShader = SharpenShader;

        // Set up PresentPass
        m_PresentPass.Initialize();
        m_PresentPass.m_SceneColorTexture = m_SceneColorTexture;
        m_PresentPass.m_ScreenQuadVertexBuffer = screenQuadVertexBuffer;
        m_PresentPass.m_ScreenQuadVertexDefinition = screenQuadVertexDefinition;
    }

    void RenderPipeline::ResizeSceneTargets(uint32_t width, uint32_t height)
    {
        if (width == 0 || height == 0)
        {
            return;
        }

        if (m_SceneColorTexture && m_SceneColorTexture->GetWidth() == width && m_SceneColorTexture->GetHeight() == height)
        {
            return;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        m_SceneBuffer = rhi->CreateFrameBuffer(width, height);
        m_SceneBufferWidth = width;
        m_SceneBufferHeight = height;

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

        m_SceneColorTexture = rhi->CreateRHITexture2D(nullptr, colorDesc);
        m_SceneDepthTexture = rhi->CreateRHITexture2D(nullptr, depthDesc);

        m_SceneBuffer->AttachColorBuffer(m_SceneColorTexture);
        m_SceneBuffer->AttachDepthStencilBuffer(m_SceneDepthTexture);

        m_BasePass.m_FrameBuffer = m_SceneBuffer.get();
        m_TranslucentPass.m_FrameBuffer = m_SceneBuffer.get();
        for(auto& postProcessPass : m_PostProcessPasses)
        {
            postProcessPass.m_SceneColorTexture = m_SceneColorTexture;
        }
        m_PresentPass.m_SceneColorTexture = m_SceneColorTexture;

        // ME_CORE_INFO("Resize scene render targets to {}x{}", width, height);
    }

    void RenderPipeline::Shutdown()
    {
        m_ShadowResourceManager.Shutdown();

        m_OpaqueQueue.clear();
        m_TranslucentQueue.clear();

        m_ShadowRequests.clear();
        m_ShadowDrawCommands.clear();

        m_DirectionalShadowHandle = ShadowResourceHandle{};
        m_DirectionalLightViewProj = Matrix4(1.0f);

        m_ShadowPass.m_ShadowDrawCommands.clear();
        m_ShadowPass.m_DirectionalShadowArray.reset();
        m_ShadowPass.m_OpaqueQueue.clear();

        m_BasePass.m_DirectionalShadowArray.reset();
        m_BasePass.m_DirectionalShadowHandle = ShadowResourceHandle{};
        m_BasePass.m_DirectionalLightViewProj = Matrix4(1.0f);

        m_PresentPass.m_SceneColorTexture.reset();
        m_ShadowPass.m_LightViewProjUniformBuffer = nullptr;

        m_ShadowPass.m_FrameBuffer = nullptr;
        m_BasePass.m_FrameBuffer = nullptr;
        m_TranslucentPass.m_FrameBuffer = nullptr;

        m_SceneDepthTexture.reset();
        m_SceneColorTexture.reset();
        m_SceneBuffer.reset();
        m_ShadowBuffer.reset();

        m_LightUniformBuffer.reset();
        m_PerFrameUniformBuffer.reset();
        m_LightViewProjUniformBuffer.reset();
    }

    void RenderPipeline::Execute()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        if (!m_ShadowBuffer || !m_SceneBuffer || !m_SceneColorTexture)
        {
            ME_CORE_ERROR("RenderPipeline resources are not ready");
            return;
        }

        m_ShadowResourceManager.BeginFrame(m_FrameIndex);

        if (RenderSystem::Get().m_RenderScene)
        {
            RenderSystem::Get().m_RenderScene->CollectOrphanedSceneProxies();
        }
        
        // Build render queue for this frame
        // Build RenderQueue before shadow entries because we need to get all opauque objects to generate shadow maps
        BuildRenderQueue();

        // Build shadow requests and draw commands for this frame.
        CollectShadowRequests();
        BuildShadowDrawCommands();

        // For simplicity, we will render all opaque objects in the shadow pass.
        m_ShadowPass.m_OpaqueQueue = m_OpaqueQueue; 
        m_ShadowPass.m_ShadowDrawCommands = m_ShadowDrawCommands;
        m_ShadowPass.m_DirectionalShadowArray = m_ShadowResourceManager.GetDirectionalShadowArray();

        m_ShadowBuffer->Bind();
        m_ShadowPass.Execute();

        UpdatePerFrameUBO();
        UpdateLightUBO();



        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_BasePass.m_DirectionalShadowArray = m_ShadowResourceManager.GetDirectionalShadowArray();
        m_BasePass.m_DirectionalShadowHandle = m_DirectionalShadowHandle;
        m_BasePass.m_DirectionalLightViewProj = m_DirectionalLightViewProj;
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

        for(auto& postProcessPass : m_PostProcessPasses)
        {
            postProcessPass.Execute();
        }

        m_SceneBuffer->Unbind();

        if (m_EnablePresentPass)
        {
            m_PresentPass.Execute();
        }

        m_ShadowResourceManager.EndFrame();
        ++m_FrameIndex;
        
    }

    void RenderPipeline::UpdatePerFrameUBO()
    {
        // Update per-frame uniform buffer
        RenderCamera* mainCamera = RenderSystem::Get().GetMainCamera();
        if (!mainCamera || !m_PerFrameUniformBuffer)
        {
            return;
        }

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
        RenderScene* renderScene = RenderSystem::Get().m_RenderScene.get();
        if (!renderScene || !m_LightUniformBuffer)
        {
            return;
        }

        // Update light uniform buffer
        LightsData lightsData{};

        // ... populate lightsData with actual light information ...

        // Support only one directional light for now, we can extend this to support multiple lights later
        DirectionalLightSceneProxy* firstValidDirectionalLight = nullptr;
        for (auto* dirLightProxy : renderScene->m_DirectionalLightSceneProxies)
        {
            if (dirLightProxy && dirLightProxy->m_LightComponent)
            {
                firstValidDirectionalLight = dirLightProxy;
                break;
            }
        }

        if(firstValidDirectionalLight)
        {
            DirectionalLightSceneProxy* dirLightProxy = firstValidDirectionalLight;
            // Convert light direction to render space. Ref: RenderCamera.
            Vector4 lightRenderDir = Vector4(dirLightProxy->m_Direction.z, dirLightProxy->m_Direction.y, -dirLightProxy->m_Direction.x, 0.0f);
            lightsData.DirectionalLight.Direction = lightRenderDir;
            lightsData.DirectionalLight.Color = Vector4(dirLightProxy->m_LightColor, dirLightProxy->m_Intensity);
            int shadowMapIndex = -1;
            if (m_DirectionalShadowHandle.Valid)
            {
                shadowMapIndex = m_DirectionalShadowHandle.ArrayBaseLayer;
            }
            lightsData.DirectionalLight.Params = Vector4(0.0f, 0.0f, 0.0f, static_cast<float>(shadowMapIndex));
        }

        uint32_t pointLightCount = 0;
        for(size_t i = 0; i < renderScene->m_PointLightSceneProxies.size() && i < RenderSystem::MAX_POINT_LIGHTS; ++i)
        {
            PointLightSceneProxy* pointLightProxy = renderScene->m_PointLightSceneProxies[i];
            if (!pointLightProxy || !pointLightProxy->m_LightComponent)
            {
                continue;
            }
            Vector4 lightRenderPos = Vector4(pointLightProxy->m_Position.z, pointLightProxy->m_Position.y, -pointLightProxy->m_Position.x, 1.0f); // Convert to render space
            lightsData.PointLights[pointLightCount].Position = lightRenderPos;
            lightsData.PointLights[pointLightCount].Color = Vector4(pointLightProxy->m_LightColor, pointLightProxy->m_Intensity);
            pointLightCount++;
        }
        lightsData.PointLightsCount = pointLightCount;

        uint32_t spotLightCount = 0;
        for(size_t i = 0; i < renderScene->m_SpotLightSceneProxies.size() && i < RenderSystem::MAX_SPOT_LIGHTS; ++i)
        {
            SpotLightSceneProxy* spotLightProxy = renderScene->m_SpotLightSceneProxies[i];
            if (!spotLightProxy || !spotLightProxy->m_LightComponent)
            {
                continue;
            }
            Vector4 lightRenderPos = Vector4(spotLightProxy->m_Position.z, spotLightProxy->m_Position.y, -spotLightProxy->m_Position.x, 1.0f); // Convert to render space
            Vector4 lightRenderDir = Vector4(spotLightProxy->m_Direction.z, spotLightProxy->m_Direction.y, -spotLightProxy->m_Direction.x, 0.0f); // Convert to render space
            lightsData.SpotLights[spotLightCount].Position = lightRenderPos;
            lightsData.SpotLights[spotLightCount].Direction = lightRenderDir;
            lightsData.SpotLights[spotLightCount].Color = Vector4(spotLightProxy->m_LightColor, spotLightProxy->m_Intensity);
            
            lightsData.SpotLights[spotLightCount].Params = Vector4(spotLightProxy->m_InnerConeAngle, spotLightProxy->m_OuterConeAngle, 0.0f, 0.0f); // inner cone angle, outer cone angle
            spotLightCount++;
        }
        lightsData.SpotLightsCount = spotLightCount;


        m_LightUniformBuffer->UpdateData(&lightsData, 0, sizeof(LightsData));
        m_LightUniformBuffer->BindToBindingPoint(1); // Bind the uniform buffer to the binding point for light data
    }

    void RenderPipeline::CollectShadowRequests()
    {
        m_ShadowRequests.clear();

        RenderScene* renderScene = RenderSystem::Get().m_RenderScene.get();
        if (!renderScene)
        {
            return;
        }

        for (auto* dirLightProxy : renderScene->m_DirectionalLightSceneProxies)
        {
            if (!dirLightProxy || !dirLightProxy->m_LightComponent)
            {
                continue;
            }

            if (!dirLightProxy->m_CastsShadow)
            {
                continue;
            }

            ShadowRequest shadowRequest{};
            shadowRequest.Key.Type = LightType::Directional;
            shadowRequest.Key.LightProxyPtr = dirLightProxy;
            shadowRequest.Resolution = ShadowResolution{    // Currently we use a fixed shadow map resolution for simplicity, we can make this configurable later.
                .Width = 512,
                .Height = 512
            };
            shadowRequest.Priority = 0;
            m_ShadowRequests.push_back(shadowRequest);
        }
    }

    void RenderPipeline::BuildShadowDrawCommands()
    {
        m_ShadowDrawCommands.clear();
        m_DirectionalShadowHandle = ShadowResourceHandle{};
        m_DirectionalLightViewProj = Matrix4(1.0f);

        constexpr uint32_t kDirectionalCascadeCount = 1;

        for (const auto& shadowRequest : m_ShadowRequests)
        {
            if (shadowRequest.Key.Type != LightType::Directional)
            {
                continue;
            }

            auto* dirLightProxy = static_cast<DirectionalLightSceneProxy*>(
                const_cast<void*>(shadowRequest.Key.LightProxyPtr));
            if (!dirLightProxy)
            {
                continue;
            }

            // Current lighting path only consumes one directional shadow.
            if (m_DirectionalShadowHandle.Valid)
            {
                continue;
            }

            ShadowResourceHandle handle = m_ShadowResourceManager.AcquireDirectional(shadowRequest, kDirectionalCascadeCount);
            if (!handle.Valid)
            {
                continue;
            }

            ShadowDrawCommand shadowDrawCommand{};
            shadowDrawCommand.Type = LightType::Directional;
            shadowDrawCommand.Handle = handle;
            shadowDrawCommand.ViewProj = CalculateDirectionalLightViewProjMatrix(dirLightProxy);
            shadowDrawCommand.TargetLayer = handle.ArrayBaseLayer;
            shadowDrawCommand.TargetFace = -1;

            m_ShadowDrawCommands.push_back(shadowDrawCommand);
            m_DirectionalShadowHandle = handle;
            m_DirectionalLightViewProj = shadowDrawCommand.ViewProj;
        }
    }

    void RenderPipeline::BuildRenderQueue()
    {
        m_OpaqueQueue.clear();
        m_TranslucentQueue.clear();

        RenderScene* renderScene = RenderSystem::Get().m_RenderScene.get();
        if(!renderScene)
        {
            return;
        }

        for(auto& primitiveProxy : renderScene->m_PrimitiveSceneProxies)
        {
            if (!primitiveProxy || !primitiveProxy->m_PrimitiveComponent)
            {
                continue;
            }

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

                if (!command.m_Material || !command.m_VertexDefinition || !command.m_VertexBuffer)
                {
                    continue;
                }
                  
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

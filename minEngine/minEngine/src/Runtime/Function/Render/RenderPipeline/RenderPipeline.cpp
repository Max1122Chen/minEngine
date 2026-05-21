#include "RenderPipeline.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Render/WindowSystem.h"
#include "Render/RenderSystem.h"
#include "Render/RenderScene.h"
#include "Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Render/DrawCommands/MeshDrawCommand.h"
#include "Render/Material.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/Shader.h"
#include "Render/RenderCamera.h"
#include "Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Math/Geometry/AABB.h"
#include <glad/glad.h>

namespace
{
    constexpr float kSpotShadowNear = 0.1f;
    constexpr float kSpotShadowFar = 50.0f;
    constexpr float kPointShadowNear = 0.1f;
    constexpr float kPointShadowFar = 50.0f;
}

namespace minEngine
{
    void RenderPipeline::Initialize()
    {
        WindowSystem* windowSystem = &WindowSystem::Get();

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
        m_LightDataUniformBuffer = rhi->CreateUniformBuffer(sizeof(LightsData), 1); // Binding point 1 for light data

        m_DirLightViewProjUniformBuffer = rhi->CreateUniformBuffer(sizeof(Matrix4) * MAX_CASCADES, 9); // Binding point 9 for directional light view projection matrix in base pass for CSM
        m_CascadeFarPlaneUniformBuffer = rhi->CreateUniformBuffer(sizeof(float) * 4 * MAX_CASCADES, 10); // Binding point 10 for CSM cascade far plane distances in base pass for CSM
        m_SpotLightViewProjUniformBuffer = rhi->CreateUniformBuffer(sizeof(Matrix4) * MAX_SPOT_LIGHTS, 11); // Binding point 11 for spot light view projection matrices in base pass

        // Create framebuffers
        m_ShadowBuffer = rhi->CreateFrameBuffer(512, 512); // Shadow map framebuffer, we will use a fixed size for now

        // Set up BasePass

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
        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_SceneColorTexture = m_SceneColorTexture;
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = screenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexDefinition = screenQuadVertexDefinition;
        if (std::shared_ptr<Shader> fxaaShader = Shader::CreateFromFiles(
                *rhi,
                Shader::EngineShaderPath("Present.vert"),
                Shader::EngineShaderPath("FXAA.frag")))
        {
            m_PostProcessPasses.back().m_PostProcessShader = fxaaShader->GetRHIShader();
        }

        // Add a sharpen pass after FXAA
        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_SceneColorTexture = m_SceneColorTexture;
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = screenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexDefinition = screenQuadVertexDefinition;
        if (std::shared_ptr<Shader> sharpenShader = Shader::CreateFromFiles(
                *rhi,
                Shader::EngineShaderPath("Present.vert"),
                Shader::EngineShaderPath("Sharpen.frag")))
        {
            m_PostProcessPasses.back().m_PostProcessShader = sharpenShader->GetRHIShader();
        }

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

        m_ShadowPass.m_ShadowDrawCommands.clear();
        m_ShadowPass.m_OpaqueQueue.clear();

        m_BasePass.m_DirectionalShadowHandle = ShadowResourceHandle{};

        m_PresentPass.m_SceneColorTexture.reset();
        m_ShadowPass.m_LightViewProjUniformBuffer = nullptr;

        m_ShadowPass.m_FrameBuffer = nullptr;
        m_BasePass.m_FrameBuffer = nullptr;
        m_TranslucentPass.m_FrameBuffer = nullptr;

        m_SceneDepthTexture.reset();
        m_SceneColorTexture.reset();
        m_SceneBuffer.reset();
        m_ShadowBuffer.reset();

        m_LightDataUniformBuffer.reset();
        m_PerFrameUniformBuffer.reset();
        m_LightViewProjUniformBuffer.reset();
        m_SpotLightViewProjUniformBuffer.reset();
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

        m_ShadowBuffer->Bind();
        m_ShadowPass.Execute();

        UpdatePerFrameUBO();
        UpdateLightUBO();



        m_BasePass.m_DrawCommands = m_OpaqueQueue;
        m_BasePass.m_DirectionalShadowHandle = m_DirectionalShadowHandle;
        m_BasePass.m_SpotShadowHandles = m_SpotShadowHandles;
        m_BasePass.m_PointShadowHandles = m_PointShadowHandles;
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
        perFrameData.ViewProj = mainCamera->GetViewProjMatrix();
        perFrameData.CameraPos = Vector4(mainCamera->m_Position, 1.0f);
        m_PerFrameUniformBuffer->UpdateData(&perFrameData, 0, sizeof(PerFrameData));
        m_PerFrameUniformBuffer->BindToBindingPoint(0); // Bind the uniform buffer to the binding point for this frame
    }

    void RenderPipeline::UpdateLightUBO()
    {
        RenderScene* renderScene = RenderSystem::Get().m_RenderScene.get();
        if (!renderScene || !m_LightDataUniformBuffer)
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
            lightsData.DirectionalLight.Direction = Vector4(dirLightProxy->m_Direction, 0.0f);
            lightsData.DirectionalLight.Color = Vector4(dirLightProxy->m_LightColor, dirLightProxy->m_Intensity);
            int shadowMapIndex = -1;
            if (m_DirectionalShadowHandle.IsValid())
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
            lightsData.PointLights[pointLightCount].Position = Vector4(pointLightProxy->m_Position, 1.0f);
            lightsData.PointLights[pointLightCount].Color = Vector4(pointLightProxy->m_LightColor, pointLightProxy->m_Intensity);
            int shadowIndex = -1;
            auto pointShadowIt = m_PointShadowHandleMap.find(pointLightProxy);
            if (pointShadowIt != m_PointShadowHandleMap.end() && pointShadowIt->second.IsValid())
            {
                shadowIndex = pointShadowIt->second.TextureUnit - POINT_SHADOW_MAP_BASE_UNIT;
                if (shadowIndex < 0 || shadowIndex >= MAX_POINT_SHADOW_MAPS)
                {
                    shadowIndex = -1;
                }
            }
            lightsData.PointLights[pointLightCount].Params = Vector4(0.0f, 0.0f, kPointShadowFar, static_cast<float>(shadowIndex));
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
            lightsData.SpotLights[spotLightCount].Position = Vector4(spotLightProxy->m_Position, 1.0f);
            lightsData.SpotLights[spotLightCount].Direction = Vector4(spotLightProxy->m_Direction, 0.0f);
            lightsData.SpotLights[spotLightCount].Color = Vector4(spotLightProxy->m_LightColor, spotLightProxy->m_Intensity);
            int shadowIndex = -1;
            auto spotShadowIt = m_SpotShadowHandleMap.find(spotLightProxy);
            if (spotShadowIt != m_SpotShadowHandleMap.end() && spotShadowIt->second.IsValid())
            {
                shadowIndex = spotShadowIt->second.TextureUnit - SPOT_SHADOW_MAP_BASE_UNIT;
                if (shadowIndex < 0 || shadowIndex >= MAX_SPOT_SHADOW_MAPS)
                {
                    shadowIndex = -1;
                }
            }
            lightsData.SpotLights[spotLightCount].Params = Vector4(spotLightProxy->m_InnerConeAngle, spotLightProxy->m_OuterConeAngle, 0.0f, static_cast<float>(shadowIndex)); // inner cone angle, outer cone angle
            spotLightCount++;
        }
        lightsData.SpotLightsCount = spotLightCount;


        m_LightDataUniformBuffer->UpdateData(&lightsData, 0, sizeof(LightsData));
        m_LightDataUniformBuffer->BindToBindingPoint(1); // Bind the uniform buffer to the binding point for light data
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
            if (!dirLightProxy || !dirLightProxy->m_LightComponent || !dirLightProxy->m_CastsShadow)
            {
                continue;
            }

            ShadowRequest shadowRequest{};
            shadowRequest.Type = LightType::Directional;
            shadowRequest.LightProxy = dirLightProxy;
            shadowRequest.Resolution = ShadowResolution{    // Currently we use a fixed shadow map resolution for simplicity, we can make this configurable later.
                .Width = m_ShadowBuffer->GetWidth(),
                .Height = m_ShadowBuffer->GetHeight()
            };
            shadowRequest.Priority = 0;
            m_ShadowRequests.push_back(shadowRequest);
        }

        uint32_t spotShadowCount = 0;
        for (auto* spotLightProxy : renderScene->m_SpotLightSceneProxies)
        {
            if (!spotLightProxy || !spotLightProxy->m_LightComponent || !spotLightProxy->m_CastsShadow)
            {
                continue;
            }

            if (spotShadowCount >= MAX_SPOT_SHADOW_MAPS)
            {
                break;
            }

            ShadowRequest shadowRequest{};
            shadowRequest.Type = LightType::Spot;
            shadowRequest.LightProxy = spotLightProxy;
            shadowRequest.Resolution = ShadowResolution{
                .Width = m_ShadowBuffer->GetWidth(),
                .Height = m_ShadowBuffer->GetHeight()
            };
            shadowRequest.Priority = 0;
            m_ShadowRequests.push_back(shadowRequest);
            spotShadowCount++;
        }

        uint32_t pointShadowCount = 0;
        for (auto* pointLightProxy : renderScene->m_PointLightSceneProxies)
        {
            if (!pointLightProxy || !pointLightProxy->m_LightComponent || !pointLightProxy->m_CastsShadow)
            {
                continue;
            }

            if (pointShadowCount >= MAX_POINT_SHADOW_MAPS)
            {
                break;
            }

            ShadowRequest shadowRequest{};
            shadowRequest.Type = LightType::Point;
            shadowRequest.LightProxy = pointLightProxy;
            shadowRequest.Resolution = ShadowResolution{
                .Width = m_ShadowBuffer->GetWidth(),
                .Height = m_ShadowBuffer->GetHeight()
            };
            shadowRequest.Priority = 0;
            m_ShadowRequests.push_back(shadowRequest);
            pointShadowCount++;
        }
    }

    void RenderPipeline::BuildShadowDrawCommands()
    {
        m_ShadowDrawCommands.clear();
        m_DirectionalShadowHandle = ShadowResourceHandle{};
        m_SpotShadowHandles.clear();
        m_PointShadowHandles.clear();
        m_SpotShadowHandleMap.clear();
        m_PointShadowHandleMap.clear();

        int directionalLightCount = 0;
        int spotLightCount = 0;
        int pointLightCount = 0;
        for (const auto& shadowRequest : m_ShadowRequests)
        {
            if (shadowRequest.Type == LightType::Directional)
            {
                auto dirLightProxy = static_cast<DirectionalLightSceneProxy*>(shadowRequest.LightProxy);
                ShadowResourceHandle handle = m_ShadowResourceManager.AcquireDirectional(shadowRequest, MAX_CASCADES);
                if (!dirLightProxy || !handle.IsValid())
                {
                    continue;
                }

                DirShadowCommandBuildResult result = BuildDirectionalShadowDrawCommands(shadowRequest, handle, dirLightProxy, MAX_CASCADES);
                m_ShadowDrawCommands.insert(m_ShadowDrawCommands.end(), result.Commands.begin(), result.Commands.end());
                m_DirectionalShadowHandle = handle;
                directionalLightCount++;
                // Update the directional light view projection matrix for CSM in the base pass uniform buffer
                for(int i = 0; i < MAX_CASCADES; i++)
                {
                    auto& command = result.Commands[i];
                    m_CascadeFarPlaneUniformBuffer->UpdateData(&result.CascadeFarPlaneVS[i], sizeof(float) * 4 * i, sizeof(float));
                    m_DirLightViewProjUniformBuffer->UpdateData(&command.ViewProj, sizeof(Matrix4) * i, sizeof(Matrix4));
                }
            }
            else if (shadowRequest.Type == LightType::Spot)
            {
                auto spotLightProxy = static_cast<SpotLightSceneProxy*>(shadowRequest.LightProxy);
                ShadowResourceHandle handle = m_ShadowResourceManager.AcquireSpot(shadowRequest);
                if (!spotLightProxy || !handle.IsValid())                
                {
                    continue;
                }
                m_SpotShadowHandleMap[spotLightProxy] = handle;
                if (m_SpotShadowHandles.size() < MAX_SPOT_SHADOW_MAPS)
                {
                    m_SpotShadowHandles.push_back(handle);
                }
                ShadowDrawCommand command = BuildSpotShadowDrawCommand(shadowRequest, handle, spotLightProxy);
                m_ShadowDrawCommands.push_back(command);
                m_SpotLightViewProjUniformBuffer->UpdateData(&command.ViewProj, sizeof(Matrix4) * spotLightCount, sizeof(Matrix4)); // Update the spot light view projection matrix in the base pass uniform buffer
                spotLightCount++;
            }
            else if(shadowRequest.Type == LightType::Point)
            {
                auto pointLightProxy = static_cast<PointLightSceneProxy*>(shadowRequest.LightProxy);
                ShadowResourceHandle handle = m_ShadowResourceManager.AcquirePoint(shadowRequest);
                if (!pointLightProxy || !handle.IsValid())
                {
                    continue;
                }
                m_PointShadowHandleMap[pointLightProxy] = handle;
                if (m_PointShadowHandles.size() < MAX_POINT_SHADOW_MAPS)
                {
                    m_PointShadowHandles.push_back(handle);
                }
                std::vector<ShadowDrawCommand> commands = BuildPointShadowDrawCommands(shadowRequest, handle, pointLightProxy);
                m_ShadowDrawCommands.insert(m_ShadowDrawCommands.end(), commands.begin(), commands.end());
                pointLightCount++;
            }
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
                command.m_BoundingBox = staticMeshProxy->m_PrimitiveComponent->GetBoundingBox();

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

    DirShadowCommandBuildResult RenderPipeline::BuildDirectionalShadowDrawCommands(const ShadowRequest &shadowRequest, 
                                                                                     const ShadowResourceHandle &handle, 
                                                                                     const DirectionalLightSceneProxy* lightProxy, 
                                                                                     uint32_t cascadeCount)
    {
        DirShadowCommandBuildResult result;

        // === Prepare camera matrices ===
        RenderCamera* mainCamera = RenderSystem::Get().GetMainCamera();
        if(!mainCamera)
        {
            ME_CORE_ERROR("Main camera is not available when building directional shadow draw commands");
            return result;
        }
        float nearPlane = mainCamera->m_zNear;
        float farPlane = mainCamera->m_zFar;
        Matrix4 cameraViewProj = mainCamera->GetViewProjMatrix();
        Matrix4 invCameraViewProj = glm::inverse(cameraViewProj);

        // === Prepare light view matrix ===
        Vector3 lightDir = glm::normalize(lightProxy->m_Direction);
        Vector3 up = Math::abs(glm::dot(lightDir, Vector3(0.0f, 1.0f, 0.0f))) > 0.999f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);
        Matrix4 lightView = glm::lookAt(Vector3(0.0f), lightDir * 100.0f, up);

        // === Split cascade ===
        std::vector<CascadeSplit> cascadeSplits = CalculateCascadeSplits(nearPlane, farPlane, cascadeCount);
        // Calculate the far plane of each cascade in camera view space, we will need them to help determine which cascade the fragment belongs to in the Object shader.
        std::vector<float> cascadeFarPlanesVS(cascadeCount);
        for(int i = 0; i < cascadeCount; i++)
        {
            cascadeFarPlanesVS[i] = cascadeSplits[i].Far;
        }
        result.CascadeFarPlaneVS = cascadeFarPlanesVS;

        // === Split the camera frustum ===
        // An OpenGL NDC cube has corners from (-1, -1, -1) to (1, 1, 1)
        Vector4 ndcCorners[8] = {
            Vector4(-1, -1, -1, 1), // Near bottom left
            Vector4(1, -1, -1, 1),  // Near bottom right
            Vector4(1, 1, -1, 1),   // Near top right
            Vector4(-1, 1, -1, 1),  // Near top left
            Vector4(-1, -1, 1, 1), // Far bottom left
            Vector4(1, -1, 1, 1),  // Far bottom right
            Vector4(1, 1, 1, 1),   // Far top right
            Vector4(-1, 1, 1, 1)   // Far top left
        };
        // Build the whole camera frustum corners in world space
        Frustum cameraFrustumWS;    // "WS" stands for "world space"
        for(int i = 0; i < 4; i++)
        {
            Vector4 nearCorner = ndcCorners[i];
            Vector4 farCorner = ndcCorners[i + 4];
            cameraFrustumWS.Corners[i] = invCameraViewProj * nearCorner;
            cameraFrustumWS.Corners[i + 4] = invCameraViewProj * farCorner;
            cameraFrustumWS.Corners[i] /= cameraFrustumWS.Corners[i].w;
            cameraFrustumWS.Corners[i + 4] /= cameraFrustumWS.Corners[i + 4].w;
        }

        // Split the frustum corners for each cascade based on the cascade splits
        std::vector<Frustum> cascadeFrustumsWS(cascadeCount);
        for(int i = 0; i < cascadeCount; i++)
        {
            float nearSplit = cascadeSplits[i].Near;
            float farSplit = cascadeSplits[i].Far;
            for(int j = 0; j < 4; j++)
            {
                Vector4 nearCorner = cameraFrustumWS.Corners[j];
                Vector4 farCorner = cameraFrustumWS.Corners[j + 4];
                Vector4 splitFarCorner = nearCorner + (farCorner - nearCorner) * (farSplit - nearPlane) / (farPlane - nearPlane);  // Calculate the far corner of the split
                Vector4 splitNearCorner = nearCorner + (farCorner - nearCorner) * (nearSplit - nearPlane) / (farPlane - nearPlane);  // Calculate the near corner of the split
                cascadeFrustumsWS[i].Corners[j] = splitFarCorner;
                cascadeFrustumsWS[i].Corners[j + 4] = splitNearCorner;
            }
        }

        // === Calculate the light view-projection matrix for each cascade ===
        using Math::Geometry::AABB;
        std::vector<Matrix4> cascadeLightViewProjs(cascadeCount);
        for(int i = 0; i < cascadeCount; i++)
        {
            AABB aabb;
            for(int j = 0; j < 8; j++)
            {
                Vector4 cornerLS = lightView * cascadeFrustumsWS[i].Corners[j];
                aabb.Encapsulate(Vector3(cornerLS.x, cornerLS.y, cornerLS.z));
            }

            ExpandCascadeZForShadowCasters(aabb, lightView);
            
            // Texel snapping 
            Vector3 aabbSize = aabb.GetSize();
            float texelSizeX = aabbSize.x / shadowRequest.Resolution.Width;
            float texelSizeY = aabbSize.y / shadowRequest.Resolution.Height;
            Vector3 aabbCenter = aabb.GetCenter();
            // Snap the center of the bounding box to the nearest texel to reduce shimmering when the camera or light moves slightly.
            aabbCenter.x = std::floor(aabbCenter.x / texelSizeX) * texelSizeX;
            aabbCenter.y = std::floor(aabbCenter.y / texelSizeY) * texelSizeY;
            aabb.Min = aabbCenter - aabb.GetExtent();
            aabb.Max = aabbCenter + aabb.GetExtent();


            // Build the light Matrices for this cascade based on the bounding box of the split frustum in light space.
            Matrix4 lightProj = glm::ortho(aabb.Min.x, aabb.Max.x, aabb.Min.y, aabb.Max.y, -aabb.Max.z, -aabb.Min.z);
            // TODO: what's wrong with this?
            // Vector4 aabbCenterWS = glm::inverse(lightView) * Vector4(aabb.GetCenter(), 1.0f);
            // aabbCenterWS /= aabbCenterWS.w;
            // Vector3 aabbCenterWS3 = Vector3(aabbCenterWS.x, aabbCenterWS.y, aabbCenterWS.z);
            // Matrix4 cascadeLightView = glm::lookAt(aabbCenterWS3 - lightProxy->m_Direction * 100.0f, aabbCenterWS3, Vector3(0.0f, 1.0f, 0.0f));

            Matrix4 lightViewProj = lightProj * lightView;
            cascadeLightViewProjs[i] = lightViewProj;
        }

        // === Finally build shadow draw commands for each cascade layer ===
        for(uint32_t layerIndex = 0; layerIndex < cascadeCount; ++layerIndex)
        {
            ShadowDrawCommand command{};
            command.Type = LightType::Directional;
            command.Handle = handle;
            command.ViewProj = cascadeLightViewProjs[layerIndex];
            // command.ViewProj = CalculateDirectionalLightViewProjMatrix(lightProxy); // For simplicity, we use the same view projection matrix for all cascades for now, we can use the actual cascade-specific view projection matrix later.
            command.Target.TargetLayer = layerIndex;
            result.Commands.push_back(command);
        }

        return result;
    }

    ShadowDrawCommand RenderPipeline::BuildSpotShadowDrawCommand(const ShadowRequest& shadowRequest,
                                                                  const ShadowResourceHandle& handle,
                                                                  const SpotLightSceneProxy* lightProxy)
    {
        (void)shadowRequest;

        ShadowDrawCommand command{};
        command.Type = LightType::Spot;
        command.Handle = handle;
        command.Target.TargetLayer = 0;

        if (!lightProxy)
        {
            return command;
        }

        Vector3 lightPos = lightProxy->m_Position;
        Vector3 lightDir = glm::normalize(lightProxy->m_Direction);
        // Vector3 lightDir = glm::normalize(Vector3(1,-1,0));
        Vector3 up = Math::abs(glm::dot(lightDir, Vector3(0.0f, 1.0f, 0.0f))) > 0.99f ? Vector3(0.0f, 0.0f, 1.0f) : Vector3(0.0f, 1.0f, 0.0f);

        float outerAngle = glm::clamp(lightProxy->m_OuterConeAngle, 1.0f, 89.0f);
        float fov = glm::radians(glm::clamp(outerAngle * 2.0f, 1.0f, 179.0f));

        Matrix4 lightView = glm::lookAt(lightPos, lightPos + lightDir, up);
        Matrix4 lightProj = glm::perspective(fov, 1.0f, kSpotShadowNear, kSpotShadowFar);
        command.ViewProj = lightProj * lightView;

        return command;
    }

    std::vector<ShadowDrawCommand> RenderPipeline::BuildPointShadowDrawCommands(const ShadowRequest& shadowRequest,
                                                                                 const ShadowResourceHandle& handle,
                                                                                 const PointLightSceneProxy* lightProxy)
    {
        (void)shadowRequest;

        std::vector<ShadowDrawCommand> commands;
        if (!lightProxy)
        {
            return commands;
        }

        const Vector3 lightPos = lightProxy->m_Position;
        Matrix4 lightProj = glm::perspective(glm::radians(90.0f), 1.0f, kPointShadowNear, kPointShadowFar);

        const Vector3 directions[6] = {
            Vector3(1.0f, 0.0f, 0.0f),
            Vector3(-1.0f, 0.0f, 0.0f),
            Vector3(0.0f, 1.0f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, 0.0f, 1.0f),
            Vector3(0.0f, 0.0f, -1.0f)
        };

        const Vector3 ups[6] = {
            Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, 0.0f, 1.0f),
            Vector3(0.0f, 0.0f, -1.0f),
            Vector3(0.0f, -1.0f, 0.0f),
            Vector3(0.0f, -1.0f, 0.0f)
        };

        commands.reserve(6);
        for (int face = 0; face < 6; ++face)
        {
            ShadowDrawCommand command{};
            command.Type = LightType::Point;
            command.Handle = handle;
            command.Target.TargetFace = face;
            command.LightPosition = lightPos;
            command.FarPlane = kPointShadowFar;

            Matrix4 lightView = glm::lookAt(lightPos, lightPos + directions[face], ups[face]);
            command.ViewProj = lightProj * lightView;
            commands.push_back(command);
        }

        return commands;
    }

    std::vector<CascadeSplit> RenderPipeline::CalculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount)
    {
        std::vector<CascadeSplit> splits(cascadeCount);

        float lambda = 0.6f; // Cascade split factor, controls how the splits are distributed between logarithmic and linear. 0 means linear, 1 means logarithmic.

        std::vector<float> splitDepths(cascadeCount + 1);

        splitDepths[0] = nearPlane;

        for (uint32_t i = 1; i <= cascadeCount; i++)
        {
            float p = (float)i / (float)cascadeCount;

            float logSplit = nearPlane * std::pow(farPlane / nearPlane, p);
            float linSplit = nearPlane + (farPlane - nearPlane) * p;

            float split = glm::mix(linSplit, logSplit, lambda);

            splitDepths[i] = split;
        }

        // To Near / Far for each cascade layer
        for (uint32_t i = 0; i < cascadeCount; i++)
        {
            splits[i].Near = splitDepths[i];
            splits[i].Far  = splitDepths[i + 1] * ( i == cascadeCount - 1 ? 1.0f : 1.05f); // Add a small bias to the far plane of each cascade except the last one to blur the transition between cascades.
        }

        return splits;
    }

    void RenderPipeline::ExpandCascadeZForShadowCasters(Math::Geometry::AABB &frustumAABB, const Matrix4& lightView)
    {
        using Math::Geometry::AABB;
        for(const auto& command : m_OpaqueQueue)
        {
            if (command.m_CastShadow)
            {
                AABB meshAABB = command.m_BoundingBox;
                if (!meshAABB.IsValid())
                {
                    ME_CORE_WARN("Invalid mesh AABB for shadow caster, skipping it in cascade Z expansion");
                    continue;
                }
                // Calculate the bounding sphere of the mesh in world space
                Vector3 meshCenterWS = meshAABB.GetCenter();
                Vector3 meshExtentWS = meshAABB.GetExtent();
                float meshBoundingSphereRadius = glm::length(meshExtentWS);
                // Transform the mesh center to light space
                Vector4 meshCenterLS = lightView * Vector4(meshCenterWS, 1.0f);

                if (meshCenterLS.z + meshBoundingSphereRadius > frustumAABB.Max.z)
                {
                    frustumAABB.Max.z = meshCenterLS.z + meshBoundingSphereRadius;
                }
            }
        }
    }
}

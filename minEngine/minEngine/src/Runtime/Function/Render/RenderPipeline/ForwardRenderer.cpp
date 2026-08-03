#include "ForwardRenderer.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Render/WindowSystem.h"
#include "Render/RenderSystem.h"
#include "Render/SceneRenderTarget.h"
#include "Render/RenderScene.h"
#include "Render/PrimitiveSceneProxies/StaticMeshSceneProxy.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Render/DrawCommands/MeshDrawCommand.h"
#include "Render/Material.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHITexture.h"
#include "Render/EngineRHITextureUtils.h"
#include "Render/EngineSceneBindingSets.h"
#include "Render/EngineShaderBindings.h"
#include "Render/RHI/RHIBuffers.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/EngineShaderUtils.h"
#include "Render/RenderGraph/RenderGraph.h"
#include "Render/RenderGraph/RDGTypes.h"
#include "Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Render/RenderCamera.h"
#include "Render/LightSceneProxies/DirectionalLightSceneProxy.h"
#include "Render/LightSceneProxies/PointLightSceneProxy.h"
#include "Render/LightSceneProxies/SpotLightSceneProxy.h"
#include "Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"
#include "Math/Geometry/AABB.h"
#include <filesystem>

namespace
{
    std::string MakeShadowGraphPassName(size_t index)
    {
        return "Shadow." + std::to_string(index);
    }

    std::string MakeShadowDepthSlotName(size_t index)
    {
        return "ShadowDepth." + std::to_string(index);
    }

    constexpr float kSpotShadowNear = 0.1f;
    constexpr float kSpotShadowFar = 50.0f;
    constexpr float kPointShadowNear = 0.1f;
    constexpr float kPointShadowFar = 50.0f;
}

namespace minEngine
{
    void ForwardRenderer::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        m_ShadowResourceManager.Initialize();
        m_FrameIndex = 0;

        RHICommandList cmdList(rhi);
        m_LightViewProjUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(Matrix4)));
        m_PerFrameUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
        m_LightDataUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(LightsData)));
        m_PerObjectUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(Matrix4)));
        m_DirLightViewProjUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(Matrix4) * MAX_CASCADES));
        m_CascadeFarPlaneUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(float) * 4 * MAX_CASCADES));
        m_SpotLightViewProjUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(Matrix4) * MAX_SPOT_LIGHTS));

        m_SceneBindings.Initialize(cmdList);
        m_PipelineLayouts.Initialize(cmdList, m_SceneBindings);

        m_ShadowPass.pipeline = this;
        m_BasePass.pipeline = this;
        m_TranslucentPass.pipeline = this;

        m_ShadowPass.Initialize();
        m_ShadowPass.m_LightViewProjUniformBuffer = m_LightViewProjUniformBuffer.get();
        m_ShadowPass.m_PerObjectUniformBuffer = m_PerObjectUniformBuffer.get();

        float quadVertices[] = {
            -1, -1, 0, 0,
            1, -1, 1, 0,
            1, 1, 1, 1,
            -1, -1, 0, 0,
            1, 1, 1, 1,
            -1, 1, 0, 1
        };
        RHIBufferCreateDesc quadDesc;
        quadDesc.Usage = RHIBufferUsage::Vertex;
        quadDesc.ByteSize = sizeof(quadVertices);
        quadDesc.Stride = 4 * sizeof(float);
        quadDesc.ElementCount = 6;
        m_ScreenQuadVertexBuffer = cmdList.CreateBuffer(quadDesc, quadVertices);
        m_ScreenQuadVertexLayout = cmdList.CreateVertexInputLayout({
            {"a_Position", VertexElementType::Float2, false},
            {"a_TexCoord", VertexElementType::Float2, false}
        });

        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = m_ScreenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexLayout = m_ScreenQuadVertexLayout;
        if (RHIShaderRef fxaaShader = EngineShaderUtils::CreateShaderFromFiles(
                *rhi,
                EngineShaderUtils::EngineShaderPath("Present.vert"),
                EngineShaderUtils::EngineShaderPath("FXAA.frag")))
        {
            m_PostProcessPasses.back().m_PostProcessShader = std::move(fxaaShader);
        }

        // Add a sharpen pass after FXAA
        m_PostProcessPasses.emplace_back();
        m_PostProcessPasses.back().m_ScreenQuadVertexBuffer = m_ScreenQuadVertexBuffer;
        m_PostProcessPasses.back().m_ScreenQuadVertexLayout = m_ScreenQuadVertexLayout;
        if (RHIShaderRef sharpenShader = EngineShaderUtils::CreateShaderFromFiles(
                *rhi,
                EngineShaderUtils::EngineShaderPath("Present.vert"),
                EngineShaderUtils::EngineShaderPath("Sharpen.frag")))
        {
            m_PostProcessPasses.back().m_PostProcessShader = std::move(sharpenShader);
        }

        if (!m_PostProcessPasses.empty())
        {
            m_PostProcessPasses[0].SetGraphTextureNames(kRDGSceneColor, kRDGPostBufferA);
            m_PostProcessPasses[0].SetPredecessor(nullptr);
        }
        if (m_PostProcessPasses.size() > 1)
        {
            m_PostProcessPasses[1].SetGraphTextureNames(kRDGPostBufferA, kRDGSceneColor);
            m_PostProcessPasses[1].SetPredecessor(&m_PostProcessPasses[0]);
        }
        m_PresentPass.SetInputTextureName(kRDGSceneColor);

        for (PostProcessPass& postProcessPass : m_PostProcessPasses)
        {
            postProcessPass.Initialize();
        }

        m_PresentPass.Initialize();
        m_PresentPass.m_ScreenQuadVertexBuffer = m_ScreenQuadVertexBuffer;
        m_PresentPass.m_ScreenQuadVertexLayout = m_ScreenQuadVertexLayout;

        // SkyBox cubemap: LoadEngineRenderingAssets() after PathRegistry is ready (F03-M4 P0: no runtime IBL).
    }

    void ForwardRenderer::LoadEngineRenderingAssets(const std::string& engineDefaultAssetsRoot)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        m_EngineDefaultAssetsRoot = engineDefaultAssetsRoot;
        if (!engineDefaultAssetsRoot.empty())
        {
            m_SkyBoxPass.Initialize(*rhi, std::filesystem::path(engineDefaultAssetsRoot));
        }
    }

    void ForwardRenderer::BindSceneRenderTarget(SceneRenderTarget& target)
    {
        (void)target;
    }

    void ForwardRenderer::EnsurePostBufferTexture(RHI* rhi, uint32_t width, uint32_t height)
    {
        // RND-F07 Phase 1: do not allocate frame post buffers.
        (void)rhi;
        (void)width;
        (void)height;
    }

    void ForwardRenderer::BuildFrameRenderGraph(
        size_t shadowPassCount,
        bool enablePostProcess,
        bool presentToBackBuffer)
    {
        m_FrameRenderGraph.Reset();
        m_LastShadowResourceFingerprint.clear();
        m_ShadowGraphPasses.clear();
        m_ShadowGraphPassPtrs.clear();
        m_SceneSkyGraphPass = nullptr;
        m_SceneOpaqueGraphPass = nullptr;
        m_SceneTranslucentGraphPass = nullptr;
        m_PostFxaaGraphPass = nullptr;
        m_PostSharpenGraphPass = nullptr;
        m_PresentGraphPass = nullptr;

        m_ShadowGraphPasses.reserve(shadowPassCount);
        m_ShadowGraphPassPtrs.reserve(shadowPassCount);
        for (size_t shadowIndex = 0; shadowIndex < shadowPassCount; ++shadowIndex)
        {
            auto shadowGraphPass = std::make_unique<ShadowGraphPass>(m_ShadowPass);
            shadowGraphPass->SetSlotNames(
                MakeShadowGraphPassName(shadowIndex),
                MakeShadowDepthSlotName(shadowIndex));

            const std::string passName = MakeShadowGraphPassName(shadowIndex);
            RenderPass& graphPass = m_FrameRenderGraph.AddPass(passName);
            graphPass.SetImplementation(shadowGraphPass.get());
            m_FrameRenderGraph.ForceIncludePass(passName);
            m_ShadowGraphPassPtrs.push_back(&graphPass);
            m_ShadowGraphPasses.push_back(std::move(shadowGraphPass));
        }

        RenderPass& skyPass = m_FrameRenderGraph.AddPass("Scene.Sky");
        skyPass.SetImplementation(&m_SkyBoxPass);
        m_SceneSkyGraphPass = &skyPass;

        RenderPass& opaquePass = m_FrameRenderGraph.AddPass("Scene.Opaque");
        opaquePass.SetImplementation(&m_BasePass);
        m_SceneOpaqueGraphPass = &opaquePass;

        RenderPass& translucentPass = m_FrameRenderGraph.AddPass("Scene.Translucent");
        translucentPass.SetImplementation(&m_TranslucentPass);
        m_SceneTranslucentGraphPass = &translucentPass;

        if (enablePostProcess && !m_PostProcessPasses.empty())
        {
            RenderPass& fxaaPass = m_FrameRenderGraph.AddPass("Post.FXAA");
            fxaaPass.SetImplementation(&m_PostProcessPasses[0]);
            m_PostFxaaGraphPass = &fxaaPass;
        }

        if (enablePostProcess && m_PostProcessPasses.size() > 1)
        {
            RenderPass& sharpenPass = m_FrameRenderGraph.AddPass("Post.Sharpen");
            sharpenPass.SetImplementation(&m_PostProcessPasses[1]);
            m_PostSharpenGraphPass = &sharpenPass;
        }

        if (presentToBackBuffer)
        {
            RenderPass& presentPass = m_FrameRenderGraph.AddPass("Present");
            presentPass.SetImplementation(&m_PresentPass);
            m_FrameRenderGraph.ForceIncludePass("Present");
            m_PresentGraphPass = &presentPass;
        }

        m_FrameRenderGraph.SetBackbufferSource(kRDGSceneColor);
        m_ConfiguredEnablePostProcess = enablePostProcess;
        m_ConfiguredPresentToBackBuffer = presentToBackBuffer;
        m_FrameRenderGraphBuilt = true;
    }

    void ForwardRenderer::SetupFrameRenderGraph(
        RHICommandList& cmdList,
        const SceneDrawDesc& desc,
        SceneRenderContext& ctx)
    {
        SceneRenderTarget* sceneTarget = desc.RenderTarget;
        if (sceneTarget == nullptr)
        {
            return;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            return;
        }

        const uint32_t width = sceneTarget->GetWidth();
        const uint32_t height = sceneTarget->GetHeight();
        if (width == 0 || height == 0)
        {
            return;
        }

        const bool enablePostProcess = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnablePostProcess);
        const bool presentToBackBuffer =
            m_EnablePresentPass && HasSceneDrawFlag(desc.Flags, SceneDrawFlags::PresentToBackBuffer);
        const bool enableShadows = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableShadows);
        const size_t shadowPassCount = enableShadows ? ctx.ShadowDrawCommands.size() : 0;

        if (shadowPassCount != m_ConfiguredShadowGraphPassCount
            || enablePostProcess != m_ConfiguredEnablePostProcess
            || presentToBackBuffer != m_ConfiguredPresentToBackBuffer)
        {
            m_FrameRenderGraphBuilt = false;
            m_ConfiguredShadowGraphPassCount = shadowPassCount;
        }

        if (!m_FrameRenderGraphBuilt)
        {
            BuildFrameRenderGraph(shadowPassCount, enablePostProcess, presentToBackBuffer);
        }

        if (m_FrameRenderGraph.GetBackbufferWidth() != width
            || m_FrameRenderGraph.GetBackbufferHeight() != height)
        {
            m_FrameRenderGraph.SetBackbufferDimensions(width, height);
        }

        for (size_t shadowIndex = 0; shadowIndex < m_ShadowGraphPasses.size(); ++shadowIndex)
        {
            if (shadowIndex < ctx.ShadowDrawCommands.size())
            {
                m_ShadowGraphPasses[shadowIndex]->Configure(ctx.ShadowDrawCommands[shadowIndex]);
            }
            else
            {
                m_ShadowGraphPasses[shadowIndex]->ClearCommand();
            }
        }

        const std::string shadowFingerprint = BuildShadowResourceFingerprint(ctx);
        if (shadowFingerprint != m_LastShadowResourceFingerprint)
        {
            m_FrameRenderGraph.InvalidateBake();
            m_LastShadowResourceFingerprint = shadowFingerprint;
        }

        // SkyBoxPass always clears SceneColor/Depth when present in the stack (first writer).
        m_BasePass.SetClearSceneTargets(false);

        RenderGraphFrameContext frameContext;
        frameContext.DrawDesc = &desc;
        frameContext.SceneContext = &ctx;
        frameContext.Renderer = this;
        frameContext.CommandList = &cmdList;
        m_FrameRenderGraph.SetFrameContext(frameContext);

        if (!m_FrameRenderGraph.IsBaked())
        {
            m_FrameRenderGraph.Bake();
        }

        m_FrameRenderGraph.SetupAttachments(*rhi, nullptr);
    }

    void ForwardRenderer::BindGraphShadowTextures(SceneRenderContext& ctx)
    {
        for (size_t shadowIndex = 0; shadowIndex < m_ShadowGraphPasses.size(); ++shadowIndex)
        {
            ShadowGraphPass& shadowGraphPass = *m_ShadowGraphPasses[shadowIndex];
            if (shadowIndex >= ctx.ShadowDrawCommands.size())
            {
                continue;
            }

            ShadowDrawCommand& command = ctx.ShadowDrawCommands[shadowIndex];
            if (!command.Handle.IsValid() || command.GraphDepthResourceName.empty())
            {
                continue;
            }

            RDGTextureResource* depthResource =
                m_FrameRenderGraph.FindTextureResource(command.GraphDepthResourceName);
            if (depthResource == nullptr || depthResource->GetPhysicalIndex() == RDGResource::kUnused)
            {
                continue;
            }

            RHITextureRef texture = m_FrameRenderGraph.GetPhysicalTextureShared(*depthResource);
            shadowGraphPass.BindGraphTexture(texture);
            command.Handle.Texture = texture;

            if (command.Type == LightType::Directional)
            {
                ctx.DirectionalShadowHandle.Texture = texture;
            }
            else if (command.Type == LightType::Spot)
            {
                for (ShadowResourceHandle& handle : ctx.SpotShadowHandles)
                {
                    if (handle.TextureUnit == command.Handle.TextureUnit)
                    {
                        handle.Texture = texture;
                    }
                }
                for (auto& entry : ctx.SpotShadowHandleMap)
                {
                    if (entry.second.TextureUnit == command.Handle.TextureUnit)
                    {
                        entry.second.Texture = texture;
                    }
                }
            }
            else if (command.Type == LightType::Point)
            {
                for (ShadowResourceHandle& handle : ctx.PointShadowHandles)
                {
                    if (handle.TextureUnit == command.Handle.TextureUnit)
                    {
                        handle.Texture = texture;
                    }
                }
                for (auto& entry : ctx.PointShadowHandleMap)
                {
                    if (entry.second.TextureUnit == command.Handle.TextureUnit)
                    {
                        entry.second.Texture = texture;
                    }
                }
            }
        }
    }

    void ForwardRenderer::EnqueueFrameRenderGraph(RHICommandList& cmdList, SceneRenderTarget* sceneTarget)
    {
        if (sceneTarget == nullptr || !m_FrameRenderGraph.IsBaked())
        {
            return;
        }

        m_FrameRenderGraph.EnqueueRenderPasses(cmdList);

        sceneTarget->PublishGraphColorTexture(
            m_FrameRenderGraph.GetPhysicalTextureShared(m_FrameRenderGraph.GetTextureResource(kRDGSceneColor)));
        if (RDGTextureResource* depthResource = m_FrameRenderGraph.FindTextureResource(kRDGSceneDepth))
        {
            if (depthResource->GetPhysicalIndex() != RDGResource::kUnused)
            {
                sceneTarget->PublishGraphDepthTexture(
                    m_FrameRenderGraph.GetPhysicalTextureShared(*depthResource));
            }
        }
    }

    std::string ForwardRenderer::BuildShadowResourceFingerprint(const SceneRenderContext& ctx) const
    {
        std::string key;
        key.reserve(ctx.ShadowDrawCommands.size() * 32);
        for (const ShadowDrawCommand& command : ctx.ShadowDrawCommands)
        {
            key += command.GraphDepthResourceName;
            key.push_back('#');
            key += std::to_string(static_cast<uint32_t>(command.Handle.ResourceType));
            key.push_back('x');
            key += std::to_string(command.Handle.Resolution.Width);
            key.push_back('x');
            key += std::to_string(command.Handle.Resolution.Height);
            key.push_back('l');
            key += std::to_string(command.Handle.LayerCount);
            key.push_back(';');
        }
        return key;
    }

    void ForwardRenderer::Shutdown()
    {
        m_PipelineLayouts.Shutdown();
        m_SceneBindings.Shutdown();
        m_SkyBoxPass.Shutdown();
        m_ShadowResourceManager.Shutdown();

        m_ShadowPass.m_OpaqueQueue.clear();

        m_ShadowGraphPasses.clear();
        m_ShadowGraphPassPtrs.clear();
        m_ConfiguredShadowGraphPassCount = 0;
        m_FrameRenderGraph.Reset();
        m_LastShadowResourceFingerprint.clear();
        m_PostBufferTexture.reset();
        m_SceneSkyGraphPass = nullptr;
        m_SceneOpaqueGraphPass = nullptr;
        m_SceneTranslucentGraphPass = nullptr;
        m_PostFxaaGraphPass = nullptr;
        m_PostSharpenGraphPass = nullptr;
        m_PresentGraphPass = nullptr;
        m_FrameRenderGraphBuilt = false;
        m_PostBufferWidth = 0;
        m_PostBufferHeight = 0;

        m_ShadowPass.m_LightViewProjUniformBuffer = nullptr;
        m_ShadowPass.m_PerObjectUniformBuffer = nullptr;

        m_ScreenQuadVertexBuffer.reset();
        m_ScreenQuadVertexLayout.reset();

        m_LightDataUniformBuffer.reset();
        m_PerFrameUniformBuffer.reset();
        m_PerObjectUniformBuffer.reset();
        m_LightViewProjUniformBuffer.reset();
        m_DirLightViewProjUniformBuffer.reset();
        m_CascadeFarPlaneUniformBuffer.reset();
        m_SpotLightViewProjUniformBuffer.reset();
    }

    void ForwardRenderer::Execute(const SceneDrawDesc& desc)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi)
        {
            return;
        }

        if (!desc.Scene || !desc.Camera || !desc.RenderTarget)
        {
            return;
        }

        SceneRenderTarget* sceneTarget = desc.RenderTarget;
        if (sceneTarget->GetWidth() == 0 || sceneTarget->GetHeight() == 0)
        {
            return;
        }

        BindSceneRenderTarget(*sceneTarget);

        SceneRenderContext ctx;
        ctx.Scene = desc.Scene;
        ctx.Camera = desc.Camera;

        m_ShadowResourceManager.BeginFrame(m_FrameIndex);

        desc.Scene->CollectOrphanedSceneProxies();

        BuildRenderQueue(ctx);

        const bool enableShadows = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableShadows);
        if (enableShadows)
        {
            CollectShadowRequests(ctx);
            BuildShadowDrawCommands(ctx);
        }

        m_ShadowPass.m_OpaqueQueue = ctx.OpaqueQueue;

        RHICommandList cmdList(rhi);

        UpdatePerFrameUBO(ctx);
        UpdateLightUBO(ctx);

        // RND-F08: allocate graph shadow maps before Set1 samples them.
        SetupFrameRenderGraph(cmdList, desc, ctx);
        BindGraphShadowTextures(ctx);

        m_SceneBindings.BuildSceneSet0(
            cmdList,
            m_PerFrameUniformBuffer.get(),
            m_LightDataUniformBuffer.get(),
            m_PerObjectUniformBuffer.get());
        m_SceneBindings.BuildSceneSet1(
            cmdList,
            ctx,
            m_DirLightViewProjUniformBuffer.get(),
            m_CascadeFarPlaneUniformBuffer.get(),
            m_SpotLightViewProjUniformBuffer.get());

        m_BasePass.m_DrawCommands = ctx.OpaqueQueue;
        m_BasePass.m_DirectionalShadowHandle = ctx.DirectionalShadowHandle;
        m_BasePass.m_SpotShadowHandles = ctx.SpotShadowHandles;
        m_BasePass.m_PointShadowHandles = ctx.PointShadowHandles;
        m_TranslucentPass.m_SortCamera = ctx.Camera;
        m_TranslucentPass.m_DrawCommands = ctx.TranslucentQueue;
        m_TranslucentPass.m_DirectionalShadowHandle = ctx.DirectionalShadowHandle;
        m_TranslucentPass.m_SpotShadowHandles = ctx.SpotShadowHandles;
        m_TranslucentPass.m_PointShadowHandles = ctx.PointShadowHandles;

        EnqueueFrameRenderGraph(cmdList, sceneTarget);

        m_ShadowResourceManager.EndFrame();
        ++m_FrameIndex;
    }

    void ForwardRenderer::UpdatePerFrameUBO(const SceneRenderContext& ctx)
    {
        RenderCamera* mainCamera = ctx.Camera;
        if (!mainCamera || !m_PerFrameUniformBuffer)
        {
            return;
        }

        PerFrameData perFrameData;
        perFrameData.View = mainCamera->GetViewMatrix();
        perFrameData.Proj = mainCamera->GetProjectionMatrix();
        perFrameData.ViewProj = mainCamera->GetViewProjMatrix();
        perFrameData.CameraPos = Vector4(mainCamera->m_Position, 1.0f);
        m_PerFrameUniformBuffer->UpdateSubresource(&perFrameData, 0, sizeof(PerFrameData));
    }

    void ForwardRenderer::UpdateLightUBO(const SceneRenderContext& ctx)
    {
        RenderScene* renderScene = ctx.Scene;
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
            if (ctx.DirectionalShadowHandle.IsValid())
            {
                shadowMapIndex = ctx.DirectionalShadowHandle.ArrayBaseLayer;
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
            auto pointShadowIt = ctx.PointShadowHandleMap.find(pointLightProxy);
            if (pointShadowIt != ctx.PointShadowHandleMap.end() && pointShadowIt->second.IsValid())
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
            auto spotShadowIt = ctx.SpotShadowHandleMap.find(spotLightProxy);
            if (spotShadowIt != ctx.SpotShadowHandleMap.end() && spotShadowIt->second.IsValid())
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


        m_LightDataUniformBuffer->UpdateSubresource(&lightsData, 0, sizeof(LightsData));
    }

    void ForwardRenderer::CollectShadowRequests(SceneRenderContext& ctx)
    {
        ctx.ShadowRequests.clear();

        RenderScene* renderScene = ctx.Scene;
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
            shadowRequest.Resolution = ShadowResolution{
                .Width = kShadowMapResolution,
                .Height = kShadowMapResolution
            };
            shadowRequest.Priority = 0;
            ctx.ShadowRequests.push_back(shadowRequest);
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
                .Width = kShadowMapResolution,
                .Height = kShadowMapResolution
            };
            shadowRequest.Priority = 0;
            ctx.ShadowRequests.push_back(shadowRequest);
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
                .Width = kShadowMapResolution,
                .Height = kShadowMapResolution
            };
            shadowRequest.Priority = 0;
            ctx.ShadowRequests.push_back(shadowRequest);
            pointShadowCount++;
        }
    }

    void ForwardRenderer::BuildShadowDrawCommands(SceneRenderContext& ctx)
    {
        ctx.ShadowDrawCommands.clear();
        ctx.DirectionalShadowHandle = ShadowResourceHandle{};
        ctx.SpotShadowHandles.clear();
        ctx.PointShadowHandles.clear();
        ctx.SpotShadowHandleMap.clear();
        ctx.PointShadowHandleMap.clear();

        int directionalLightCount = 0;
        int spotLightCount = 0;
        int pointLightCount = 0;
        for (const auto& shadowRequest : ctx.ShadowRequests)
        {
            if (shadowRequest.Type == LightType::Directional)
            {
                auto dirLightProxy = static_cast<DirectionalLightSceneProxy*>(shadowRequest.LightProxy);
                ShadowResourceHandle handle = m_ShadowResourceManager.AcquireDirectional(shadowRequest, MAX_CASCADES);
                if (!dirLightProxy || !handle.IsValid())
                {
                    continue;
                }

                DirShadowCommandBuildResult result = BuildDirectionalShadowDrawCommands(
                    shadowRequest, handle, dirLightProxy, MAX_CASCADES, ctx.Camera, ctx.OpaqueQueue);
                ctx.ShadowDrawCommands.insert(ctx.ShadowDrawCommands.end(), result.Commands.begin(), result.Commands.end());
                ctx.DirectionalShadowHandle = handle;
                directionalLightCount++;
                // Update the directional light view projection matrix for CSM in the base pass uniform buffer
                for(int i = 0; i < MAX_CASCADES; i++)
                {
                    auto& command = result.Commands[i];
                    m_CascadeFarPlaneUniformBuffer->UpdateSubresource(
                        &result.CascadeFarPlaneVS[i],
                        sizeof(float) * 4 * static_cast<uint32_t>(i),
                        sizeof(float));
                    m_DirLightViewProjUniformBuffer->UpdateSubresource(
                        &command.ViewProj,
                        sizeof(Matrix4) * static_cast<uint32_t>(i),
                        sizeof(Matrix4));
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
                ctx.SpotShadowHandleMap[spotLightProxy] = handle;
                if (ctx.SpotShadowHandles.size() < MAX_SPOT_SHADOW_MAPS)
                {
                    ctx.SpotShadowHandles.push_back(handle);
                }
                ShadowDrawCommand command = BuildSpotShadowDrawCommand(shadowRequest, handle, spotLightProxy);
                command.GraphDepthResourceName = "SpotShadow." + std::to_string(spotLightCount);
                ctx.ShadowDrawCommands.push_back(command);
                m_SpotLightViewProjUniformBuffer->UpdateSubresource(
                    &command.ViewProj,
                    sizeof(Matrix4) * static_cast<uint32_t>(spotLightCount),
                    sizeof(Matrix4));
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
                ctx.PointShadowHandleMap[pointLightProxy] = handle;
                if (ctx.PointShadowHandles.size() < MAX_POINT_SHADOW_MAPS)
                {
                    ctx.PointShadowHandles.push_back(handle);
                }
                std::vector<ShadowDrawCommand> commands = BuildPointShadowDrawCommands(shadowRequest, handle, pointLightProxy);
                const std::string pointDepthName = "PointShadow." + std::to_string(pointLightCount);
                for (ShadowDrawCommand& command : commands)
                {
                    command.GraphDepthResourceName = pointDepthName;
                }
                ctx.ShadowDrawCommands.insert(ctx.ShadowDrawCommands.end(), commands.begin(), commands.end());
                pointLightCount++;
            }
        }
    }

    void ForwardRenderer::BuildRenderQueue(SceneRenderContext& ctx)
    {
        ctx.OpaqueQueue.clear();
        ctx.TranslucentQueue.clear();

        RenderScene* renderScene = ctx.Scene;
        if (!renderScene)
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
                command.m_VertexInputLayout = staticMeshProxy->m_VertexInputLayout;
                command.m_IndexBuffer = staticMeshProxy->m_IndexBuffer;
                command.m_Material = staticMeshProxy->m_Material;
                command.m_ModelMatrix = staticMeshProxy->m_Transform.ToMatrix(); 
                command.m_CastShadow = staticMeshProxy->m_CastShadow;
                command.m_BoundingBox = staticMeshProxy->m_PrimitiveComponent->GetBoundingBox();

                if (!command.m_Material || !command.m_VertexInputLayout || !command.m_VertexBuffer)
                {
                    continue;
                }
                  
                if (command.m_Material->IsTranslucent())
                {
                    ctx.TranslucentQueue.push_back(command);
                }
                else
                {
                    ctx.OpaqueQueue.push_back(command);
                }
            }
        }
    }

    DirShadowCommandBuildResult ForwardRenderer::BuildDirectionalShadowDrawCommands(const ShadowRequest& shadowRequest,
                                                                                     const ShadowResourceHandle& handle,
                                                                                     const DirectionalLightSceneProxy* lightProxy,
                                                                                     uint32_t cascadeCount,
                                                                                     RenderCamera* camera,
                                                                                     const std::vector<MeshDrawCommand>& opaqueQueue)
    {
        DirShadowCommandBuildResult result;

        RenderCamera* mainCamera = camera;
        if (!mainCamera)
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

            ExpandCascadeZForShadowCasters(aabb, lightView, opaqueQueue);
            
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
            command.GraphDepthResourceName = kRDGDirShadowAtlas;
            command.ViewProj = cascadeLightViewProjs[layerIndex];
            // command.ViewProj = CalculateDirectionalLightViewProjMatrix(lightProxy); // For simplicity, we use the same view projection matrix for all cascades for now, we can use the actual cascade-specific view projection matrix later.
            command.Target.TargetLayer = layerIndex;
            result.Commands.push_back(command);
        }

        return result;
    }

    ShadowDrawCommand ForwardRenderer::BuildSpotShadowDrawCommand(const ShadowRequest& shadowRequest,
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

    std::vector<ShadowDrawCommand> ForwardRenderer::BuildPointShadowDrawCommands(const ShadowRequest& shadowRequest,
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

    std::vector<CascadeSplit> ForwardRenderer::CalculateCascadeSplits(float nearPlane, float farPlane, uint32_t cascadeCount)
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

    void ForwardRenderer::ExpandCascadeZForShadowCasters(Math::Geometry::AABB& frustumAABB,
                                                        const Matrix4& lightView,
                                                        const std::vector<MeshDrawCommand>& opaqueQueue)
    {
        using Math::Geometry::AABB;
        for (const auto& command : opaqueQueue)
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

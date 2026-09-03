#include "ForwardRenderer.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHIBackend.h"
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
#include "Render/Environment/EnvironmentMap.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Function/Physics/PhysicsDebugDraw.h"
#include "Math/Geometry/AABB.h"
#include "Render/RHI/RHIClipSpace.h"
#include "Render/RHI/RHIClipSpaceCapabilities.h"
#include <glm/gtc/matrix_transform.hpp>
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
    ShadowGraphPermanentOutput ForwardRenderer::MakePermanentShadowOutput(size_t passIndex)
    {
        ShadowGraphPermanentOutput output{};
        output.IsSet = true;
        output.Resolution = ShadowResolution{
            .Width = kShadowMapResolution,
            .Height = kShadowMapResolution
        };

        if (passIndex < MAX_CASCADES)
        {
            output.DepthResourceName = kRDGDirShadowAtlas;
            output.ResourceType = ShadowResourceType::Depth2DArray;
            output.LayerCount = static_cast<int>(MAX_CASCADES);
        }
        else if (passIndex < MAX_CASCADES + static_cast<size_t>(MAX_SPOT_SHADOW_MAPS))
        {
            const int slot = static_cast<int>(passIndex - MAX_CASCADES);
            output.DepthResourceName = "SpotShadow." + std::to_string(slot);
            output.ResourceType = ShadowResourceType::Depth2D;
            output.LayerCount = 1;
        }
        else
        {
            const size_t remainder = passIndex - (MAX_CASCADES + static_cast<size_t>(MAX_SPOT_SHADOW_MAPS));
            const int pointSlot = static_cast<int>(remainder / 6u);
            output.DepthResourceName = "PointShadow." + std::to_string(pointSlot);
            output.ResourceType = ShadowResourceType::DepthCube;
            output.LayerCount = 6;
        }

        return output;
    }

    size_t ForwardRenderer::GetFixedShadowGraphPassIndex(const ShadowDrawCommand& command)
    {
        switch (command.Type)
        {
        case LightType::Directional:
            return static_cast<size_t>(command.Target.TargetLayer);
        case LightType::Spot:
            return MAX_CASCADES + static_cast<size_t>(command.Handle.SlotIndex);
        case LightType::Point:
            return MAX_CASCADES + static_cast<size_t>(MAX_SPOT_SHADOW_MAPS)
                + static_cast<size_t>(command.Handle.SlotIndex * 6 + command.Target.TargetFace);
        default:
            return static_cast<size_t>(-1);
        }
    }

    void ForwardRenderer::AssignShadowGraphPassCommands(const SceneRenderContext& ctx)
    {
        for (std::unique_ptr<ShadowGraphPass>& pass : m_ShadowGraphPasses)
        {
            pass->ClearCommand();
        }

        for (const ShadowDrawCommand& command : ctx.ShadowDrawCommands)
        {
            const size_t slot = GetFixedShadowGraphPassIndex(command);
            if (slot < m_ShadowGraphPasses.size())
            {
                m_ShadowGraphPasses[slot]->Configure(command);
            }
        }
    }

    void ForwardRenderer::Initialize()
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        m_FrameIndex = 0;

        RHICommandList cmdList(rhi);
        m_PerFrameUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(PerFrameData)));
        m_LightDataUniformBuffer = cmdList.CreateBuffer(MakeUniformBufferDesc(sizeof(LightsData)));

        const uint32_t uboAlign = rhi->RHIGetMinUniformBufferOffsetAlignment();
        m_PerObjectSlotStride = ((static_cast<uint32_t>(sizeof(Matrix4)) + uboAlign - 1u) / uboAlign) * uboAlign;
        m_PerObjectUniformBuffer = cmdList.CreateBuffer(
            MakeUniformBufferDesc(m_PerObjectSlotStride * EngineSceneBindingSets::kPerObjectRingSlots));
        m_ShadowUniformBuffers.Initialize(cmdList, *rhi);

        m_SceneBindings.Initialize(cmdList);
        m_PipelineLayouts.Initialize(cmdList, m_SceneBindings);

        m_ShadowPass.pipeline = this;
        m_BasePass.pipeline = this;
        m_TranslucentPass.pipeline = this;
        m_DebugDrawPass.pipeline = this;

        m_ShadowPass.Initialize();
        m_ShadowPass.m_PerObjectUniformBuffer = m_PerObjectUniformBuffer.get();
        m_ShadowPass.m_ShadowUniformBuffers = &m_ShadowUniformBuffers;

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
        if (RHIShaderRef fxaaShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
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
        if (RHIShaderRef sharpenShader = EngineShaderUtils::CreateShaderFromSpirvFiles(
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

        // Assign shared screen-quad resources before Initialize so PSO keeps live layout/buffer pointers.
        m_PresentPass.m_ScreenQuadVertexBuffer = m_ScreenQuadVertexBuffer;
        m_PresentPass.m_ScreenQuadVertexLayout = m_ScreenQuadVertexLayout;
        m_PresentPass.Initialize();

        m_DebugDrawPass.Initialize();

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
        bool enablePostProcess,
        bool presentToBackBuffer,
        bool enableDebugDraw)
    {
        if (RHI* rhi = RenderSystem::Get().GetRHI())
        {
            rhi->NotifyAttachmentResourcesDiscarded();
        }

        m_FrameRenderGraph.Reset();
        m_ShadowGraphPasses.clear();
        m_ShadowGraphPassPtrs.clear();
        m_SceneSkyGraphPass = nullptr;
        m_SceneOpaqueGraphPass = nullptr;
        m_SceneTranslucentGraphPass = nullptr;
        m_SceneDebugGraphPass = nullptr;
        m_PostFxaaGraphPass = nullptr;
        m_PostSharpenGraphPass = nullptr;
        m_PresentGraphPass = nullptr;

        m_ShadowGraphPasses.reserve(kMaxShadowGraphPasses);
        m_ShadowGraphPassPtrs.reserve(kMaxShadowGraphPasses);
        for (size_t shadowIndex = 0; shadowIndex < kMaxShadowGraphPasses; ++shadowIndex)
        {
            auto shadowGraphPass = std::make_unique<ShadowGraphPass>(m_ShadowPass);
            shadowGraphPass->SetSlotNames(
                MakeShadowGraphPassName(shadowIndex),
                MakeShadowDepthSlotName(shadowIndex));
            shadowGraphPass->SetPermanentGraphOutput(MakePermanentShadowOutput(shadowIndex));

            const std::string passName = MakeShadowGraphPassName(shadowIndex);
            RenderPass& graphPass = m_FrameRenderGraph.AddPass(passName);
            graphPass.SetImplementation(shadowGraphPass.get());
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

        if (enableDebugDraw)
        {
            RenderPass& debugPass = m_FrameRenderGraph.AddPass("Scene.Debug");
            debugPass.SetImplementation(&m_DebugDrawPass);
            m_SceneDebugGraphPass = &debugPass;
        }

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
        m_ConfiguredEnableDebugDraw = enableDebugDraw;
        m_FrameRenderGraphBuilt = true;
        m_SceneBindings.InvalidateShadowTextureBindings();
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
        const bool enableDebugDraw = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableDebugDraw);
        if (enablePostProcess != m_ConfiguredEnablePostProcess
            || presentToBackBuffer != m_ConfiguredPresentToBackBuffer
            || enableDebugDraw != m_ConfiguredEnableDebugDraw)
        {
            m_FrameRenderGraphBuilt = false;
        }

        if (!m_FrameRenderGraphBuilt)
        {
            BuildFrameRenderGraph(enablePostProcess, presentToBackBuffer, enableDebugDraw);
        }

        if (m_FrameRenderGraph.GetBackbufferWidth() != width
            || m_FrameRenderGraph.GetBackbufferHeight() != height)
        {
            m_FrameRenderGraph.SetBackbufferDimensions(width, height);
        }

        AssignShadowGraphPassCommands(ctx);

        // RND-F12: Granite-aligned — re-run setup_dependencies + pass order every frame.
        const bool enableSky = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableSkyBox);
        m_BasePass.SetClearSceneTargets(!enableSky);

        RenderGraphFrameContext frameContext;
        frameContext.DrawDesc = &desc;
        frameContext.SceneContext = &ctx;
        frameContext.Renderer = this;
        frameContext.CommandList = &cmdList;
        m_FrameRenderGraph.SetFrameContext(frameContext);

        m_FrameRenderGraph.Bake();
        if (m_FrameRenderGraph.SetupAttachments(*rhi, nullptr))
        {
            m_SceneBindings.InvalidateShadowTextureBindings();
        }
    }

    void ForwardRenderer::BindGraphShadowTextures(SceneRenderContext& ctx)
    {
        ctx.DirectionalShadowHandle.Texture = nullptr;
        ctx.DirectionalShadowHandle.RdgPhysicalIndex = UINT32_MAX;
        for (ShadowResourceHandle& handle : ctx.SpotShadowHandles)
        {
            handle.Texture = nullptr;
            handle.RdgPhysicalIndex = UINT32_MAX;
        }
        for (ShadowResourceHandle& handle : ctx.PointShadowHandles)
        {
            handle.Texture = nullptr;
            handle.RdgPhysicalIndex = UINT32_MAX;
        }
        for (auto& entry : ctx.SpotShadowHandleMap)
        {
            entry.second.Texture = nullptr;
            entry.second.RdgPhysicalIndex = UINT32_MAX;
        }
        for (auto& entry : ctx.PointShadowHandleMap)
        {
            entry.second.Texture = nullptr;
            entry.second.RdgPhysicalIndex = UINT32_MAX;
        }

        for (const ShadowDrawCommand& command : ctx.ShadowDrawCommands)
        {
            if (!command.Handle.IsValid() || command.GraphDepthResourceName.empty())
            {
                continue;
            }

            const size_t slot = GetFixedShadowGraphPassIndex(command);
            if (slot >= m_ShadowGraphPasses.size())
            {
                continue;
            }

            ShadowGraphPass& shadowGraphPass = *m_ShadowGraphPasses[slot];

            RDGTextureResource* depthResource =
                m_FrameRenderGraph.FindTextureResource(command.GraphDepthResourceName);
            if (depthResource == nullptr || depthResource->GetPhysicalIndex() == RDGResource::kUnused)
            {
                continue;
            }

            RHITextureRef texture = m_FrameRenderGraph.GetPhysicalTextureShared(*depthResource);
            const uint32_t physicalIndex = depthResource->GetPhysicalIndex();
            shadowGraphPass.BindGraphTexture(texture);

            if (command.Type == LightType::Directional)
            {
                ctx.DirectionalShadowHandle.Texture = texture;
                ctx.DirectionalShadowHandle.RdgPhysicalIndex = physicalIndex;
            }
            else if (command.Type == LightType::Spot)
            {
                const int spotSlot = command.Handle.SlotIndex;
                if (spotSlot >= 0 && spotSlot < static_cast<int>(ctx.SpotShadowHandles.size()))
                {
                    ctx.SpotShadowHandles[static_cast<size_t>(spotSlot)].Texture = texture;
                    ctx.SpotShadowHandles[static_cast<size_t>(spotSlot)].RdgPhysicalIndex = physicalIndex;
                }
                for (auto& entry : ctx.SpotShadowHandleMap)
                {
                    if (entry.second.SlotIndex == spotSlot)
                    {
                        entry.second.Texture = texture;
                        entry.second.RdgPhysicalIndex = physicalIndex;
                    }
                }
            }
            else if (command.Type == LightType::Point)
            {
                const int pointSlot = command.Handle.SlotIndex;
                if (pointSlot >= 0 && pointSlot < static_cast<int>(ctx.PointShadowHandles.size()))
                {
                    ctx.PointShadowHandles[static_cast<size_t>(pointSlot)].Texture = texture;
                    ctx.PointShadowHandles[static_cast<size_t>(pointSlot)].RdgPhysicalIndex = physicalIndex;
                }
                for (auto& entry : ctx.PointShadowHandleMap)
                {
                    if (entry.second.SlotIndex == pointSlot)
                    {
                        entry.second.Texture = texture;
                        entry.second.RdgPhysicalIndex = physicalIndex;
                    }
                }
            }
        }
    }

    void ForwardRenderer::EnqueueFrameRenderGraph(RHICommandList& cmdList, SceneRenderTarget* sceneTarget)
    {
        if (sceneTarget == nullptr)
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

    void ForwardRenderer::Shutdown()
    {
        m_PipelineLayouts.Shutdown();
        m_SceneBindings.Shutdown();
        m_SkyBoxPass.Shutdown();
        m_DebugDrawPass.Shutdown();

        m_ShadowPass.m_OpaqueQueue.clear();

        m_ShadowGraphPasses.clear();
        m_ShadowGraphPassPtrs.clear();
        m_FrameRenderGraph.Reset();
        m_PostBufferTexture.reset();
        m_SceneSkyGraphPass = nullptr;
        m_SceneOpaqueGraphPass = nullptr;
        m_SceneTranslucentGraphPass = nullptr;
        m_SceneDebugGraphPass = nullptr;
        m_PostFxaaGraphPass = nullptr;
        m_PostSharpenGraphPass = nullptr;
        m_PresentGraphPass = nullptr;
        m_FrameRenderGraphBuilt = false;
        m_PostBufferWidth = 0;
        m_PostBufferHeight = 0;

        m_ShadowPass.m_PerObjectUniformBuffer = nullptr;
        m_ShadowPass.m_ShadowUniformBuffers = nullptr;

        m_ScreenQuadVertexBuffer.reset();
        m_ScreenQuadVertexLayout.reset();

        m_LightDataUniformBuffer.reset();
        m_PerFrameUniformBuffer.reset();
        m_PerObjectUniformBuffer.reset();
        m_ShadowUniformBuffers.Shutdown();
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

        if (HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableDebugDraw) && desc.GameplayScene != nullptr)
        {
            DebugDrawService::Get().ClearFrameQueues();
            PhysicsDebugDraw::SubmitScene(*desc.GameplayScene, PhysicsDebugDraw::GetOptions());
        }

        BindSceneRenderTarget(*sceneTarget);

        SceneRenderContext ctx;
        ctx.Scene = desc.Scene;
        ctx.Camera = desc.Camera;

        desc.Scene->CollectOrphanedSceneProxies();

        BuildRenderQueue(ctx);

        const bool enableShadows = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableShadows);
        if (enableShadows)
        {
            m_ShadowUniformBuffers.BeginShadowFrame();
            CollectShadowRequests(ctx);
            BuildShadowDrawCommands(ctx);
        }
        else
        {
            ctx.ShadowDrawCommands.clear();
            ctx.DirectionalShadowHandle = ShadowResourceHandle{};
            ctx.SpotShadowHandles.clear();
            ctx.PointShadowHandles.clear();
            ctx.SpotShadowHandleMap.clear();
            ctx.PointShadowHandleMap.clear();
            ClearUnusedShadowViewProjSlots(ctx);
        }

        m_ShadowPass.m_OpaqueQueue = ctx.OpaqueQueue;

        RHICommandList cmdList(rhi);

        UpdatePerFrameUBO(ctx);

        // RND-F08: allocate graph shadow maps before Set1 samples them.
        SetupFrameRenderGraph(cmdList, desc, ctx);
        BindGraphShadowTextures(ctx);

        RenderGraphFrameContext frameContext = m_FrameRenderGraph.GetFrameContext();
        frameContext.DrawDesc = &desc;
        frameContext.SceneContext = &ctx;
        m_FrameRenderGraph.SetFrameContext(frameContext);

        UpdateLightUBO(ctx);

        if (RHI* rhiForIbl = RenderSystem::Get().GetRHI())
        {
            if (SkyBoxSceneProxy* skyProxy = ctx.Scene ? ctx.Scene->GetSkyBoxProxy() : nullptr)
            {
                if (skyProxy->m_EnvironmentMap
                    && skyProxy->m_EnvironmentMap->EnsureGPUResources(*rhiForIbl))
                {
                    EnvironmentMap* env = skyProxy->m_EnvironmentMap.get();
                    if (env->GetIrradiance() != nullptr)
                    {
                        ctx.IblIrradianceTexture = env->GetIrradiance()->GetRHITexture();
                    }
                    if (env->GetPrefilter() != nullptr)
                    {
                        ctx.IblPrefilterTexture = env->GetPrefilter()->GetRHITexture();
                    }
                    if (env->GetBrdfLUT() != nullptr)
                    {
                        ctx.IblBrdfLutTexture = env->GetBrdfLUT()->GetRHITexture();
                    }
                }
            }
        }

        m_SceneBindings.BeginFrame(
            cmdList,
            m_PerFrameUniformBuffer.get(),
            m_LightDataUniformBuffer.get(),
            m_PerObjectUniformBuffer.get(),
            m_PerObjectSlotStride);
        m_SceneBindings.BuildSceneSet1(
            cmdList,
            ctx,
            m_ShadowUniformBuffers.GetDirLightViewProjBuffer(),
            m_ShadowUniformBuffers.GetCascadeFarPlaneBuffer(),
            m_ShadowUniformBuffers.GetSpotLightViewProjBuffer());

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
            if (ctx.DirectionalShadowHandle.IsValid() && dirLightProxy->m_CastsShadow)
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
            lightsData.PointLights[pointLightCount].Position =
                Vector4(pointLightProxy->m_Position, pointLightProxy->m_AttenuationRadius);
            lightsData.PointLights[pointLightCount].Color =
                Vector4(pointLightProxy->m_LightColor, pointLightProxy->m_Intensity);
            int shadowIndex = -1;
            auto pointShadowIt = ctx.PointShadowHandleMap.find(pointLightProxy);
            if (pointShadowIt != ctx.PointShadowHandleMap.end() && pointShadowIt->second.IsValid()
                && pointLightProxy->m_CastsShadow)
            {
                shadowIndex = pointShadowIt->second.SlotIndex;
                if (shadowIndex < 0 || shadowIndex >= MAX_POINT_SHADOW_MAPS)
                {
                    shadowIndex = -1;
                }
            }
            const float shadowFar = glm::clamp(
                pointLightProxy->m_AttenuationRadius,
                kPointShadowNear + 0.01f,
                kPointShadowFar);
            lightsData.PointLights[pointLightCount].Params = Vector4(
                pointLightProxy->m_AttenuationFalloff,
                0.0f,
                shadowFar,
                static_cast<float>(shadowIndex));
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
            if (spotShadowIt != ctx.SpotShadowHandleMap.end() && spotShadowIt->second.IsValid()
                && spotLightProxy->m_CastsShadow)
            {
                shadowIndex = spotShadowIt->second.SlotIndex;
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

    ShadowResourceHandle ForwardRenderer::MakeDirectionalShadowBinding(
        const ShadowRequest& req,
        uint32_t cascadeCount) const
    {
        if (!req.Resolution.IsValid() || cascadeCount == 0)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2DArray;
        handle.SlotIndex = 0;
        handle.ArrayBaseLayer = 0;
        handle.LayerCount = static_cast<int>(cascadeCount);
        handle.Resolution = req.Resolution;
        handle.Texture = nullptr;
        return handle;
    }

    ShadowResourceHandle ForwardRenderer::MakeSpotShadowBinding(const ShadowRequest& req, int slotIndex) const
    {
        if (!req.Resolution.IsValid() || !req.LightProxy || slotIndex < 0 || slotIndex >= MAX_SPOT_SHADOW_MAPS)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::Depth2D;
        handle.SlotIndex = slotIndex;
        handle.Resolution = req.Resolution;
        handle.Texture = nullptr;
        return handle;
    }

    ShadowResourceHandle ForwardRenderer::MakePointShadowBinding(const ShadowRequest& req, int slotIndex) const
    {
        if (!req.Resolution.IsValid() || !req.LightProxy || slotIndex < 0 || slotIndex >= MAX_POINT_SHADOW_MAPS)
        {
            return ShadowResourceHandle::InvalidHandle();
        }

        ShadowResourceHandle handle{};
        handle.ResourceType = ShadowResourceType::DepthCube;
        handle.SlotIndex = slotIndex;
        handle.Resolution = req.Resolution;
        handle.Texture = nullptr;
        return handle;
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
                ShadowResourceHandle handle = MakeDirectionalShadowBinding(shadowRequest, MAX_CASCADES);
                if (!dirLightProxy || !handle.IsValid())
                {
                    continue;
                }

                DirShadowCommandBuildResult result = BuildDirectionalShadowDrawCommands(
                    shadowRequest, handle, dirLightProxy, MAX_CASCADES, ctx.Camera, ctx.OpaqueQueue);
                // Update directional UBO slots and per-command bindings before inserting into ctx.
                for (size_t i = 0; i < result.Commands.size(); ++i)
                {
                    auto& command = result.Commands[i];
                    m_ShadowUniformBuffers.GetCascadeFarPlaneBuffer()->UpdateSubresource(
                        &result.CascadeFarPlaneVS[i],
                        m_ShadowUniformBuffers.GetCascadeFarPlaneOffset(static_cast<uint32_t>(i)),
                        sizeof(float));
                    m_ShadowUniformBuffers.GetDirLightViewProjBuffer()->UpdateSubresource(
                        &command.ViewProj,
                        m_ShadowUniformBuffers.GetDirLightViewProjOffset(static_cast<uint32_t>(i)),
                        sizeof(Matrix4));
                    command.ViewProjUniformBuffer = m_ShadowUniformBuffers.GetDirLightViewProjBuffer();
                    command.ViewProjUniformOffset =
                        m_ShadowUniformBuffers.GetDirLightViewProjOffset(static_cast<uint32_t>(i));
                    ShadowPassParamsUBO params{};
                    params.UseLinearDepth = 0;
                    command.ParamsUniformBuffer = m_ShadowUniformBuffers.GetParamsRingBuffer();
                    command.ParamsUniformOffset = m_ShadowUniformBuffers.WriteParams(params);
                }
                ctx.ShadowDrawCommands.insert(
                    ctx.ShadowDrawCommands.end(), result.Commands.begin(), result.Commands.end());
                ctx.DirectionalShadowHandle = handle;
                directionalLightCount++;
            }
            else if (shadowRequest.Type == LightType::Spot)
            {
                auto spotLightProxy = static_cast<SpotLightSceneProxy*>(shadowRequest.LightProxy);
                if (ctx.SpotShadowHandles.size() >= static_cast<size_t>(MAX_SPOT_SHADOW_MAPS))
                {
                    continue;
                }
                const int spotSlot = static_cast<int>(ctx.SpotShadowHandles.size());
                ShadowResourceHandle handle = MakeSpotShadowBinding(shadowRequest, spotSlot);
                if (!spotLightProxy || !handle.IsValid())                
                {
                    continue;
                }
                ctx.SpotShadowHandleMap[spotLightProxy] = handle;
                ctx.SpotShadowHandles.push_back(handle);
                ShadowDrawCommand command = BuildSpotShadowDrawCommand(shadowRequest, handle, spotLightProxy);
                command.GraphDepthResourceName = "SpotShadow." + std::to_string(spotSlot);
                m_ShadowUniformBuffers.GetSpotLightViewProjBuffer()->UpdateSubresource(
                    &command.ViewProj,
                    m_ShadowUniformBuffers.GetSpotLightViewProjOffset(static_cast<uint32_t>(spotSlot)),
                    sizeof(Matrix4));
                command.ViewProjUniformBuffer = m_ShadowUniformBuffers.GetSpotLightViewProjBuffer();
                command.ViewProjUniformOffset =
                    m_ShadowUniformBuffers.GetSpotLightViewProjOffset(static_cast<uint32_t>(spotSlot));
                ShadowPassParamsUBO params{};
                params.UseLinearDepth = 0;
                command.ParamsUniformBuffer = m_ShadowUniformBuffers.GetParamsRingBuffer();
                command.ParamsUniformOffset = m_ShadowUniformBuffers.WriteParams(params);
                ctx.ShadowDrawCommands.push_back(command);
                spotLightCount++;
            }
            else if(shadowRequest.Type == LightType::Point)
            {
                auto pointLightProxy = static_cast<PointLightSceneProxy*>(shadowRequest.LightProxy);
                if (ctx.PointShadowHandles.size() >= static_cast<size_t>(MAX_POINT_SHADOW_MAPS))
                {
                    continue;
                }
                const int pointSlot = static_cast<int>(ctx.PointShadowHandles.size());
                ShadowResourceHandle handle = MakePointShadowBinding(shadowRequest, pointSlot);
                if (!pointLightProxy || !handle.IsValid())
                {
                    continue;
                }
                ctx.PointShadowHandleMap[pointLightProxy] = handle;
                ctx.PointShadowHandles.push_back(handle);
                std::vector<ShadowDrawCommand> commands = BuildPointShadowDrawCommands(shadowRequest, handle, pointLightProxy);
                const std::string pointDepthName = "PointShadow." + std::to_string(pointSlot);
                for (ShadowDrawCommand& command : commands)
                {
                    command.GraphDepthResourceName = pointDepthName;
                    command.ViewProjUniformBuffer = m_ShadowUniformBuffers.GetPointLightViewProjRingBuffer();
                    command.ViewProjUniformOffset = m_ShadowUniformBuffers.WritePointLightViewProj(command.ViewProj);
                    ShadowPassParamsUBO params{};
                    params.UseLinearDepth = 1;
                    params.LightPos[0] = command.LightPosition.x;
                    params.LightPos[1] = command.LightPosition.y;
                    params.LightPos[2] = command.LightPosition.z;
                    params.FarPlane = command.FarPlane;
                    command.ParamsUniformBuffer = m_ShadowUniformBuffers.GetParamsRingBuffer();
                    command.ParamsUniformOffset = m_ShadowUniformBuffers.WriteParams(params);
                }
                ctx.ShadowDrawCommands.insert(ctx.ShadowDrawCommands.end(), commands.begin(), commands.end());
                pointLightCount++;
            }
        }

        if (directionalLightCount == 0
            && m_ShadowUniformBuffers.GetDirLightViewProjBuffer()
            && m_ShadowUniformBuffers.GetCascadeFarPlaneBuffer())
        {
            const Matrix4 zeroMatrix{};
            const float zeroFar = 0.0f;
            for (int i = 0; i < MAX_CASCADES; ++i)
            {
                m_ShadowUniformBuffers.GetDirLightViewProjBuffer()->UpdateSubresource(
                    &zeroMatrix,
                    m_ShadowUniformBuffers.GetDirLightViewProjOffset(static_cast<uint32_t>(i)),
                    sizeof(Matrix4));
                m_ShadowUniformBuffers.GetCascadeFarPlaneBuffer()->UpdateSubresource(
                    &zeroFar,
                    m_ShadowUniformBuffers.GetCascadeFarPlaneOffset(static_cast<uint32_t>(i)),
                    sizeof(float));
            }
        }

        ClearUnusedShadowViewProjSlots(ctx);
    }

    void ForwardRenderer::ClearUnusedShadowViewProjSlots(const SceneRenderContext& ctx)
    {
        RHIBuffer* spotBuffer = m_ShadowUniformBuffers.GetSpotLightViewProjBuffer();
        if (!spotBuffer)
        {
            return;
        }

        const Matrix4 zeroMatrix{};
        for (int slot = 0; slot < MAX_SPOT_SHADOW_MAPS; ++slot)
        {
            bool slotInUse = false;
            for (const ShadowResourceHandle& handle : ctx.SpotShadowHandles)
            {
                if (handle.IsValid() && handle.SlotIndex == slot)
                {
                    slotInUse = true;
                    break;
                }
            }
            if (!slotInUse)
            {
                spotBuffer->UpdateSubresource(
                    &zeroMatrix,
                    m_ShadowUniformBuffers.GetSpotLightViewProjOffset(static_cast<uint32_t>(slot)),
                    sizeof(Matrix4));
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
        const float nearNdcZ = GetFrustumNdcZNear();
        const float farNdcZ = GetFrustumNdcZFar();
        Vector4 ndcCorners[8] = {
            Vector4(-1, -1, nearNdcZ, 1), // Near bottom left
            Vector4(1, -1, nearNdcZ, 1),  // Near bottom right
            Vector4(1, 1, nearNdcZ, 1),   // Near top right
            Vector4(-1, 1, nearNdcZ, 1),  // Near top left
            Vector4(-1, -1, farNdcZ, 1),  // Far bottom left
            Vector4(1, -1, farNdcZ, 1),   // Far bottom right
            Vector4(1, 1, farNdcZ, 1),    // Far top right
            Vector4(-1, 1, farNdcZ, 1)    // Far top left
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
            Matrix4 lightProj = RHIClipSpace::MakeOrthographic(
                aabb.Min.x,
                aabb.Max.x,
                aabb.Min.y,
                aabb.Max.y,
                -aabb.Max.z,
                -aabb.Min.z);
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
        Matrix4 lightProj = RHIClipSpace::MakePerspective(fov, 1.0f, kSpotShadowNear, kSpotShadowFar);
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
        const float shadowFar = glm::clamp(
            lightProxy->m_AttenuationRadius,
            kPointShadowNear + 0.01f,
            kPointShadowFar);
        Matrix4 lightProj =
            RHIClipSpace::MakePerspective(glm::radians(90.0f), 1.0f, kPointShadowNear, shadowFar);

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
            command.FarPlane = shadowFar;

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
            if (!command.m_CastShadow)
            {
                continue;
            }

            const AABB& meshAABB = command.m_BoundingBox;
            if (!meshAABB.IsValid())
            {
                ME_CORE_WARN("Invalid mesh AABB for shadow caster, skipping it in cascade Z expansion");
                continue;
            }

            // Fit light-space Z to the caster's AABB corners. A bounding-sphere radius
            // over-expands flat/large meshes (e.g. 100x100 ground) and destroys CSM depth
            // precision, which shows up as directional self-shadow acne.
            const AABB meshAabbLS = Math::Geometry::Transform(meshAABB, lightView);
            frustumAABB.Min.z = glm::min(frustumAABB.Min.z, meshAabbLS.Min.z);
            frustumAABB.Max.z = glm::max(frustumAABB.Max.z, meshAabbLS.Max.z);
        }
    }
}

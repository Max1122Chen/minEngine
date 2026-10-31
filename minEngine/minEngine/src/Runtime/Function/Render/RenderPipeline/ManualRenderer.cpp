#include "ManualRenderer.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Render/EngineRHITextureUtils.h"
#include "Render/Environment/EnvironmentMap.h"
#include "Render/RenderGraph/SceneRenderPassUtils.h"
#include "Render/RenderPipeline/SceneMeshDrawUtils.h"
#include "Render/RenderScene.h"
#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"
#include "Render/RHI/RHICommandList.h"
#include "Render/RHI/RHICommandListPool.h"
#include "Render/SceneDrawDesc.h"
#include "Render/SceneRenderTarget.h"
#include "Render/SkyBoxSceneProxies/SkyBoxSceneProxy.h"

namespace minEngine
{
    namespace
    {
        RHITextureCreateDesc MakeManualSceneColorDesc(uint32_t width, uint32_t height)
        {
            RHITextureCreateDesc desc{};
            desc.Dimension = RHITextureDimension::Texture2D;
            desc.Width = width;
            desc.Height = height;
            desc.DepthOrArrayLayers = 1;
            desc.Format = TextureFormat::RGBA8;
            desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
            return desc;
        }

        RHITextureCreateDesc MakeManualSceneDepthDesc(uint32_t width, uint32_t height)
        {
            RHITextureCreateDesc desc{};
            desc.Dimension = RHITextureDimension::Texture2D;
            desc.Width = width;
            desc.Height = height;
            desc.DepthOrArrayLayers = 1;
            desc.Format = TextureFormat::DEPTH24STENCIL8;
            desc.Flags = RHITextureCreateFlags::RenderTarget | RHITextureCreateFlags::ShaderResource;
            return desc;
        }

        bool TextureMatches(
            const RHITextureRef& texture,
            const RHITextureCreateDesc& wantDesc)
        {
            if (!texture)
            {
                return false;
            }

            const RHITextureCreateDesc& haveDesc = texture->GetDesc();
            return haveDesc.Width == wantDesc.Width && haveDesc.Height == wantDesc.Height
                && haveDesc.Format == wantDesc.Format
                && haveDesc.Dimension == wantDesc.Dimension
                && haveDesc.DepthOrArrayLayers == wantDesc.DepthOrArrayLayers
                && (haveDesc.Flags & wantDesc.Flags) == wantDesc.Flags;
        }
    }

    bool ManualRenderer::EnsureManualTextures(RHI& rhi, uint32_t width, uint32_t height)
    {
        bool recreated = false;

        const RHITextureCreateDesc colorDesc = MakeManualSceneColorDesc(width, height);
        if (!TextureMatches(m_ManualSceneColor, colorDesc))
        {
            m_ManualSceneColor = rhi.RHICreateTexture2D(colorDesc, nullptr);
            recreated = true;
        }

        const RHITextureCreateDesc depthDesc = MakeManualSceneDepthDesc(width, height);
        if (!TextureMatches(m_ManualSceneDepth, depthDesc))
        {
            m_ManualSceneDepth = rhi.RHICreateTexture2D(depthDesc, nullptr);
            recreated = true;
        }

        const RHITextureCreateDesc dirShadowDesc = MakeDepthTextureDesc(
            kShadowMapResolution,
            kShadowMapResolution,
            RHITextureDimension::Texture2DArray,
            MAX_CASCADES);
        if (!TextureMatches(m_ManualDirShadowAtlas, dirShadowDesc))
        {
            m_ManualDirShadowAtlas = rhi.RHICreateTexture2D(dirShadowDesc, nullptr);
            recreated = true;
        }

        for (int spotIndex = 0; spotIndex < MAX_SPOT_SHADOW_MAPS; ++spotIndex)
        {
            const RHITextureCreateDesc spotDesc = MakeDepthTextureDesc(
                kShadowMapResolution,
                kShadowMapResolution,
                RHITextureDimension::Texture2D,
                1);
            if (!TextureMatches(m_ManualSpotShadowMaps[static_cast<size_t>(spotIndex)], spotDesc))
            {
                m_ManualSpotShadowMaps[static_cast<size_t>(spotIndex)] = rhi.RHICreateTexture2D(spotDesc, nullptr);
                recreated = true;
            }
        }

        for (int pointIndex = 0; pointIndex < MAX_POINT_SHADOW_MAPS; ++pointIndex)
        {
            const RHITextureCreateDesc pointDesc = MakeDepthTextureDesc(
                kShadowMapResolution,
                kShadowMapResolution,
                RHITextureDimension::TextureCube,
                6);
            if (!TextureMatches(m_ManualPointShadowMaps[static_cast<size_t>(pointIndex)], pointDesc))
            {
                m_ManualPointShadowMaps[static_cast<size_t>(pointIndex)] =
                    rhi.RHICreateTexture2D(pointDesc, nullptr);
                recreated = true;
            }
        }

        m_ManualWidth = width;
        m_ManualHeight = height;
        return recreated;
    }

    void ManualRenderer::BindManualShadowTextures(SceneRenderContext& ctx)
    {
        if (m_ManualDirShadowAtlas && ctx.DirectionalShadowHandle.IsValid())
        {
            ctx.DirectionalShadowHandle.Texture = m_ManualDirShadowAtlas;
            ctx.DirectionalShadowHandle.RdgPhysicalIndex = UINT32_MAX;
        }

        for (size_t spotIndex = 0; spotIndex < ctx.SpotShadowHandles.size(); ++spotIndex)
        {
            if (spotIndex < m_ManualSpotShadowMaps.size() && m_ManualSpotShadowMaps[spotIndex])
            {
                ctx.SpotShadowHandles[spotIndex].Texture = m_ManualSpotShadowMaps[spotIndex];
                ctx.SpotShadowHandles[spotIndex].RdgPhysicalIndex = UINT32_MAX;
            }
        }

        for (auto& entry : ctx.SpotShadowHandleMap)
        {
            const int slot = entry.second.SlotIndex;
            if (slot >= 0 && slot < MAX_SPOT_SHADOW_MAPS && m_ManualSpotShadowMaps[static_cast<size_t>(slot)])
            {
                entry.second.Texture = m_ManualSpotShadowMaps[static_cast<size_t>(slot)];
                entry.second.RdgPhysicalIndex = UINT32_MAX;
            }
        }

        for (size_t pointIndex = 0; pointIndex < ctx.PointShadowHandles.size(); ++pointIndex)
        {
            if (pointIndex < m_ManualPointShadowMaps.size() && m_ManualPointShadowMaps[pointIndex])
            {
                ctx.PointShadowHandles[pointIndex].Texture = m_ManualPointShadowMaps[pointIndex];
                ctx.PointShadowHandles[pointIndex].RdgPhysicalIndex = UINT32_MAX;
            }
        }

        for (auto& entry : ctx.PointShadowHandleMap)
        {
            const int slot = entry.second.SlotIndex;
            if (slot >= 0 && slot < MAX_POINT_SHADOW_MAPS && m_ManualPointShadowMaps[static_cast<size_t>(slot)])
            {
                entry.second.Texture = m_ManualPointShadowMaps[static_cast<size_t>(slot)];
                entry.second.RdgPhysicalIndex = UINT32_MAX;
            }
        }

        for (ShadowDrawCommand& command : ctx.ShadowDrawCommands)
        {
            if (!command.Handle.IsValid())
            {
                continue;
            }

            switch (command.Type)
            {
            case LightType::Directional:
                command.Handle.Texture = m_ManualDirShadowAtlas;
                break;
            case LightType::Spot:
            {
                const int slot = command.Handle.SlotIndex;
                if (slot >= 0 && slot < MAX_SPOT_SHADOW_MAPS)
                {
                    command.Handle.Texture = m_ManualSpotShadowMaps[static_cast<size_t>(slot)];
                }
                break;
            }
            case LightType::Point:
            {
                const int slot = command.Handle.SlotIndex;
                if (slot >= 0 && slot < MAX_POINT_SHADOW_MAPS)
                {
                    command.Handle.Texture = m_ManualPointShadowMaps[static_cast<size_t>(slot)];
                }
                break;
            }
            default:
                break;
            }
        }
    }

    void ManualRenderer::ExecuteManualShadowPasses(RHICommandList& cmdList, SceneRenderContext& ctx)
    {
        m_ShadowPass.m_ShadowDrawCommands = ctx.ShadowDrawCommands;
        // The same hand-over the forward frame performs: the pass reads the assembled queue, and
        // the borrow ends with this recording function rather than outliving the frame.
        const ShadowCasterBorrowScope shadowCastersForThisFrame(
            m_ShadowPass.m_CasterQueue,
            ctx.ShadowCasterQueue,
            ctx.ShadowCasters);
        m_ShadowPass.PrepareShadowPass(cmdList);

        for (const ShadowDrawCommand& command : ctx.ShadowDrawCommands)
        {
            m_ShadowPass.RenderSingleDrawCommand(cmdList, command);
        }

        if (m_ManualDirShadowAtlas)
        {
            RHITextureTransitionInfo transition{};
            transition.Texture = m_ManualDirShadowAtlas.get();
            cmdList.Transition(transition);
        }
    }

    bool ManualRenderer::ExecuteManualSkyBoxPass(
        RHICommandList& cmdList,
        const SceneDrawDesc& desc,
        const SceneRenderContext& ctx,
        uint32_t width,
        uint32_t height)
    {
        if (!m_ManualSceneColor || !m_ManualSceneDepth)
        {
            return false;
        }

        if (!HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableSkyBox))
        {
            return false;
        }

        // Match Forward+RDG: sky pass owns the clear when EnableSkyBox is set.
        RHIRenderPassInfo passInfo =
            MakeSceneRenderPassInfo(
                m_ManualSceneColor.get(),
                m_ManualSceneDepth.get(),
                true,
                desc.ClearColor,
                desc.DepthClear);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, width, height, RHIViewportConvention::Scene);

        if (SkyBoxSceneProxy* skyProxy = ctx.Scene ? ctx.Scene->GetSkyBoxProxy() : nullptr)
        {
            if (skyProxy->m_Enabled && m_SkyBoxPass.IsReady())
            {
                m_SkyBoxPass.Execute(cmdList, *ctx.Camera, *skyProxy);
            }
        }

        cmdList.EndRenderPass();
        return true;
    }

    void ManualRenderer::ExecuteManualBasePass(
        RHICommandList& cmdList,
        const SceneDrawDesc& desc,
        const SceneRenderContext& ctx,
        bool clearScene,
        uint32_t width,
        uint32_t height)
    {
        if (!m_ManualSceneColor || !m_ManualSceneDepth)
        {
            return;
        }

        std::vector<MeshDrawPacket> drawPackets;
        PrepareSceneMeshDrawPackets(*this, cmdList, ctx.OpaqueQueue, MeshPassKind::Opaque, drawPackets);

        RHIRenderPassInfo passInfo =
            MakeSceneRenderPassInfo(
                m_ManualSceneColor.get(),
                m_ManualSceneDepth.get(),
                clearScene,
                clearScene ? desc.ClearColor : nullptr,
                clearScene ? desc.DepthClear : 1.0f);
        cmdList.BeginRenderPass(passInfo);
        cmdList.SetViewport(0, 0, width, height, RHIViewportConvention::Scene);
        SubmitSceneMeshDrawPackets(*this, cmdList, ctx.OpaqueQueue, drawPackets);
        cmdList.EndRenderPass();
    }

    void ManualRenderer::Execute(const SceneDrawDesc& desc)
    {
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (!rhi || !desc.Scene || !desc.Camera || !desc.RenderTarget)
        {
            return;
        }

        if (desc.ViewId == 0)
        {
            ME_LOG(
                LogRender,
                Error,
                "ManualRenderer::Execute: ViewId == 0; SceneDrawDesc must come from SceneViewport::BuildDrawDesc.");
            return;
        }

        SceneRenderTarget* sceneTarget = desc.RenderTarget;
        const uint32_t width = sceneTarget->GetWidth();
        const uint32_t height = sceneTarget->GetHeight();
        if (width == 0 || height == 0)
        {
            return;
        }

        BindSceneRenderTarget(*sceneTarget);

        SceneRenderContext ctx;
        ctx.Scene = desc.Scene;
        ctx.Camera = desc.Camera;
        ctx.bEnableShadows = HasSceneDrawFlag(desc.Flags, SceneDrawFlags::EnableShadows);

        desc.Scene->CollectOrphanedSceneProxies();
        BuildRenderQueue(ctx);
        SortMeshDrawQueues(ctx);

        // Before the frame starts recording: the environment bake loads an asset and submits an
        // immediate command buffer of its own, which cannot be spliced into a stream that is
        // already being built.
        PrepareSceneEnvironment(*rhi, ctx);

        RHICommandList stackList(rhi);
        const bool immediate = stackList.IsImmediate();
        // A lease, not a raw borrow: every early return below gives the slot back instead of
        // shrinking the pool until Acquire blocks.
        RHICommandListPool::Lease pooledList;
        RHICommandList* cmdListPtr = &stackList;
        if (!immediate)
        {
            ME_ASSERT(RHICommandListPool::HasInstance(), "Multi Recording requires RHICommandListPool");
            pooledList = RHICommandListPool::Get().AcquireLease();
            cmdListPtr = pooledList.Get();
            if (cmdListPtr == nullptr)
            {
                return;
            }
        }
        RHICommandList& cmdList = *cmdListPtr;
        // The window is a scope: an early return between here and the hand-off closes it on this
        // thread instead of leaving it open for the next frame's uploads.
        RHICommandListRecordingScope recordingScope(cmdList);

        UpdatePerFrameUBO(cmdList, ctx);

        // Shadow draws and the uniform slots they read are recorded in this frame's stream, so the
        // slot write sits before the draw that points at it instead of landing as an upload ahead of
        // the whole window.
        if (ctx.bEnableShadows)
        {
            m_ShadowUniformBuffers.BeginShadowFrame();
            CollectShadowRequests(ctx);
            BuildShadowDrawCommands(cmdList, ctx);
        }
        else
        {
            ctx.ShadowDrawCommands.clear();
            ctx.DirectionalShadowHandle = ShadowResourceHandle{};
            ctx.SpotShadowHandles.clear();
            ctx.PointShadowHandles.clear();
            ctx.SpotShadowHandleMap.clear();
            ctx.PointShadowHandleMap.clear();
            ClearUnusedShadowViewProjSlots(cmdList, ctx);
        }

        if (const bool texturesRecreated = EnsureManualTextures(*rhi, width, height); texturesRecreated)
        {
            m_SceneBindings.InvalidateShadowTextureBindings();
        }

        BindManualShadowTextures(ctx);
        UpdateLightUBO(cmdList, ctx);

        // The frame's IBL textures were published by PrepareSceneEnvironment above; recording only
        // names what that step produced.
        m_SceneBindings.BeginFrame(
            cmdList,
            m_PerFrameUniformBuffer.get(),
            m_LightDataUniformBuffer.get(),
            m_PerObjectUniformBuffer.get(),
            m_PerObjectSlotStride,
            m_BonePaletteUniformBuffer.get(),
            m_BonePaletteSlotStride);
        m_SceneBindings.BuildSceneSet1(
            cmdList,
            ctx,
            m_ShadowUniformBuffers.GetDirLightViewProjBuffer(),
            m_ShadowUniformBuffers.GetCascadeFarPlaneBuffer(),
            m_ShadowUniformBuffers.GetSpotLightViewProjBuffer());

        if (ctx.bEnableShadows && !ctx.ShadowDrawCommands.empty())
        {
            ExecuteManualShadowPasses(cmdList, ctx);
        }

        const bool skyClearedTargets = ExecuteManualSkyBoxPass(cmdList, desc, ctx, width, height);
        const bool clearScene = !skyClearedTargets;
        ExecuteManualBasePass(cmdList, desc, ctx, clearScene, width, height);

        if (m_ManualSceneColor)
        {
            RHITextureTransitionInfo colorTransition{};
            colorTransition.Texture = m_ManualSceneColor.get();
            cmdList.Transition(colorTransition);
        }

        const bool presentToBackBuffer =
            m_EnablePresentPass && HasSceneDrawFlag(desc.Flags, SceneDrawFlags::PresentToBackBuffer);
        if (presentToBackBuffer)
        {
            m_PresentPass.RunWithInputTexture(cmdList, m_ManualSceneColor.get());
        }

        sceneTarget->PublishGraphColorTexture(m_ManualSceneColor);
        sceneTarget->PublishGraphDepthTexture(m_ManualSceneDepth);

        // Close the window before the stream is handed over: Replay runs on the RHI role, which in a
        // frame-threaded topology is not the thread that recorded this frame.
        recordingScope.Finish();
        if (immediate)
        {
            cmdList.Replay();
        }
        else
        {
            pooledList.SubmitFinished();
        }

        ++m_FrameIndex;
    }

    void ManualRenderer::Shutdown()
    {
        m_ManualSceneColor.reset();
        m_ManualSceneDepth.reset();
        m_ManualDirShadowAtlas.reset();
        m_ManualSpotShadowMaps.fill(nullptr);
        m_ManualPointShadowMaps.fill(nullptr);
        m_ManualWidth = 0;
        m_ManualHeight = 0;

        ForwardRenderer::Shutdown();
    }
}

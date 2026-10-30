#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Geometry/Frustum.h"
#include "Runtime/Function/Render/SceneRenderContext.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"
#include "Runtime/Function/Render/DrawCommands/MeshCollectContext.h"
#include "Runtime/Function/Render/DrawCommands/MeshPassProcessor.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/SceneProxies/WidgetSceneProxy.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUICoords.h"

#include <algorithm>
#include <cstdint>

/**
 * Queue construction for the forward path.
 *
 * These live in a header as inline functions on purpose: the frame recorder owns GPU work that a
 * headless test cannot stand up, while the queues themselves are plain data, and the tables these
 * builders read - cached bounds, the widget draw bit, the canvas mapping - are exactly the contract
 * a test has to pin down. Reaching them from the test binary any other way would mean exporting
 * more of the renderer, and the export table has no headroom to spend on test reachability.
 */
namespace minEngine
{
    inline void BuildPrimitiveRenderQueue(SceneRenderContext& ctx, const Math::Geometry::Frustum* mainFrustum)
    {
        ctx.OpaqueQueue.clear();
        ctx.TranslucentQueue.clear();
        ctx.ShadowCasterQueue.clear();

        RenderScene* renderScene = ctx.Scene;
        if (!renderScene)
        {
            return;
        }

        const bool cullEnabled = mainFrustum != nullptr;

        MeshBatchCollector mainCollector;
        MeshBatchCollector shadowOnlyCollector;

        MeshCollectContext mainCollectContext;
        mainCollectContext.Purpose = MeshCollectPurpose::MainView;
        if (cullEnabled)
        {
            mainCollectContext.ViewFrustum = mainFrustum;
        }

        MeshCollectContext shadowOnlyCollectContext;
        shadowOnlyCollectContext.Purpose = MeshCollectPurpose::ShadowOnly;
        // No main-view frustum: sub-culling would drop casters outside the camera cone.

        for (PrimitiveSceneProxy* primitiveProxy : renderScene->m_PrimitiveSceneProxies)
        {
            // A removal command has already taken this proxy out of the draw set. The flag is
            // written where those commands run, so this decision never reads a game-role pointer
            // that the game role may be mutating or have freed.
            if (!primitiveProxy || primitiveProxy->m_bPendingRemove)
            {
                continue;
            }

            // Hidden primitives stay registered and keep ticking; they only stop producing draw
            // and shadow output. The game thread already decided that and stored it on the proxy,
            // and both collectors are fed from this loop, so one guard covers both.
            if (!primitiveProxy->m_Enabled)
            {
                continue;
            }

            // The box was captured on the game role. Resolving it here would reach the component,
            // which is exactly what is gone while a queued removal waits for the frame to end.
            const Math::Geometry::AABB worldBounds = primitiveProxy->ResolveWorldBounds();

            bool bMainViewVisible = true;
            if (cullEnabled && worldBounds.IsValid())
            {
                bMainViewVisible = mainFrustum->Intersect(worldBounds);
            }

            const bool bWantShadowCollect = ctx.bEnableShadows && primitiveProxy->m_CastShadow;
            if (!bMainViewVisible && !bWantShadowCollect)
            {
                continue;
            }

            if (bMainViewVisible)
            {
                primitiveProxy->GetMeshBatches(mainCollector, mainCollectContext);
            }
            else
            {
                primitiveProxy->GetMeshBatches(shadowOnlyCollector, shadowOnlyCollectContext);
            }
        }

        OpaqueMeshPassProcessor opaqueProcessor(ctx.OpaqueQueue);
        TranslucentMeshPassProcessor translucentProcessor(ctx.TranslucentQueue);
        ShadowDepthMeshPassProcessor shadowProcessor(ctx.ShadowCasterQueue);

        mainCollector.DispatchTo(opaqueProcessor);
        mainCollector.DispatchTo(translucentProcessor);
        mainCollector.DispatchTo(shadowProcessor);
        shadowOnlyCollector.DispatchTo(shadowProcessor);
    }

    inline void BuildScreenUIDrawQueue(SceneRenderContext& ctx, uint32_t viewportWidth, uint32_t viewportHeight)
    {
        ctx.ScreenUIQueue.clear();

        RenderScene* renderScene = ctx.Scene;
        if (!renderScene)
        {
            return;
        }

        for (WidgetSceneProxy* widgetProxy : renderScene->m_WidgetSceneProxies)
        {
            // The draw bit is the whole gate: the game thread ANDs the widget's own state, its
            // owning canvas being active and the material being compiled into it when the proxy is
            // assembled. Nothing here asks a component whether it may draw, or which canvas owns
            // it - the canvas context needed for the mapping travelled with the same sync.
            if (!widgetProxy || !widgetProxy->m_bVisible || !widgetProxy->m_bHasActiveCanvas)
            {
                continue;
            }

            if (!widgetProxy->GetMaterialProxy() || !widgetProxy->m_VertexBuffer || !widgetProxy->m_VertexInputLayout)
            {
                continue;
            }

            // Proxy stores Canvas reference-space rect; map to viewport pixels.
            Vector2 topLeftPx = widgetProxy->m_TopLeftPx;
            Vector2 sizePx = widgetProxy->m_SizePx;
            float letterboxScale = 1.0f;
            if (widgetProxy->m_CanvasScaleMode == EUICanvasScaleMode::Letterbox)
            {
                const Vector2 ref = widgetProxy->m_CanvasReferenceResolution;
                const ScreenUICoords::LetterboxMapping mapping = ScreenUICoords::MakeLetterboxMapping(
                    ref.x,
                    ref.y,
                    static_cast<float>(viewportWidth),
                    static_cast<float>(viewportHeight));
                topLeftPx = mapping.MapPoint(widgetProxy->m_TopLeftPx);
                sizePx = mapping.MapSize(widgetProxy->m_SizePx);
                letterboxScale = mapping.Scale;
            }

            UIDrawCommand command;
            command.StableOrder = widgetProxy->m_StableOrder;
            command.CanvasSortOrder = widgetProxy->m_CanvasSortOrder;
            // The handles travel with the command: the proxy can be removed before this draw is
            // submitted, and the geometry it names has to outlive it.
            command.Draw.m_VertexBufferRef = widgetProxy->m_VertexBufferRef;
            command.Draw.m_IndexBufferRef = widgetProxy->m_IndexBufferRef;
            command.Draw.m_VertexBuffer = widgetProxy->m_VertexBuffer;
            command.Draw.m_IndexBuffer = widgetProxy->m_IndexBuffer;
            command.Draw.m_IndexCount = widgetProxy->m_IndexCount;
            command.Draw.m_VertexCount = widgetProxy->m_VertexCount;
            // Same rule one level down: the layout says how the vertex buffer is read, and a
            // recorded stream names it after this proxy is gone.
            command.Draw.m_VertexInputLayoutRef = widgetProxy->m_VertexInputLayoutRef;
            command.Draw.m_VertexInputLayout = widgetProxy->m_VertexInputLayoutRef
                ? widgetProxy->m_VertexInputLayoutRef.get()
                : widgetProxy->m_VertexInputLayout;
            command.Draw.m_MaterialProxy = widgetProxy->GetMaterialProxy();
            if (widgetProxy->m_bLocalPixelMesh)
            {
                command.Draw.m_ModelMatrix =
                    ScreenUICoords::MakeWidgetLocalPixelModelMatrix(topLeftPx, letterboxScale);
            }
            else
            {
                command.Draw.m_ModelMatrix = ScreenUICoords::MakeWidgetModelMatrix(topLeftPx, sizePx);
            }
            command.Draw.m_CastShadow = false;
            ctx.ScreenUIQueue.push_back(command);
        }

        std::stable_sort(
            ctx.ScreenUIQueue.begin(),
            ctx.ScreenUIQueue.end(),
            [](const UIDrawCommand& a, const UIDrawCommand& b)
            {
                if (a.CanvasSortOrder != b.CanvasSortOrder)
                {
                    return a.CanvasSortOrder < b.CanvasSortOrder;
                }
                return a.StableOrder < b.StableOrder;
            });
    }
}

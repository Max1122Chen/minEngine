#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Geometry/Frustum.h"
#include "Runtime/Function/Render/DrawCommands/MeshBatchCollector.h"
#include "Runtime/Function/Render/DrawCommands/MeshCollectContext.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/MeshPassProcessor.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"
#include "Runtime/Function/Render/RenderScene.h"
#include "Runtime/Function/Render/SceneRenderContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * Primitive render queue assembly, cut into proxy-index chunks.
 *
 * The split exists so that the per-proxy work can later be handed to workers without touching the
 * code that produces it: one chunk is assembled from a read-only view of the scene and writes only
 * into itself, and the merge is the single place that decides order. Neither the chunk assembly
 * entry nor its inputs name an RHI command list or the renderer type, so a function that only
 * assembles a chunk cannot reach the GPU or the frame recorder even by accident.
 *
 * These are inline functions in a header for the reason the queue builders next door are: the
 * assemblies are plain data, and a headless test has to be able to call them without the frame
 * recorder being stood up or a new export being spent on test reachability.
 */
namespace minEngine
{
    /** Everything a chunk assembly may read. Holds no mutable member and no owner. */
    struct MeshAssemblyInput
    {
        /**
         * The draw set, borrowed. Never written here: the render role owns this vector and is the
         * only role that mutates it, and assembly runs in a window where that role is not mutating.
         */
        const std::vector<PrimitiveSceneProxy*>* PrimitiveProxies = nullptr;

        /** Null disables main-view frustum culling, which is how a shadow-only pass runs. */
        const Math::Geometry::Frustum* MainFrustum = nullptr;

        /** From the frame's draw flags: decides whether a proxy outside the view still casts. */
        bool bEnableShadows = true;

        uint32_t GetProxyCount() const
        {
            return PrimitiveProxies != nullptr ? static_cast<uint32_t>(PrimitiveProxies->size()) : 0u;
        }

        static MeshAssemblyInput FromContext(
            const SceneRenderContext& ctx,
            const Math::Geometry::Frustum* mainFrustum)
        {
            MeshAssemblyInput input;
            input.PrimitiveProxies = ctx.Scene != nullptr ? &ctx.Scene->m_PrimitiveSceneProxies : nullptr;
            input.MainFrustum = mainFrustum;
            input.bEnableShadows = ctx.bEnableShadows;
            return input;
        }
    };

    /**
     * One chunk's output: the proxy interval it covers plus the commands it produced, bucketed per
     * pass.
     *
     * The two shadow buckets are separate because the queue they merge into already has two
     * segments - casters of main-view primitives, then casters that were only collected for the
     * shadow pass. Their relative order is part of what the queues mean, not an implementation
     * detail, so the split is kept in the data instead of being flattened per chunk.
     */
    struct MeshAssemblyChunk
    {
        uint32_t FirstProxy = 0;
        uint32_t ProxyCount = 0;

        std::vector<MeshDrawCommand> Opaque;
        std::vector<MeshDrawCommand> Translucent;
        std::vector<MeshDrawCommand> ShadowFromMainVisible;
        std::vector<MeshDrawCommand> ShadowFromHidden;

        /** Proxies this chunk walked, including the ones it skipped. Proves the chunk ran. */
        uint32_t VisitCount = 0;

        void Clear()
        {
            Opaque.clear();
            Translucent.clear();
            ShadowFromMainVisible.clear();
            ShadowFromHidden.clear();
            VisitCount = 0;
        }
    };

    /** How the caller wants the proxy range split. One is the serial shape. */
    struct MeshAssemblySchedule
    {
        int32_t ChunkCount = 1;

        uint32_t GetChunkCount() const
        {
            return ChunkCount > 0 ? static_cast<uint32_t>(ChunkCount) : 1u;
        }
    };

    /**
     * Splits a proxy count into chunks that tile it exactly: no gap, no overlap, no empty chunk.
     *
     * Chunks differ in size by at most one proxy, and asking for more chunks than there are
     * proxies yields one chunk per proxy rather than empty ones - an empty chunk would make
     * "this chunk ran" indistinguishable from "this chunk had nothing to do".
     */
    inline void ComputeMeshAssemblyChunkRanges(
        uint32_t proxyCount,
        uint32_t requestedChunkCount,
        std::vector<MeshAssemblyChunk>& outChunks)
    {
        outChunks.clear();
        if (proxyCount == 0)
        {
            return;
        }

        const uint32_t chunkCount = std::max(1u, std::min(requestedChunkCount, proxyCount));
        outChunks.resize(chunkCount);

        const uint32_t proxiesPerChunk = proxyCount / chunkCount;
        const uint32_t remainder = proxyCount % chunkCount;

        uint32_t firstProxy = 0;
        for (uint32_t index = 0; index < chunkCount; ++index)
        {
            const uint32_t proxyCountInChunk = proxiesPerChunk + (index < remainder ? 1u : 0u);
            outChunks[index].FirstProxy = firstProxy;
            outChunks[index].ProxyCount = proxyCountInChunk;
            firstProxy += proxyCountInChunk;
        }
    }

    /**
     * Assembles one chunk: visibility cull, mesh collection and the per-pass conversion, for the
     * proxy interval the chunk names.
     *
     * Every proxy takes exactly one of the two collection routes - main view, or shadow-only - and a
     * proxy that neither a frustum nor the shadow flag admits produces nothing at all. All three
     * decisions are read from the frame's inputs, so the branch a proxy takes does not depend on
     * which chunk it landed in or on how many chunks there are.
     */
    inline void AssembleMeshAssemblyChunk(const MeshAssemblyInput& input, MeshAssemblyChunk& chunk)
    {
        chunk.Clear();

        const std::vector<PrimitiveSceneProxy*>* const primitives = input.PrimitiveProxies;
        if (primitives == nullptr)
        {
            return;
        }

        const uint32_t proxyCount = static_cast<uint32_t>(primitives->size());
        const uint32_t chunkEnd = static_cast<uint32_t>(
            std::min<size_t>(proxyCount, static_cast<size_t>(chunk.FirstProxy) + chunk.ProxyCount));
        if (chunk.FirstProxy >= proxyCount)
        {
            return;
        }

        const bool cullEnabled = input.MainFrustum != nullptr;

        // Two collectors, as the single-pass loop always used: a proxy that failed the main view
        // but still casts is collected on its own route, and the two routes reach the shadow queue
        // in a fixed order.
        MeshBatchCollector mainCollector;
        MeshBatchCollector shadowOnlyCollector;

        MeshCollectContext mainCollectContext;
        mainCollectContext.Purpose = MeshCollectPurpose::MainView;
        if (cullEnabled)
        {
            mainCollectContext.ViewFrustum = input.MainFrustum;
        }

        MeshCollectContext shadowOnlyCollectContext;
        shadowOnlyCollectContext.Purpose = MeshCollectPurpose::ShadowOnly;
        // No main-view frustum: sub-culling would drop casters outside the camera cone.

        for (uint32_t proxyIndex = chunk.FirstProxy; proxyIndex < chunkEnd; ++proxyIndex)
        {
            ++chunk.VisitCount;

            PrimitiveSceneProxy* primitiveProxy = (*primitives)[proxyIndex];

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
                bMainViewVisible = input.MainFrustum->Intersect(worldBounds);
            }

            const bool bWantShadowCollect = input.bEnableShadows && primitiveProxy->m_CastShadow;
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

        OpaqueMeshPassProcessor opaqueProcessor(chunk.Opaque);
        TranslucentMeshPassProcessor translucentProcessor(chunk.Translucent);
        ShadowDepthMeshPassProcessor mainVisibleShadowProcessor(chunk.ShadowFromMainVisible);
        ShadowDepthMeshPassProcessor hiddenShadowProcessor(chunk.ShadowFromHidden);

        mainCollector.DispatchTo(opaqueProcessor);
        mainCollector.DispatchTo(translucentProcessor);
        mainCollector.DispatchTo(mainVisibleShadowProcessor);
        shadowOnlyCollector.DispatchTo(hiddenShadowProcessor);
    }

    /**
     * Merges the chunks into the frame's queues.
     *
     * The order is decided here and nowhere else: chunks are appended by ascending `FirstProxy`,
     * never in the order they happen to arrive. With that rule the merged sequence is the same
     * sequence a single pass over all proxies produces, which is what the sorts downstream rely on
     * - they are stable, so equal keys keep the order the commands were inserted in.
     *
     * The shadow queue is filled in two sweeps for the same reason the single-pass loop dispatched
     * its two collectors one after the other: every main-view caster precedes every shadow-only
     * caster.
     */
    inline void MergeMeshAssemblyChunks(
        const std::vector<MeshAssemblyChunk>& chunks,
        SceneRenderContext& ctx)
    {
        ctx.OpaqueQueue.clear();
        ctx.TranslucentQueue.clear();
        ctx.ShadowCasterQueue.clear();

        std::vector<const MeshAssemblyChunk*> orderedChunks;
        orderedChunks.reserve(chunks.size());
        for (const MeshAssemblyChunk& chunk : chunks)
        {
            orderedChunks.push_back(&chunk);
        }
        std::stable_sort(
            orderedChunks.begin(),
            orderedChunks.end(),
            [](const MeshAssemblyChunk* left, const MeshAssemblyChunk* right)
            {
                return left->FirstProxy < right->FirstProxy;
            });

        size_t opaqueTotal = 0;
        size_t translucentTotal = 0;
        size_t shadowTotal = 0;
        for (const MeshAssemblyChunk* chunk : orderedChunks)
        {
            opaqueTotal += chunk->Opaque.size();
            translucentTotal += chunk->Translucent.size();
            shadowTotal += chunk->ShadowFromMainVisible.size() + chunk->ShadowFromHidden.size();
        }
        ctx.OpaqueQueue.reserve(opaqueTotal);
        ctx.TranslucentQueue.reserve(translucentTotal);
        ctx.ShadowCasterQueue.reserve(shadowTotal);

        for (const MeshAssemblyChunk* chunk : orderedChunks)
        {
            ctx.OpaqueQueue.insert(ctx.OpaqueQueue.end(), chunk->Opaque.begin(), chunk->Opaque.end());
            ctx.TranslucentQueue.insert(
                ctx.TranslucentQueue.end(),
                chunk->Translucent.begin(),
                chunk->Translucent.end());
        }

        for (const MeshAssemblyChunk* chunk : orderedChunks)
        {
            ctx.ShadowCasterQueue.insert(
                ctx.ShadowCasterQueue.end(),
                chunk->ShadowFromMainVisible.begin(),
                chunk->ShadowFromMainVisible.end());
        }
        for (const MeshAssemblyChunk* chunk : orderedChunks)
        {
            ctx.ShadowCasterQueue.insert(
                ctx.ShadowCasterQueue.end(),
                chunk->ShadowFromHidden.begin(),
                chunk->ShadowFromHidden.end());
        }
    }

    /**
     * Assembles the primitive queues for one view from an explicit read-only input.
     *
     * Chunks are assembled one after another in this slice; the loop is the only thing that has to
     * change for the work to be spread over workers, because a chunk reads nothing the others write.
     */
    inline void BuildMeshAssemblyQueues(
        const MeshAssemblyInput& input,
        SceneRenderContext& ctx,
        const MeshAssemblySchedule& schedule,
        std::vector<MeshAssemblyChunk>* outChunks)
    {
        std::vector<MeshAssemblyChunk> chunks;
        ComputeMeshAssemblyChunkRanges(input.GetProxyCount(), schedule.GetChunkCount(), chunks);

        for (MeshAssemblyChunk& chunk : chunks)
        {
            AssembleMeshAssemblyChunk(input, chunk);
        }

        MergeMeshAssemblyChunks(chunks, ctx);

        if (outChunks != nullptr)
        {
            *outChunks = std::move(chunks);
        }
    }

    /**
     * Queue assembly for a frame's scene context.
     *
     * Kept as the single entry point for the pipeline and the headless tests, so the serial shape is
     * the same code as the chunked one rather than a second traversal.
     */
    inline void BuildPrimitiveRenderQueueChunked(
        SceneRenderContext& ctx,
        const Math::Geometry::Frustum* mainFrustum,
        const MeshAssemblySchedule& schedule,
        std::vector<MeshAssemblyChunk>* outChunks)
    {
        BuildMeshAssemblyQueues(
            MeshAssemblyInput::FromContext(ctx, mainFrustum),
            ctx,
            schedule,
            outChunks);
    }
}

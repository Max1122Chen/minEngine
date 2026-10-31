#pragma once

#include "Core.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Math/Geometry/Frustum.h"
#include "Runtime/Core/Profiling/ProfileMacros.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"
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
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

/**
 * Primitive render queue assembly, cut into proxy-index chunks.
 *
 * The split is what lets the per-proxy work run on workers without touching the code that produces
 * it: one chunk is assembled from a read-only view of the scene and writes only into itself, and the
 * merge is the single place that decides order. Neither the chunk assembly entry nor its inputs name
 * an RHI command list or the renderer type, so a function that only assembles a chunk cannot reach
 * the GPU or the frame recorder even by accident.
 *
 * Handing chunks to workers is a property of the schedule, not of this code: a schedule that names
 * no task system assembles everything in one loop, and the pipeline passes exactly that.
 *
 * Which topology is running is not part of that decision. Single and Multi differ in which threads
 * own the frame roles, not in whether workers exist, so the gate asks the pool (is it running, how
 * many workers) and the draw set (is there enough to spread) - never the mode.
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

        /**
         * The scene the draw set belongs to, when the caller has one.
         *
         * Read for one fact only: whether the frame-end delete point ran while the set was being
         * walked. Null means the caller watches that itself, and every other input stays usable.
         */
        const RenderScene* Scene = nullptr;

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
            input.Scene = ctx.Scene;
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

        /**
         * Proxies this chunk walked, including the ones it skipped.
         *
         * Written once, as the last step of the assembly and only when the whole range was walked.
         * Zero therefore reads as "this chunk was asked for but did not run" - refused by the pool,
         * or thrown out of part-way - which is the only signal the caller can trust, because a
         * pool that declines a fan-out returns without saying so.
         */
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

    /**
     * Draw-set size below which handing the range to workers costs more than it saves.
     *
     * A conservative starting value, not a measured one: the two ends of a fan-out - the submissions
     * and the join - are paid in full whatever a chunk holds, so the threshold has to be a number
     * someone measured. This one only keeps small scenes off the pool in the meantime.
     */
    inline constexpr uint32_t kMeshAssemblyMinProxiesPerChunk = 128;

    /** How the caller wants the proxy range split, and where the work may be handed over. */
    struct MeshAssemblySchedule
    {
        /**
         * Workers to hand chunks to. Null keeps every call serial, which is the shape the pipeline
         * passes: a frame that never opted in cannot reach a worker by accident.
         */
        TaskSystem* Tasks = nullptr;

        /** One is the serial shape: one chunk, one loop, nothing scheduled. */
        int32_t ChunkCount = 1;

        /** Draw-set size below which a fan-out is not worth attempting. */
        uint32_t MinProxiesPerChunk = kMeshAssemblyMinProxiesPerChunk;

        /**
         * Set by the resolver when the caller turned the fan-out off.
         *
         * Kept next to the schedule rather than re-read at the gate: the switch is read once, where
         * the schedule is built, so the reason a frame stayed serial is the reason it was decided
         * for, not a second reading that could disagree with it.
         */
        bool bParallelDisabled = false;

        uint32_t GetChunkCount() const
        {
            return ChunkCount > 0 ? static_cast<uint32_t>(ChunkCount) : 1u;
        }
    };

    /**
     * What the assembly has done so far, and what it had to repair.
     *
     * Per module rather than process-wide: the assembly is header-inline, so this binary and the
     * engine each own the counters of the calls they make. That is the right scope for what only
     * the caller needs (calls, chunk count, the reason it stayed serial), and the wrong scope for
     * one number: a repair the frame summary has to report is accumulated in the engine instead,
     * through `TaskSystem::NoteMeshAssemblyChunksNotRun`, so every module's repair counts once.
     */
    struct MeshAssemblyStats
    {
        /** Assembly calls: one per view per frame. */
        uint64_t Calls = 0;

        /** Calls that handed their chunks to workers. The only proof a fan-out was attempted. */
        uint64_t ParallelCalls = 0;

        /**
         * Chunks a fan-out left unrun and the caller had to assemble itself. Non-zero means the
         * queues were still complete but the parallelism the caller asked for did not happen.
         */
        uint64_t ChunksNotRun = 0;

        uint32_t LastProxyCount = 0;
        uint32_t LastChunkCount = 0;
        uint32_t LastWorkerCount = 0;

        /** Why the last call stayed serial, or null when it fanned out. */
        const char* LastSerialReason = nullptr;
    };

    inline MeshAssemblyStats& GetMeshAssemblyStats()
    {
        static MeshAssemblyStats stats;
        return stats;
    }

    inline void ResetMeshAssemblyStats()
    {
        GetMeshAssemblyStats() = MeshAssemblyStats{};
    }

    /**
     * Whether this module fans its chunks out at all.
     *
     * On by default: the fan-out is the point of the cut, and the switch exists so a host that
     * distrusts it (a profiler session, a driver under investigation, a frame that has to be
     * reproduced byte for byte) can get the single-loop shape back without rebuilding. Off means
     * one chunk, which is the same code path the fan-out ends in - not a second traversal.
     */
    inline int& MeshAssemblyParallelSwitch()
    {
        // -1 follows the environment, 0 forces serial, 1 forces the fan-out. Per module, like the
        // stats: the switch belongs to the code that assembles, and the environment is what a whole
        // process shares.
        static int sSwitch = -1;
        return sSwitch;
    }

    /** Reads MINENGINE_MESH_ASSEMBLY_PARALLEL once; unset or empty means enabled. */
    inline bool ReadMeshAssemblyParallelEnvironment()
    {
        const char* const value = std::getenv("MINENGINE_MESH_ASSEMBLY_PARALLEL");
        if (value == nullptr || value[0] == '\0')
        {
            return true;
        }

        char lowered[8] = {};
        const size_t length = std::min<size_t>(sizeof(lowered) - 1, std::strlen(value));
        for (size_t index = 0; index < length; ++index)
        {
            const char raw = value[index];
            lowered[index] = (raw >= 'A' && raw <= 'Z') ? static_cast<char>(raw - 'A' + 'a') : raw;
        }
        return !(std::strcmp(lowered, "0") == 0 || std::strcmp(lowered, "off") == 0 ||
                 std::strcmp(lowered, "false") == 0 || std::strcmp(lowered, "no") == 0);
    }

    inline bool IsMeshAssemblyParallelEnabled()
    {
        const int switchValue = MeshAssemblyParallelSwitch();
        if (switchValue >= 0)
        {
            return switchValue != 0;
        }
        static const bool sEnvironmentEnabled = ReadMeshAssemblyParallelEnvironment();
        return sEnvironmentEnabled;
    }

    /** Force the fan-out on or off for this module. The environment still decides for a process. */
    inline void SetMeshAssemblyParallelEnabled(bool enabled)
    {
        MeshAssemblyParallelSwitch() = enabled ? 1 : 0;
    }

    /** Give the decision back to the environment. */
    inline void ClearMeshAssemblyParallelOverride()
    {
        MeshAssemblyParallelSwitch() = -1;
    }

    /**
     * Chunks to cut a draw set into for a pool with this many workers.
     *
     * Two chunks per worker: the long tail of a scene is one heavy proxy interval, and with one
     * chunk per worker that interval decides when the join returns. The ceiling is the per-chunk
     * threshold, because a fan-out pays two submissions and a join whatever a chunk holds - below
     * that size the parallelism is a loss no matter how many workers are idle.
     */
    inline uint32_t ComputeMeshAssemblyChunkCount(uint32_t proxyCount, int32_t workerCount)
    {
        if (workerCount <= 1 || proxyCount == 0)
        {
            return 1;
        }

        const uint32_t chunkBudget = std::max<uint32_t>(
            1u,
            (proxyCount + kMeshAssemblyMinProxiesPerChunk - 1u) / kMeshAssemblyMinProxiesPerChunk);
        const uint32_t wanted = static_cast<uint32_t>(workerCount) * 2u;
        return std::max(1u, std::min(wanted, chunkBudget));
    }

    /**
     * The schedule a frame resolves once, before it assembles anything.
     *
     * Resolving here and not per chunk is what keeps the reported reason and the actual shape the
     * same fact: the reason is read off the schedule afterwards, not recomputed from it.
     */
    inline MeshAssemblySchedule ResolveMeshAssemblySchedule(TaskSystem* tasks, uint32_t proxyCount)
    {
        MeshAssemblySchedule schedule;
        schedule.Tasks = tasks;

        if (!IsMeshAssemblyParallelEnabled())
        {
            schedule.ChunkCount = 1;
            schedule.bParallelDisabled = true;
            return schedule;
        }

        const int32_t workerCount = tasks != nullptr ? tasks->GetWorkerCount() : 1;
        schedule.ChunkCount = static_cast<int32_t>(ComputeMeshAssemblyChunkCount(proxyCount, workerCount));
        return schedule;
    }

    /**
     * The contract the assembly entry is called under.
     *
     * Both reports, never refusals: a frame that silently lost its geometry would be the worse bug,
     * so the queues are built either way and the frame summary says what was broken. The role check
     * only asks when a role table exists - a host that never stood one up has no role to hold, and
     * reporting that as a violation would make the check noise in exactly the place it is silent.
     */
    inline void ReportMeshAssemblyEntryContracts()
    {
        if (FrameThreading::HasInstance())
        {
            ME_ENSURE(
                FrameThreading::Get().IsInGameThread() || FrameThreading::Get().IsInRenderingThread(),
                "mesh assembly reads the draw set on a thread that holds the game or render role");
        }
        ME_ENSURE(
            !RHICommandList::IsRecordingWindowOpenOnThisThread(),
            "mesh assembly runs before the frame opens a recording window, never inside one");
    }

    /**
     * Why the draw set could not be held stable while it was read, or null when it was.
     *
     * The proxies are only immutable while the frame that reads them is still running; the
     * frame-end delete is what ends that. Sampling before and after is the whole check, and it is a
     * named function so a caller can apply the same predicate to its own window.
     */
    inline const char* DescribeMeshAssemblySetInstability(
        const MeshAssemblyInput& input,
        uint64_t flushSerialBefore)
    {
        if (input.Scene == nullptr)
        {
            return nullptr;
        }
        return RenderScene::GetProxyDeleteFlushCount() == flushSerialBefore
            ? nullptr
            : "the frame-end proxy delete ran while the draw set was being assembled";
    }

    /** The frame-end delete serial this input can observe, or 0 when it has no scene to watch. */
    inline uint64_t GetMeshAssemblySceneFlushSerial(const MeshAssemblyInput& input)
    {
        return input.Scene != nullptr ? RenderScene::GetProxyDeleteFlushCount() : 0u;
    }

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

        uint32_t visitedProxyCount = 0;

        for (uint32_t proxyIndex = chunk.FirstProxy; proxyIndex < chunkEnd; ++proxyIndex)
        {
            ++visitedProxyCount;

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

        // Last write of the assembly, deliberately: everything that can go wrong - a pool that
        // refuses the body, a throw from a proxy or a pass processor - happens before this line, so
        // the caller can read a zero as "this chunk did not run" and assemble it itself.
        chunk.VisitCount = visitedProxyCount;
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
     * Why this call has to stay serial, or null when its chunks may go to workers.
     *
     * The decision is taken here, once per call, before anything is scheduled: it asks whether there
     * is work to spread and whether the pool can take it, and nothing about who the caller is. That
     * last question is answered by the task system when the fan-out is attempted, and its answer is
     * read back afterwards from the chunks themselves - re-stating the entitlement rule here would
     * only give it two places to drift apart in.
     */
    inline const char* GetMeshAssemblySerialReason(
        const MeshAssemblyInput& input,
        const MeshAssemblySchedule& schedule)
    {
        if (schedule.bParallelDisabled)
        {
            return "parallel assembly turned off";
        }
        if (schedule.Tasks == nullptr)
        {
            return "no task system";
        }
        if (!schedule.Tasks->IsRunning())
        {
            return "task system not running";
        }
        if (schedule.Tasks->GetWorkerCount() <= 1)
        {
            return "one worker";
        }
        if (schedule.GetChunkCount() <= 1)
        {
            return "one chunk";
        }

        const uint32_t proxyCount = input.GetProxyCount();
        // Two proxies is the smallest draw set that still yields two non-empty chunks, and a
        // fan-out of one chunk would pay for scheduling to run exactly what the loop runs.
        if (proxyCount < 2u)
        {
            return "fewer than two proxies";
        }
        if (proxyCount < schedule.MinProxiesPerChunk * 2u)
        {
            return "too few proxies to spread";
        }
        return nullptr;
    }

    /**
     * Reports a fan-out that left chunks unrun. Rate limited: a refused fan-out is refused on every
     * call that attempts one, and a line per call would bury the refusal under its own repeats.
     * The counter keeps moving either way - the budget bounds the reporting, not the measurement.
     */
    /**
     * Says why a call stayed serial, once per change of reason.
     *
     * A serial frame is the right answer for a small draw set or a one-worker pool, so this is not
     * a loss and not a counter - it is the one line that turns "the frame is not parallel" from a
     * guess into a reading. Transitions only: an editor that runs the same small scene for an hour
     * has one reason, not one line per frame.
     */
    inline void ReportMeshAssemblySerialReason(const char* reason, const MeshAssemblyStats& stats)
    {
        static const char* sLastReason = nullptr;
        if (reason == sLastReason || (reason != nullptr && sLastReason != nullptr &&
                                      std::strcmp(reason, sLastReason) == 0))
        {
            return;
        }
        sLastReason = reason;
        if (reason == nullptr)
        {
            return;
        }

        ME_LOG(
            LogRender,
            Debug,
            "Mesh assembly stayed serial ({}): {} proxies, {} chunk(s), {} worker(s).",
            reason,
            stats.LastProxyCount,
            stats.LastChunkCount,
            stats.LastWorkerCount);
    }

    inline void ReportMeshAssemblyChunksNotRun(uint32_t chunksNotRun, uint32_t firstChunkNotRun, uint32_t chunkCount)
    {
        constexpr uint32_t kReportBudget = 4;

        static uint32_t sReports = 0;
        if (sReports > kReportBudget)
        {
            return;
        }
        ++sReports;
        if (sReports > kReportBudget)
        {
            ME_LOG(
                LogRender,
                Error,
                "Mesh assembly: no further reports of unrun chunks after {}; they are still counted.",
                kReportBudget);
            return;
        }

        ME_LOG(
            LogRender,
            Error,
            "Mesh assembly: {} of {} chunk(s) did not run on a worker (first index {}); assembled them "
            "in place. The queues are complete, but the frame paid for a fan-out it did not get - the "
            "caller may not orchestrate, or a chunk body threw.",
            chunksNotRun,
            chunkCount,
            firstChunkNotRun);
    }

    /**
     * Assembles the primitive queues for one view from an explicit read-only input.
     *
     * One implementation, two schedules: with a task system and more than one chunk the same chunk
     * assembly runs on workers, and every other shape runs it in the loop below. The fan-out is
     * never taken on trust - a pool that declines a body returns without saying so, and a body that
     * throws is caught and dropped by the pool - so the chunks are inspected afterwards and the ones
     * that did not run are assembled in place before the merge. Asking the pool and getting the work
     * done are two different facts, and only the second one makes the queues correct.
     */
    inline void BuildMeshAssemblyQueues(
        const MeshAssemblyInput& input,
        SceneRenderContext& ctx,
        const MeshAssemblySchedule& schedule,
        std::vector<MeshAssemblyChunk>* outChunks)
    {
        // One scope for both shapes: the serial and the fanned-out call are the same measurement,
        // which is what makes their timings comparable at all.
        ME_PROFILE_SCOPE("Render.MeshAssembly");

        ReportMeshAssemblyEntryContracts();
        const uint64_t sceneFlushSerial = GetMeshAssemblySceneFlushSerial(input);

        std::vector<MeshAssemblyChunk> chunks;
        ComputeMeshAssemblyChunkRanges(input.GetProxyCount(), schedule.GetChunkCount(), chunks);

        MeshAssemblyStats& stats = GetMeshAssemblyStats();
        const char* const serialReason = GetMeshAssemblySerialReason(input, schedule);
        stats.Calls += 1;
        stats.LastProxyCount = input.GetProxyCount();
        stats.LastChunkCount = static_cast<uint32_t>(chunks.size());
        stats.LastWorkerCount =
            schedule.Tasks != nullptr ? static_cast<uint32_t>(schedule.Tasks->GetWorkerCount()) : 0u;
        stats.LastSerialReason = serialReason;

        if (serialReason != nullptr)
        {
            ReportMeshAssemblySerialReason(serialReason, stats);
            for (MeshAssemblyChunk& chunk : chunks)
            {
                AssembleMeshAssemblyChunk(input, chunk);
            }
        }
        else
        {
            stats.ParallelCalls += 1;
            schedule.Tasks->ParallelFor(
                static_cast<int32_t>(chunks.size()),
                [&input, &chunks](int32_t index)
                {
                    AssembleMeshAssemblyChunk(input, chunks[static_cast<size_t>(index)]);
                });

            uint32_t chunksNotRun = 0;
            uint32_t firstChunkNotRun = 0;
            for (size_t index = 0; index < chunks.size(); ++index)
            {
                if (chunks[index].VisitCount != 0)
                {
                    continue;
                }

                if (chunksNotRun == 0)
                {
                    firstChunkNotRun = static_cast<uint32_t>(index);
                }
                ++chunksNotRun;
                // Reassembling starts by clearing the chunk, so a body that threw part-way leaves
                // nothing behind that the merge could pick up.
                AssembleMeshAssemblyChunk(input, chunks[index]);
            }

            if (chunksNotRun != 0)
            {
                stats.ChunksNotRun += chunksNotRun;
                // The process-level total the frame summary reads: this module's own counter would
                // be invisible to the frame that reports it.
                TaskSystem::NoteMeshAssemblyChunksNotRun(chunksNotRun);
                ReportMeshAssemblyChunksNotRun(
                    chunksNotRun,
                    firstChunkNotRun,
                    static_cast<uint32_t>(chunks.size()));
            }
        }

        // The draw set is only immutable while this frame runs. Read again on the way out, so a
        // frame end that happened underneath us is a reported contract violation instead of a
        // comment asking the reader to assume it did not.
        ME_ENSURE(
            DescribeMeshAssemblySetInstability(input, sceneFlushSerial) == nullptr,
            "the frame-end proxy delete waits until the assembly that reads the draw set is done");

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

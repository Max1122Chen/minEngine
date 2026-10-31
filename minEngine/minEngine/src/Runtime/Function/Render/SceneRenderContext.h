#pragma once

#include "Core.h"
#include "Runtime/Function/Render/DrawCommands/MeshDrawCommand.h"
#include "Runtime/Function/Render/DrawCommands/UIDrawCommand.h"
#include "Runtime/Function/Render/RenderPipeline/Shadow/ShadowTypes.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    class RenderScene;
    class RenderCamera;
    class SpotLightSceneProxy;
    class PointLightSceneProxy;
    class RHITexture;

    /** One run of commands inside a queue: where it starts and how many commands it covers. */
    struct MeshQueueSegment
    {
        uint32_t First = 0;
        uint32_t Count = 0;

        uint32_t End() const { return First + Count; }
    };

    /**
     * The two runs the shadow caster queue is assembled from, in the order they were merged.
     *
     * The queue is one container because a shadow draw walks it as one list, but the runs in it are
     * not interchangeable: casters of primitives the main view already draws come first, and the
     * ones collected for the shadow pass alone come after them. Naming the runs here keeps that
     * order readable in the data, instead of keeping it alive with a second copy of the queue.
     */
    struct ShadowCasterSegments
    {
        MeshQueueSegment MainVisible;
        MeshQueueSegment Hidden;

        uint32_t Total() const { return MainVisible.Count + Hidden.Count; }

        /** Whether the runs cover this queue exactly: first run first, no gap, no overlap. */
        bool Tiles(size_t queueSize) const
        {
            return MainVisible.First == 0u && Hidden.First == MainVisible.End() &&
                   static_cast<size_t>(Total()) == queueSize;
        }
    };

    /**
     * A pass's borrow of the frame's shadow caster queue.
     *
     * Nothing here owns a command. The queue lives in the frame context that assembled it, and the
     * pipeline keeps that context alive until the frame's recording window has been submitted, so
     * naming it is enough - and a crowded frame hands over a thousand commands instead of copying
     * them once more. Both the storage and the two runs over it are given back on release, so a
     * pass that outlives its frame cannot read a queue that has gone away.
     */
    class ShadowCasterBorrow
    {
    public:
        ShadowCasterBorrow() = default;
        ~ShadowCasterBorrow() { Release(); }

        ShadowCasterBorrow(const ShadowCasterBorrow&) = delete;
        ShadowCasterBorrow& operator=(const ShadowCasterBorrow&) = delete;

        void Bind(const std::vector<MeshDrawCommand>& queue, const ShadowCasterSegments& segments)
        {
            ME_ASSERT(
                segments.Tiles(queue.size()),
                "a shadow caster borrow names the whole queue: the runs have to tile it");
            m_Queue = &queue;
            m_Segments = segments;
            ++s_LiveBorrows;
        }

        void Release()
        {
            if (m_Queue == nullptr)
            {
                return;
            }
            m_Queue = nullptr;
            m_Segments = ShadowCasterSegments{};
            --s_LiveBorrows;
        }

        bool IsBound() const { return m_Queue != nullptr; }
        const std::vector<MeshDrawCommand>* Queue() const { return m_Queue; }
        ShadowCasterSegments Segments() const { return m_Segments; }

        /**
         * The borrowed commands, which the runs above index.
         *
         * An unbound borrow reads as an empty list rather than as a pointer into a frame that has
         * ended; the contract check is what reports the mistake.
         */
        const std::vector<MeshDrawCommand>& Commands() const
        {
            ME_ENSURE(m_Queue != nullptr, "a shadow caster borrow is read only while it is bound");
            static const std::vector<MeshDrawCommand> s_Unbound;
            return m_Queue != nullptr ? *m_Queue : s_Unbound;
        }

        /**
         * Borrows alive in this module.
         *
         * A number that is not zero once a frame has ended is a borrow nobody released, which is
         * the failure this holder exists to make visible instead of leaving as a dangling pointer.
         */
        static uint32_t LiveCount() { return s_LiveBorrows; }

    private:
        const std::vector<MeshDrawCommand>* m_Queue = nullptr;
        ShadowCasterSegments m_Segments{};

        inline static uint32_t s_LiveBorrows = 0;
    };

    /**
     * Ties a borrow to one scope: binds on construction and releases on every way out of it.
     *
     * The frame that hands its casters over has early returns between that hand-over and the end of
     * its recording window, and each of them has to leave the pass naming nothing.
     */
    class ShadowCasterBorrowScope
    {
    public:
        ShadowCasterBorrowScope(
            ShadowCasterBorrow& borrow,
            const std::vector<MeshDrawCommand>& queue,
            const ShadowCasterSegments& segments)
            : m_Borrow(borrow)
        {
            m_Borrow.Bind(queue, segments);
        }

        ~ShadowCasterBorrowScope() { m_Borrow.Release(); }

        ShadowCasterBorrowScope(const ShadowCasterBorrowScope&) = delete;
        ShadowCasterBorrowScope& operator=(const ShadowCasterBorrowScope&) = delete;

    private:
        ShadowCasterBorrow& m_Borrow;
    };

    /**
     * Visits a caster queue in the order the shadow pass draws it: every main-view caster first,
     * then the ones collected for the shadow pass alone.
     *
     * The traversal is clamped to the queue, so a run that names more than the storage holds draws
     * what is there and stops instead of reading past the end.
     */
    template <typename Visit>
    inline void VisitShadowCasters(
        const std::vector<MeshDrawCommand>& casters,
        const ShadowCasterSegments& segments,
        Visit&& visit)
    {
        const auto visitRun = [&](const MeshQueueSegment& run)
        {
            const size_t runEnd = std::min<size_t>(casters.size(), run.End());
            for (size_t index = run.First; index < runEnd; ++index)
            {
                visit(casters[index]);
            }
        };

        visitRun(segments.MainVisible);
        visitRun(segments.Hidden);
    }

    /** Per-Execute transient state (queues, shadow build results). Not stored on ForwardRenderer. */
    struct SceneRenderContext
    {
        RenderScene* Scene = nullptr;
        RenderCamera* Camera = nullptr;

        std::vector<MeshDrawCommand> OpaqueQueue;
        std::vector<MeshDrawCommand> TranslucentQueue;
        /** Casters accepted by ShadowDepthMeshPassProcessor (not a copy of OpaqueQueue). */
        std::vector<MeshDrawCommand> ShadowCasterQueue;
        /** The two runs ShadowCasterQueue holds, in the order the assembly merged them. */
        ShadowCasterSegments ShadowCasters;
        std::vector<UIDrawCommand> ScreenUIQueue;

        std::vector<ShadowRequest> ShadowRequests;
        std::vector<ShadowDrawCommand> ShadowDrawCommands;

        ShadowResourceHandle DirectionalShadowHandle;
        std::vector<ShadowResourceHandle> SpotShadowHandles;
        std::vector<ShadowResourceHandle> PointShadowHandles;
        std::unordered_map<const SpotLightSceneProxy*, ShadowResourceHandle> SpotShadowHandleMap;
        std::unordered_map<const PointLightSceneProxy*, ShadowResourceHandle> PointShadowHandleMap;

        /** From active SkyBox EnvironmentMap (project asset); null if unset. */
        RHITexture* IblIrradianceTexture = nullptr;
        RHITexture* IblPrefilterTexture = nullptr;
        RHITexture* IblBrdfLutTexture = nullptr;

        /** Copied from SceneDrawFlags before BuildRenderQueue (shadowOnly path). */
        bool bEnableShadows = true;

        void ResetFrame()
        {
            Scene = nullptr;
            Camera = nullptr;
            OpaqueQueue.clear();
            TranslucentQueue.clear();
            ShadowCasterQueue.clear();
            ShadowCasters = ShadowCasterSegments{};
            ScreenUIQueue.clear();
            ShadowRequests.clear();
            ShadowDrawCommands.clear();
            DirectionalShadowHandle = ShadowResourceHandle{};
            SpotShadowHandles.clear();
            PointShadowHandles.clear();
            SpotShadowHandleMap.clear();
            PointShadowHandleMap.clear();
            IblIrradianceTexture = nullptr;
            IblPrefilterTexture = nullptr;
            IblBrdfLutTexture = nullptr;
            bEnableShadows = true;
        }
    };
}

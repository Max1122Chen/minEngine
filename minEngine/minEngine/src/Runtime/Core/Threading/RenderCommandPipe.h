#pragma once

#include "Core.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace minEngine
{
    /**
     * Game→Render command pipe and ViewIntent queue.
     * Single and Multi use the same Enqueue API; FlushApply runs on the Render role.
     *
     * Frame-number ownership — read this before touching either frame counter:
     *  - The logical frame number belongs to the Game role. At each tick boundary it calls
     *    PublishGameFrame(completedFrame, currentFrame): the frame it has just finished
     *    simulating, and the frame new commands and intents are tagged with. The Game role is the
     *    only writer of both numbers and of the published world generation.
     *  - The Render role only reads them: GetCompletedGameFrame() is its drain target. The Render
     *    role never advances the logic frame. The frame the Render role has actually applied is
     *    tracked separately and written only by FlushApplyForFrameChecked, so the world-generation
     *    check compares two values with two different writers.
     *  - Single topology (one OS thread playing every role) goes through exactly the same two
     *    entry points. Sharing a thread is not a licence to advance the frame while draining.
     */
    class MINENGINE_API RenderCommandPipe
    {
    public:
        static constexpr size_t kMaxPendingRenderCommands = 4096;
        static constexpr size_t kMaxPendingViewIntents = 3;

        /**
         * How long a producer waits for a free command slot before giving up. Single and Multi
         * share this bound on purpose: in Single the producer is also the only consumer, so waiting
         * for space nobody can free would deadlock the process, and a bounded drop is the only
         * behaviour that is safe in both topologies.
         */
        static constexpr uint32_t kEnqueueWaitTimeoutMs = 100;

        /** Queue health. Each counter names an event that used to be silent (or a hang). */
        struct Counters
        {
            /** Command tagged with a frame the render side already drained; discarded, never applied. */
            uint64_t DroppedStaleCommands = 0;
            /** Command given up because no slot freed within kEnqueueWaitTimeoutMs. */
            uint64_t DroppedFullCommands = 0;
            /** View intent tagged with an already drained frame; discarded on take. */
            uint64_t DroppedStaleViewIntents = 0;
            /** View intent evicted because the intent queue was full (the oldest one loses). */
            uint64_t DroppedFullViewIntents = 0;
            /** Command that threw while being applied during a drain. */
            uint64_t FlushFailureCount = 0;
        };

        static bool HasInstance();
        static RenderCommandPipe& Get();

        void Initialize();
        void Shutdown();

        /**
         * Game role only. Announces a tick boundary: completedFrame is the frame whose commands are
         * now all enqueued and which the render role may drain, currentFrame is the frame new
         * commands and intents will be tagged with. Callers keep
         * currentFrame == completedFrame + 1; the first announcement uses UINT64_MAX ("no frame
         * completed yet") with currentFrame 0.
         */
        void PublishGameFrame(uint64_t completedFrame, uint64_t currentFrame);

        /** Drain target for the Render role: last frame the Game role completed. UINT64_MAX = none yet. */
        uint64_t GetCompletedGameFrame() const { return m_CompletedGameFrame.load(std::memory_order_relaxed); }

        /** Frame new commands and intents are tagged with. Read-only for the Render role. */
        uint64_t GetCurrentGameFrame() const { return m_CurrentGameFrame.load(std::memory_order_relaxed); }

        /** Last frame the Render role drained. UINT64_MAX = nothing drained yet. */
        uint64_t GetDrawnFrame() const { return m_DrawnFrame.load(std::memory_order_relaxed); }

        /** World generation the Game role committed. UINT64_MAX = no world published yet (first frame). */
        uint64_t GetPublishedWorldGen() const { return m_PublishedWorldGen.load(std::memory_order_relaxed); }

        /** Game role only: the world generation committed by the logical tick that just finished. */
        void SetPublishedWorldGen(uint64_t generation);

        /**
         * Compares the generation the Game role published with the frame this pipe has drained for
         * the Render role. The two values have different writers, so a failure means the Render
         * role is drawing a world the Game role never committed. Returns false and fills
         * outDiagnostic (both numbers plus queue depths) when that happens; the caller asserts, and
         * a test can call this directly to reach the trigger without aborting the process.
         */
        bool CheckPublishedGenerationForDrawnFrame(std::string* outDiagnostic) const;

        /**
         * Enqueue a render-role callable tagged with the frame the Game role currently announces
         * (read under the same lock as the queue, so the tag cannot belong to a frame the render
         * side already drained). When the queue is full the producer waits at most
         * kEnqueueWaitTimeoutMs, then the command is dropped and counted instead of blocking
         * forever. A command tagged with a frame that is already behind the drain is dropped and
         * counted on the spot: it could never be applied in order.
         */
        void Enqueue(std::function<void()> command);

        /**
         * Apply every command tagged with a frame at or below frameIndex, in FIFO order, and leave
         * the later frames queued. Commands tagged with a frame the render role already drained are
         * discarded and counted. frameIndex == UINT64_MAX means no game frame has completed yet,
         * so nothing is applied. Returns false when at least one command threw, which means the
         * frame was not cleanly applied; the remaining commands still run so a frame is never
         * half-applied by a single failure.
         */
        bool FlushApplyForFrameChecked(uint64_t frameIndex);

        /** Same, discarding the verdict (callers that do not report frame completeness). */
        void FlushApplyForFrame(uint64_t frameIndex);

        void SubmitViewIntent(const SceneDrawDesc& desc);

        /**
         * Take every intent tagged with a frame at or below frameIndex, in submission order, and
         * leave the later frames queued (the renderer keeps the newest intent per view).
         * frameIndex == UINT64_MAX means no game frame has completed yet: nothing is taken.
         */
        void TakeViewIntents(uint64_t frameIndex, std::vector<SceneDrawDesc>& outIntents);

        Counters GetCounters() const;

    private:
        static RenderCommandPipe* s_Instance;

        struct CommandEntry
        {
            uint64_t FrameIndex = 0;
            std::function<void()> Command;
        };

        struct IntentEntry
        {
            uint64_t FrameIndex = 0;
            SceneDrawDesc Desc{};
        };

        bool m_Initialized = false;

        // Written by the Game role, read by the Render role.
        std::atomic<uint64_t> m_CompletedGameFrame{UINT64_MAX};
        std::atomic<uint64_t> m_CurrentGameFrame{0};
        std::atomic<uint64_t> m_PublishedWorldGen{UINT64_MAX};

        // Written by the Render role while draining; read by the Game role under the matching queue
        // lock to reject tags that the drain already passed.
        std::atomic<uint64_t> m_DrawnFrame{UINT64_MAX};
        std::atomic<uint64_t> m_TakenIntentFrame{UINT64_MAX};

        mutable std::mutex m_CommandMutex;
        std::deque<CommandEntry> m_Commands;
        /** Wakes producers waiting for a slot; guarded by m_CommandMutex. */
        std::condition_variable m_CommandSpaceCv;

        mutable std::mutex m_IntentMutex;
        std::deque<IntentEntry> m_Intents;

        // Written from both roles (the Game role drops on enqueue, the Render role while draining)
        // and read by either role through GetCounters(), so the storage is atomic and the public
        // Counters struct stays a plain snapshot.
        std::atomic<uint64_t> m_DroppedStaleCommands{0};
        std::atomic<uint64_t> m_DroppedFullCommands{0};
        std::atomic<uint64_t> m_DroppedStaleViewIntents{0};
        std::atomic<uint64_t> m_DroppedFullViewIntents{0};
        std::atomic<uint64_t> m_FlushFailureCount{0};
    };

    /** Enqueue onto RenderCommandPipe (creates std::function). */
    MINENGINE_API void EnqueueRenderCommand(std::function<void()> command);

    template<typename TFunctor>
    void EnqueueRenderCommand(TFunctor&& functor)
    {
        EnqueueRenderCommand(std::function<void()>(std::forward<TFunctor>(functor)));
    }
}

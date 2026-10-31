#include "RenderCommandPipe.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

#include <chrono>
#include <exception>
#include <string>
#include <thread>
#include <utility>

namespace minEngine
{
    RenderCommandPipe* RenderCommandPipe::s_Instance = nullptr;

    namespace
    {
        /**
         * The frame a new queue entry belongs to.
         *
         * Both counters are monotonic and are published one after the other, so a producer can
         * sample them a moment apart - the drain target already moved on while the tag still shows
         * the frame before it. Taking the newer of the two keeps the entry on the frame that is
         * really current: the tag can never drop to a frame the render role may already have
         * drained. When the two counters agree, which is every state PublishGameFrame leaves
         * behind, the result is exactly the announced tag.
         */
        uint64_t NewEntryFrame(uint64_t currentFrame, uint64_t completedFrame)
        {
            if (completedFrame == UINT64_MAX)
            {
                // No game frame completed yet: the announced tag is the only number there is.
                return currentFrame;
            }
            return currentFrame > completedFrame ? currentFrame : completedFrame + 1;
        }
    }

    bool RenderCommandPipe::HasInstance()
    {
        return s_Instance != nullptr;
    }

    RenderCommandPipe& RenderCommandPipe::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "RenderCommandPipe is not initialized");
        return *s_Instance;
    }

    void RenderCommandPipe::Initialize()
    {
        ME_ASSERT(s_Instance == nullptr, "RenderCommandPipe is already initialized");
        s_Instance = this;
        m_Initialized = true;

        m_CompletedGameFrame.store(UINT64_MAX, std::memory_order_relaxed);
        m_CurrentGameFrame.store(0, std::memory_order_relaxed);
        m_PublishedWorldGen.store(UINT64_MAX, std::memory_order_relaxed);
        m_DrawnFrame.store(UINT64_MAX, std::memory_order_relaxed);
        m_TakenIntentFrame.store(UINT64_MAX, std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(m_CommandMutex);
            m_Commands.clear();
        }
        {
            std::lock_guard<std::mutex> lock(m_IntentMutex);
            m_Intents.clear();
        }

        m_DroppedStaleCommands.store(0, std::memory_order_relaxed);
        m_DroppedFullCommands.store(0, std::memory_order_relaxed);
        m_DroppedStaleViewIntents.store(0, std::memory_order_relaxed);
        m_DroppedFullViewIntents.store(0, std::memory_order_relaxed);
        m_FlushFailureCount.store(0, std::memory_order_relaxed);
    }

    void RenderCommandPipe::Shutdown()
    {
        if (!m_Initialized)
        {
            return;
        }

        uint64_t discardedCommands = 0;
        uint64_t discardedIntents = 0;
        {
            std::lock_guard<std::mutex> lock(m_CommandMutex);
            discardedCommands = static_cast<uint64_t>(m_Commands.size());
            m_Commands.clear();
        }
        m_CommandSpaceCv.notify_all();
        {
            std::lock_guard<std::mutex> lock(m_IntentMutex);
            discardedIntents = static_cast<uint64_t>(m_Intents.size());
            m_Intents.clear();
        }

        m_Initialized = false;
        s_Instance = nullptr;

        if (discardedCommands != 0 || discardedIntents != 0)
        {
            // Shutdown follows scene teardown, and the commands that teardown enqueued arrive after
            // the render role recorded its last frame - nobody will ever drain them. Counting the
            // depth is what turns "the queue was emptied" into "this much work never happened"; one
            // line per shutdown is enough to find it, and per-entry logging would drown it.
            m_DroppedStaleCommands += discardedCommands;
            m_DroppedStaleViewIntents += discardedIntents;
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: shut down with {} queued command(s) and {} queued view intent(s) "
                "discarded and never applied.",
                discardedCommands,
                discardedIntents);
        }
    }

    void RenderCommandPipe::PublishGameFrame(uint64_t completedFrame, uint64_t currentFrame)
    {
        ME_ASSERT(m_Initialized, "RenderCommandPipe::PublishGameFrame before Initialize");
        if (FrameThreading::HasInstance())
        {
            // Ownership is enforced here: the Game role announces the frame, the Render role only
            // ever reads it. In Single both roles share the thread, so this asserts identity, not
            // thread separation - the call site is what keeps the write on the game side.
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Game, "PublishGameFrame");
        }

        ME_ASSERT(
            (completedFrame == UINT64_MAX && currentFrame == 0) ||
                (completedFrame != UINT64_MAX && currentFrame == completedFrame + 1),
            "PublishGameFrame expects currentFrame == completedFrame + 1");

        // Both stores happen under the two queue locks, so a producer sampling inside either queue
        // sees one whole published pair instead of this call half-way.
        std::scoped_lock publishLock(m_CommandMutex, m_IntentMutex);

        // The order still carries the danger for readers outside those locks - the render role reads
        // the drain target before it drains, and Get*GameFrame() is open to anyone. The tag is
        // therefore published first and the drain target second: a lock-free reader can then only
        // observe a tag ahead of the drain target, never behind it.
        //
        // A tag ahead of the drain target is harmless. An entry is discarded only when its frame is
        // at or behind the frame the render role has already drained, and a frame that has not been
        // published as complete cannot have been drained - so the entry simply waits in FIFO order
        // (FlushApplyForFrameChecked leaves later frames queued) for the drain that reaches it.
        m_CurrentGameFrame.store(currentFrame, std::memory_order_release);
        m_CompletedGameFrame.store(completedFrame, std::memory_order_release);
    }

    void RenderCommandPipe::SetPublishedWorldGen(uint64_t generation)
    {
        ME_ASSERT(m_Initialized, "RenderCommandPipe::SetPublishedWorldGen before Initialize");
        if (FrameThreading::HasInstance())
        {
            // The published generation is a Game-side fact: it says which world the game has
            // committed, so the render role must not be able to move it forward itself.
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Game, "SetPublishedWorldGen");
        }
        m_PublishedWorldGen.store(generation, std::memory_order_release);
    }

    void RenderCommandPipe::Enqueue(std::function<void()> command)
    {
        if (!m_Initialized)
        {
            // Shut down, or never started. There is no drain left that could apply this command, and
            // running it on the caller's thread would touch a render side that is being torn down -
            // so it is refused and counted rather than applied out of band or dropped in silence.
            ++m_DroppedStaleCommands;
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: command refused and discarded - the pipe is not running "
                "(shut down or never initialized), so no drain would ever apply it.");
            return;
        }
        ME_ASSERT(static_cast<bool>(command), "EnqueueRenderCommand requires a callable");

        // No inline pass-through for a producer that happens to be on the render thread. Every
        // topology runs the scene mutations that produce these commands from the game role; running
        // one here instead would bypass the frame tag, the drain comparison and the FIFO order this
        // queue exists to keep, and a caller that really does need something applied immediately has
        // to say so at that call site. Queueing is safe from any thread - the wait for a slot below is
        // bounded, so it cannot deadlock whatever the topology turns out to be.

        // The tag is sampled under the queue lock, together with the drain progress the render role
        // writes under it, and repaired by NewEntryFrame so it cannot name a frame the drain passed.
        std::unique_lock<std::mutex> lock(m_CommandMutex);
        const uint64_t frameIndex = NewEntryFrame(
            m_CurrentGameFrame.load(std::memory_order_relaxed),
            m_CompletedGameFrame.load(std::memory_order_relaxed));
        const uint64_t drawnFrame = m_DrawnFrame.load(std::memory_order_relaxed);
        if (drawnFrame != UINT64_MAX && frameIndex <= drawnFrame)
        {
            // The render role already drained this frame. Applying the command later would be out
            // of order, and the next drain only ever moves forward, so it is dropped here instead
            // of growing the queue until every producer stalls.
            ++m_DroppedStaleCommands;
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: command tagged with frame {} cannot be applied (render already drained "
                "frame {}); dropped. Queue depth {}.",
                frameIndex,
                drawnFrame,
                m_Commands.size());
            return;
        }

        if (m_Commands.size() >= kMaxPendingRenderCommands)
        {
            // Bounded wait, never a spin: in the single topology the producer is also the only
            // consumer, so waiting for a slot that only this thread could free is a deadlock. Both
            // topologies therefore get the same behaviour — wait briefly, then drop and count.
            const bool bSpaceFreed = m_CommandSpaceCv.wait_for(
                lock,
                std::chrono::milliseconds(kEnqueueWaitTimeoutMs),
                [this]() { return m_Commands.size() < kMaxPendingRenderCommands; });
            if (!bSpaceFreed)
            {
                ++m_DroppedFullCommands;
                ME_LOG(
                    LogCore,
                    Error,
                    "RenderCommandPipe: no slot freed within {} ms (queue full at {}); dropping the command for frame {}.",
                    kEnqueueWaitTimeoutMs,
                    kMaxPendingRenderCommands,
                    frameIndex);
                return;
            }
        }
        m_Commands.push_back(CommandEntry{frameIndex, std::move(command)});
    }

    bool RenderCommandPipe::FlushApplyForFrameChecked(uint64_t frameIndex)
    {
        ME_ASSERT(m_Initialized, "RenderCommandPipe::FlushApplyForFrameChecked before Initialize");
        if (FrameThreading::HasInstance())
        {
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Render, "FlushApplyForFrame");
        }

        if (frameIndex == UINT64_MAX)
        {
            // No game frame has completed yet: there is no world to draw and nothing may be
            // applied, so the queue stays untouched and no frame is claimed as drawn.
            return true;
        }

        std::deque<CommandEntry> toRun;
        uint64_t staleDropped = 0;
        {
            std::lock_guard<std::mutex> lock(m_CommandMutex);
            const uint64_t drawnFrame = m_DrawnFrame.load(std::memory_order_relaxed);
            std::deque<CommandEntry> remaining;
            while (!m_Commands.empty())
            {
                CommandEntry entry = std::move(m_Commands.front());
                m_Commands.pop_front();
                if (entry.FrameIndex > frameIndex)
                {
                    // A later frame: not this drain's business, it stays queued in order.
                    remaining.push_back(std::move(entry));
                }
                else if (drawnFrame != UINT64_MAX && entry.FrameIndex <= drawnFrame)
                {
                    // Behind the drain target: this frame is already over and applying the entry
                    // now would be out of order, so it can never be applied.
                    ++staleDropped;
                }
                else
                {
                    toRun.push_back(std::move(entry));
                }
            }
            m_Commands = std::move(remaining);

            // Render-side progress, written here and nowhere else. What a drain may claim is
            // bounded by the frame the game role announced as complete: a caller that asks for
            // more - the shutdown flush, which runs where the boundary that would have drained the
            // later frames is the one that is not coming - still applies what it finds, but the
            // claim stops at the announced frame.
            //
            // Claiming a frame the game role never published is what once turned a correct flush
            // into lost scene changes: the game role tags its in-flight commands with exactly the
            // frame after the announced one, so a claim of that frame put the tag at or behind the
            // drain and every command of the still-open frame was dropped as unapplicable. The
            // announced frame is published under this lock, so the pair read here is one whole
            // published state and not a torn one.
            const uint64_t announcedFrame = m_CompletedGameFrame.load(std::memory_order_relaxed);
            if (announcedFrame != UINT64_MAX)
            {
                const uint64_t claimableFrame = frameIndex < announcedFrame ? frameIndex : announcedFrame;
                if (drawnFrame == UINT64_MAX || claimableFrame > drawnFrame)
                {
                    m_DrawnFrame.store(claimableFrame, std::memory_order_release);
                }
            }
        }
        m_CommandSpaceCv.notify_all();

        if (staleDropped != 0)
        {
            m_DroppedStaleCommands += staleDropped;
            // One summary per drain: a stalled producer can leave thousands of entries, and a log
            // line per entry would drown the rest of the frame.
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: discarded {} command(s) tagged with an already drained frame while draining frame {}.",
                staleDropped,
                frameIndex);
        }

        bool bAllApplied = true;
        for (size_t index = 0; index < toRun.size(); ++index)
        {
            CommandEntry& entry = toRun[index];
            if (!entry.Command)
            {
                continue;
            }
            // Per-command isolation: one failing command must not strand the rest of the frame,
            // which is exactly how a removal that applied and an addition that did not happen
            // produce a scene nobody asked for.
            try
            {
                entry.Command();
            }
            catch (const std::exception& ex)
            {
                bAllApplied = false;
                ++m_FlushFailureCount;
                ME_LOG(
                    LogCore,
                    Error,
                    "RenderCommandPipe: command {} of {} threw while draining frame {}: {}",
                    index,
                    toRun.size(),
                    frameIndex,
                    ex.what());
            }
            catch (...)
            {
                bAllApplied = false;
                ++m_FlushFailureCount;
                ME_LOG(
                    LogCore,
                    Error,
                    "RenderCommandPipe: command {} of {} threw an unknown exception while draining frame {}.",
                    index,
                    toRun.size(),
                    frameIndex);
            }
        }
        return bAllApplied;
    }

    void RenderCommandPipe::FlushApplyForFrame(uint64_t frameIndex)
    {
        (void)FlushApplyForFrameChecked(frameIndex);
    }

    void RenderCommandPipe::SubmitViewIntent(const SceneDrawDesc& desc)
    {
        if (!m_Initialized)
        {
            // Same rule as a command: an intent nobody will take must not sit in a queue that is
            // being torn down, and it must not disappear without a record either.
            ++m_DroppedStaleViewIntents;
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: view intent refused and discarded - the pipe is not running "
                "(shut down or never initialized), so no render frame would ever take it.");
            return;
        }

        std::lock_guard<std::mutex> lock(m_IntentMutex);
        // Tagged exactly like a command: sampled under the queue lock and repaired by NewEntryFrame,
        // so an intent can never be filed under a frame the render role already took.
        const uint64_t frameIndex = NewEntryFrame(
            m_CurrentGameFrame.load(std::memory_order_relaxed),
            m_CompletedGameFrame.load(std::memory_order_relaxed));
        if (m_Intents.size() >= kMaxPendingViewIntents)
        {
            // The producer is the game thread and must never block here: the intent the renderer
            // would draw next is the newest one, so the oldest is the one to lose.
            ++m_DroppedFullViewIntents;
            ME_LOG(
                LogCore,
                Warn,
                "ViewIntent queue full ({}); dropped the oldest intent to make room.",
                kMaxPendingViewIntents);
            m_Intents.pop_front();
        }
        m_Intents.push_back(IntentEntry{frameIndex, desc});
    }

    void RenderCommandPipe::TakeViewIntents(uint64_t frameIndex, std::vector<SceneDrawDesc>& outIntents)
    {
        outIntents.clear();
        if (frameIndex == UINT64_MAX)
        {
            // No game frame has completed yet: there is nothing this render frame may draw.
            return;
        }

        std::lock_guard<std::mutex> lock(m_IntentMutex);
        const uint64_t takenFrame = m_TakenIntentFrame.load(std::memory_order_relaxed);
        std::deque<IntentEntry> remaining;
        uint64_t staleDropped = 0;
        while (!m_Intents.empty())
        {
            IntentEntry entry = std::move(m_Intents.front());
            m_Intents.pop_front();
            if (entry.FrameIndex > frameIndex)
            {
                remaining.push_back(std::move(entry));
            }
            else if (takenFrame != UINT64_MAX && entry.FrameIndex <= takenFrame)
            {
                // Same rule as commands: an intent for a frame that is already over can never be
                // drawn, and holding it would only push the queue towards a full, producer-stalling state.
                ++staleDropped;
            }
            else
            {
                outIntents.push_back(entry.Desc);
            }
        }
        m_Intents = std::move(remaining);
        if (takenFrame == UINT64_MAX || frameIndex > takenFrame)
        {
            m_TakenIntentFrame.store(frameIndex, std::memory_order_release);
        }

        if (staleDropped != 0)
        {
            m_DroppedStaleViewIntents += staleDropped;
            ME_LOG(
                LogCore,
                Warn,
                "RenderCommandPipe: discarded {} view intent(s) tagged with an already drained frame while taking frame {}.",
                staleDropped,
                frameIndex);
        }
    }

    RenderCommandPipe::Counters RenderCommandPipe::GetCounters() const
    {
        Counters snapshot;
        snapshot.DroppedStaleCommands = m_DroppedStaleCommands.load(std::memory_order_relaxed);
        snapshot.DroppedFullCommands = m_DroppedFullCommands.load(std::memory_order_relaxed);
        snapshot.DroppedStaleViewIntents = m_DroppedStaleViewIntents.load(std::memory_order_relaxed);
        snapshot.DroppedFullViewIntents = m_DroppedFullViewIntents.load(std::memory_order_relaxed);
        snapshot.FlushFailureCount = m_FlushFailureCount.load(std::memory_order_relaxed);
        return snapshot;
    }

    bool RenderCommandPipe::CheckPublishedGenerationForDrawnFrame(std::string* outDiagnostic) const
    {
        // The two numbers come from different writers: the generation is written by the Game role
        // when its logical tick commits the world, the drawn frame by this pipe's own drain on the
        // Render role. Neither side can satisfy the check on its own, which is what makes a
        // failure mean "the render role is drawing a world the game never published".
        const uint64_t drawnFrame = m_DrawnFrame.load(std::memory_order_acquire);
        const uint64_t published = m_PublishedWorldGen.load(std::memory_order_acquire);
        const bool bSane = drawnFrame == UINT64_MAX ||
            (published != UINT64_MAX && published >= drawnFrame);
        if (bSane)
        {
            if (outDiagnostic != nullptr)
            {
                outDiagnostic->clear();
            }
            return true;
        }

        if (outDiagnostic != nullptr)
        {
            size_t commandDepth = 0;
            size_t intentDepth = 0;
            {
                std::lock_guard<std::mutex> lock(m_CommandMutex);
                commandDepth = m_Commands.size();
            }
            {
                std::lock_guard<std::mutex> lock(m_IntentMutex);
                intentDepth = m_Intents.size();
            }
            const uint64_t currentFrame = m_CurrentGameFrame.load(std::memory_order_relaxed);
            *outDiagnostic =
                "RenderCommandPipe world generation check failed: the render role has drawn frame " +
                std::to_string(drawnFrame) + " but the game role has published world generation " +
                (published == UINT64_MAX ? std::string("none") : std::to_string(published)) +
                " (current game frame " + std::to_string(currentFrame) +
                ", command queue depth " + std::to_string(commandDepth) +
                ", view intent queue depth " + std::to_string(intentDepth) +
                "). The render role is drawing a world newer than the one the game role committed.";
        }
        return false;
    }

    void EnqueueRenderCommand(std::function<void()> command)
    {
        if (RenderCommandPipe::HasInstance())
        {
            RenderCommandPipe::Get().Enqueue(std::move(command));
            return;
        }

        // No frame pipe in this process at all: unit tests, tooling and headless scenes run the
        // command inline, which is the only drain they have. That fallback deliberately does not
        // depend on why the pipe is missing - a caller that never had one and a caller whose engine
        // has already shut down look the same from here, and refusing the second would take the
        // first down with it. What must not run inline any more is decided one level down: a pipe
        // that still exists but is no longer running refuses and counts in Enqueue, and Shutdown
        // accounts for the work it discards, so nothing is dropped without a record.
        if (command)
        {
            command();
        }
    }
}

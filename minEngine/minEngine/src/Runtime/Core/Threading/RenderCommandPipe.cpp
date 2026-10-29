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

        {
            std::lock_guard<std::mutex> lock(m_CommandMutex);
            m_Commands.clear();
        }
        m_CommandSpaceCv.notify_all();
        {
            std::lock_guard<std::mutex> lock(m_IntentMutex);
            m_Intents.clear();
        }

        m_Initialized = false;
        s_Instance = nullptr;
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

        // Publish the drain target before the tag new commands will carry: a producer that samples
        // the tag mid-update must never end up ahead of the frame the render role may drain.
        m_CompletedGameFrame.store(completedFrame, std::memory_order_release);
        m_CurrentGameFrame.store(currentFrame, std::memory_order_release);
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
        ME_ASSERT(m_Initialized, "RenderCommandPipe::Enqueue before Initialize");
        ME_ASSERT(static_cast<bool>(command), "EnqueueRenderCommand requires a callable");

        // Dedicated Render OS thread (Multi, later): run inline when already on Render.
        if (FrameThreading::HasInstance())
        {
            FrameThreading& threading = FrameThreading::Get();
            if (threading.IsMulti() && threading.IsInRenderingThread() && !threading.IsInGameThread())
            {
                command();
                return;
            }
        }

        // Read the tag under the queue lock: a frame index sampled outside it can already be older
        // than the frame the render side is draining, and that entry would then never be applied.
        std::unique_lock<std::mutex> lock(m_CommandMutex);
        const uint64_t frameIndex = m_CurrentGameFrame.load(std::memory_order_relaxed);
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
            if (drawnFrame == UINT64_MAX || frameIndex > drawnFrame)
            {
                // Render-side progress, written here and nowhere else.
                m_DrawnFrame.store(frameIndex, std::memory_order_release);
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
        ME_ASSERT(m_Initialized, "RenderCommandPipe::SubmitViewIntent before Initialize");

        std::lock_guard<std::mutex> lock(m_IntentMutex);
        const uint64_t frameIndex = m_CurrentGameFrame.load(std::memory_order_relaxed);
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
        if (!RenderCommandPipe::HasInstance())
        {
            // Unit tests / tooling without Engine frame pipe: run inline.
            if (command)
            {
                command();
            }
            return;
        }
        RenderCommandPipe::Get().Enqueue(std::move(command));
    }
}

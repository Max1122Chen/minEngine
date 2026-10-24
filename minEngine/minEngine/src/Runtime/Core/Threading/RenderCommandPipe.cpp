#include "RenderCommandPipe.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

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
        m_LogicFrameIndex = 0;
        m_PublishedWorldGen = UINT64_MAX;
        m_Commands.clear();
        m_Intents.clear();
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
        {
            std::lock_guard<std::mutex> lock(m_IntentMutex);
            m_Intents.clear();
        }

        m_Initialized = false;
        s_Instance = nullptr;
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

        const uint64_t frameIndex = m_LogicFrameIndex;
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(m_CommandMutex);
                if (m_Commands.size() < kMaxPendingRenderCommands)
                {
                    m_Commands.push_back(CommandEntry{frameIndex, std::move(command)});
                    return;
                }
            }

            ME_LOG(LogCore, Warn, "RenderCommandPipe full ({}); blocking producer.", kMaxPendingRenderCommands);
            // Single topology: Flush is same-thread later — spinning forever would deadlock.
            // Drop-wait: yield until space (Multi consumers drain); Single should rarely hit this.
            std::this_thread::yield();
        }
    }

    void RenderCommandPipe::FlushApplyForFrame(uint64_t frameIndex)
    {
        ME_ASSERT(m_Initialized, "RenderCommandPipe::FlushApplyForFrame before Initialize");
        if (FrameThreading::HasInstance())
        {
            FrameThreading::Get().AssertIsFrameThread(FrameThreadKind::Render, "FlushApplyForFrame");
        }

        std::deque<CommandEntry> toRun;
        {
            std::lock_guard<std::mutex> lock(m_CommandMutex);
            std::deque<CommandEntry> remaining;
            while (!m_Commands.empty())
            {
                CommandEntry entry = std::move(m_Commands.front());
                m_Commands.pop_front();
                if (entry.FrameIndex == frameIndex)
                {
                    toRun.push_back(std::move(entry));
                }
                else
                {
                    remaining.push_back(std::move(entry));
                }
            }
            m_Commands = std::move(remaining);
        }

        for (CommandEntry& entry : toRun)
        {
            if (entry.Command)
            {
                entry.Command();
            }
        }
    }

    void RenderCommandPipe::SubmitViewIntent(const SceneDrawDesc& desc)
    {
        ME_ASSERT(m_Initialized, "RenderCommandPipe::SubmitViewIntent before Initialize");

        const uint64_t frameIndex = m_LogicFrameIndex;
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lock(m_IntentMutex);
                if (m_Intents.size() < kMaxPendingViewIntents)
                {
                    m_Intents.push_back(IntentEntry{frameIndex, desc});
                    return;
                }
            }

            ME_LOG(LogCore, Warn, "ViewIntent queue full ({}); blocking producer.", kMaxPendingViewIntents);
            std::this_thread::yield();
        }
    }

    void RenderCommandPipe::TakeViewIntents(uint64_t frameIndex, std::vector<SceneDrawDesc>& outIntents)
    {
        outIntents.clear();
        std::lock_guard<std::mutex> lock(m_IntentMutex);
        std::deque<IntentEntry> remaining;
        while (!m_Intents.empty())
        {
            IntentEntry entry = std::move(m_Intents.front());
            m_Intents.pop_front();
            if (entry.FrameIndex == frameIndex)
            {
                outIntents.push_back(entry.Desc);
            }
            else
            {
                remaining.push_back(std::move(entry));
            }
        }
        m_Intents = std::move(remaining);
    }

    void RenderCommandPipe::SetPublishedWorldGen(uint64_t generation)
    {
        m_PublishedWorldGen = generation;
    }

    void RenderCommandPipe::AdvanceLogicFrame()
    {
        ++m_LogicFrameIndex;
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

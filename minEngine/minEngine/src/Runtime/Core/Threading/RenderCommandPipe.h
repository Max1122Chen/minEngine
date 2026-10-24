#pragma once

#include "Core.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <vector>

namespace minEngine
{
    /**
     * Game→Render command pipe and ViewIntent queue.
     * Single and Multi use the same Enqueue API; FlushApply runs on the Render role.
     */
    class MINENGINE_API RenderCommandPipe
    {
    public:
        static constexpr size_t kMaxPendingRenderCommands = 4096;
        static constexpr size_t kMaxPendingViewIntents = 3;

        static bool HasInstance();
        static RenderCommandPipe& Get();

        void Initialize();
        void Shutdown();

        uint64_t GetLogicFrameIndex() const { return m_LogicFrameIndex; }
        uint64_t GetPublishedWorldGen() const { return m_PublishedWorldGen; }

        /** Enqueue a render-role callable tagged with the current logic frame. Blocks if full. */
        void Enqueue(std::function<void()> command);

        /** Execute all commands tagged with frameIndex (stable FIFO). */
        void FlushApplyForFrame(uint64_t frameIndex);

        void SubmitViewIntent(const SceneDrawDesc& desc);
        void TakeViewIntents(uint64_t frameIndex, std::vector<SceneDrawDesc>& outIntents);

        void SetPublishedWorldGen(uint64_t generation);
        void AdvanceLogicFrame();

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
        uint64_t m_LogicFrameIndex = 0;
        /** UINT64_MAX = no world published yet (first frame). */
        uint64_t m_PublishedWorldGen = UINT64_MAX;

        std::mutex m_CommandMutex;
        std::deque<CommandEntry> m_Commands;

        std::mutex m_IntentMutex;
        std::deque<IntentEntry> m_Intents;
    };

    /** Enqueue onto RenderCommandPipe (creates std::function). */
    MINENGINE_API void EnqueueRenderCommand(std::function<void()> command);

    template<typename TFunctor>
    void EnqueueRenderCommand(TFunctor&& functor)
    {
        EnqueueRenderCommand(std::function<void()>(std::forward<TFunctor>(functor)));
    }
}

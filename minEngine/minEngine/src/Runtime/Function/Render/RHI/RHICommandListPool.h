#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHICommandList.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace minEngine
{
    class RHI;

    /**
     * Bounded pool of RHICommandLists for Multi Recording → RHI Replay.
     * Depth matches D18 (kMaxInFlightCommandLists = 3).
     */
    class MINENGINE_API RHICommandListPool
    {
    public:
        static constexpr size_t kMaxInFlightCommandLists = 3;

        static bool HasInstance();
        static RHICommandListPool& Get();

        void Initialize(RHI* rhi);
        void Shutdown();

        /** Block if all lists are in-flight (Frame.Render.WaitRHI). */
        RHICommandList* Acquire();

        /** After FinishRecording: enqueue for RHI Replay. */
        void SubmitFinished(RHICommandList* list);

        /**
         * Block until a Finished list is available, or return nullptr if shutting down.
         * Used by the RHI role.
         */
        RHICommandList* WaitTakeFinished();

        /** Non-blocking drain helper. */
        RHICommandList* TryTakeFinished();

        /** After Replay: return list to free pool. */
        void Release(RHICommandList* list);

        void RequestShutdown();

    private:
        static RHICommandListPool* s_Instance;

        RHI* m_RHI = nullptr;
        bool m_Initialized = false;
        bool m_ShuttingDown = false;

        std::mutex m_Mutex;
        std::condition_variable m_FreeCv;
        std::condition_variable m_FinishedCv;

        std::vector<std::unique_ptr<RHICommandList>> m_Owners;
        std::deque<RHICommandList*> m_Free;
        std::deque<RHICommandList*> m_Finished;
    };
}

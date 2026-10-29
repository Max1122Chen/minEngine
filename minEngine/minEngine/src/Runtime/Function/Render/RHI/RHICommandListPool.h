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
     * The depth is small on purpose (one list per in-flight frame), which is why a borrow that is
     * never returned starves every later frame: use Lease, not the raw Acquire/Release pair.
     */
    class MINENGINE_API RHICommandListPool
    {
    public:
        static constexpr size_t kMaxInFlightCommandLists = 3;

        /**
         * RAII borrow of one pooled list. Destroying the lease returns the list to the free pool,
         * so a frame that bails out on a failure path cannot leak its slot; SubmitFinished hands
         * the recording to the RHI side instead of recycling it.
         * A lease must not outlive the pool it came from: stop the role threads that own leases
         * before shutting the pool down.
         */
        class MINENGINE_API Lease
        {
        public:
            Lease() = default;
            ~Lease();

            Lease(Lease&& other) noexcept;
            Lease& operator=(Lease&& other) noexcept;
            Lease(const Lease&) = delete;
            Lease& operator=(const Lease&) = delete;

            bool IsValid() const { return m_List != nullptr; }
            RHICommandList* Get() const { return m_List; }
            RHICommandList* operator->() const { return m_List; }

            /** Recording finished: queue it for RHI Replay. The lease stops owning the list. */
            void SubmitFinished();

        private:
            friend class RHICommandListPool;

            Lease(RHICommandListPool* pool, RHICommandList* list)
                : m_Pool(pool)
                , m_List(list)
            {
            }

            void Recycle();

            RHICommandListPool* m_Pool = nullptr;
            RHICommandList* m_List = nullptr;
        };

        static bool HasInstance();
        static RHICommandListPool& Get();

        void Initialize(RHI* rhi);
        void Shutdown();

        /**
         * Borrow a list. Returns an invalid lease when the pool is shutting down. Blocks while all
         * lists are in flight (Frame.Render.WaitRHI), which is only safe because a lease always
         * comes back.
         */
        Lease AcquireLease();

        /** Block if all lists are in-flight (Frame.Render.WaitRHI). Prefer AcquireLease. */
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

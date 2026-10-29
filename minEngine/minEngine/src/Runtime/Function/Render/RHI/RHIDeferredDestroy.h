#pragma once

#include "Core.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace minEngine
{
    /**
     * Deferred destruction of the native half of a GPU object.
     *
     * The handle layer counts references on the CPU side only: RHIBufferRef and
     * RHIVertexInputLayoutRef are shared_ptr, so the last release can happen anywhere - a game
     * thread dropping a mesh asset, a render thread dropping a proxy. The native object behind
     * the handle (a GL buffer name, a vertex array name, a VkBuffer) must not die there: freeing
     * it needs the drawing context to be current on the calling thread, and the GPU must be done
     * with it. The wrapper therefore posts the native handle here and the RHI role destroys it.
     *
     * When the destroy is allowed is a question about submission, not about counting frames: a
     * handle may be deleted as soon as every command batch that could name it has been handed to
     * the platform. The queue measures exactly that. Every post is stamped with the number of
     * batches already committed ("the batch I was retired after"), and a tail drain commits
     * exactly one more batch and destroys every entry whose stamp predates it. A handle retired
     * while a frame is recording is therefore destroyed at the tail of that same frame's batch -
     * the batch that carried its commands - and a handle retired between batches waits for the
     * next one. Neither case counts frames, so a slow frame and a fast frame are the same thing,
     * and a frame that records across several presents does not inherit a magic delay that
     * happens to be too short.
     *
     * A queue that sat idle is the interesting failure: if the frame loop stops presenting
     * (minimized window, no window, device lost) no batch is ever committed, so nothing is
     * destroyed and nothing may be destroyed. The queue does not invent a boundary in that case.
     * It reports instead - the pending length and the age of the oldest entry are exposed, a
     * rate-limited warning names the condition once it lasts longer than the stall interval, and
     * the two escape hatches stay explicit: the next committed batch, or DrainAll at shutdown.
     *
     * On Vulkan a committed batch is still not enough - a command buffer can still be executing -
     * so the destroy callback there hands the handle to VulkanRHI's fence-bounded retired list.
     * The queue bounds CPU-side visibility of the handle; the fence bounds GPU-side usage.
     *
     * Threading: Post is callable from any thread; the drains belong to the RHI role.
     */
    class RHIDeferredDestroyQueue
    {
    public:
        RHIDeferredDestroyQueue() = default;
        ~RHIDeferredDestroyQueue() = default;

        RHIDeferredDestroyQueue(const RHIDeferredDestroyQueue&) = delete;
        RHIDeferredDestroyQueue& operator=(const RHIDeferredDestroyQueue&) = delete;

        /**
         * Any thread: take ownership of a native handle that nobody references any more.
         *
         * The callback returns true once the native object is gone. Returning false means this
         * moment could not destroy it (no graphics context current on the RHI role) and the
         * handle stays queued for the next batch tail instead of being lost.
         */
        void Post(std::function<bool()> destroy, const char* kind)
        {
            if (!destroy)
            {
                return;
            }

            const uint64_t nowMs = NowMilliseconds();
            size_t pending = 0;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Pending.push_back(
                    Entry{ m_CommittedBatchCount, nowMs, std::move(destroy), kind });
                pending = m_Pending.size();
            }
            m_RetiredCount.fetch_add(1, std::memory_order_relaxed);

            WarnIfStalled(nowMs, pending);
        }

        /**
         * RHI role, immediately after a batch of recorded commands was handed to the platform.
         * Committing the batch is what makes every handle retired before it safe to delete, so
         * they all die here. Off the RHI role the call is refused and the queue stays intact: a
         * callback running on the wrong thread would either touch another thread's context or
         * race the frame that is still recording.
         */
        size_t DrainAtSubmitBatchTail()
        {
            if (!IsOnRHIRole())
            {
                const uint64_t refusals =
                    m_RefusedDrainCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (refusals == 1 || refusals % 100 == 0)
                {
                    ME_LOG(
                        LogRHI,
                        Error,
                        "RHIDeferredDestroyQueue: batch tail refused off the RHI role ({} "
                        "refusal(s)); {} native handle(s) stay queued.",
                        refusals,
                        GetPendingCount());
                }
                return 0;
            }

            std::vector<Entry> batch;
            uint64_t committedBatches = 0;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                ++m_CommittedBatchCount;
                committedBatches = m_CommittedBatchCount;

                for (auto it = m_Pending.begin(); it != m_Pending.end();)
                {
                    if (it->RetiredBatch < committedBatches)
                    {
                        batch.push_back(std::move(*it));
                        it = m_Pending.erase(it);
                    }
                    else
                    {
                        // Retired while this very batch was being committed: no submit can have
                        // carried it out yet, so it waits for the next one.
                        ++it;
                    }
                }
            }

            return RunBatch(batch);
        }

        /**
         * RHI role, for a frame that ended without handing anything to the platform (minimized
         * window, no swapchain, failed submit). Nothing is destroyed - there is no committed
         * batch to justify it - but the frame is counted and the stall watchdog runs, so an
         * indefinitely stalled queue is visible in the counters and in the log instead of being
         * silent. The pending handles are not lost: the next committed batch or shutdown drains
         * them.
         */
        size_t NoteFrameWithoutSubmit()
        {
            m_SkippedSubmitCount.fetch_add(1, std::memory_order_relaxed);

            size_t pending = 0;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                pending = m_Pending.size();
            }
            WarnIfStalled(NowMilliseconds(), pending);
            return pending;
        }

        /**
         * Shutdown only, after the backend confirmed the GPU is idle. Ignores the batch criterion
         * and the role check: a device that is being torn down cannot wait for another submit.
         */
        size_t DrainAll()
        {
            std::vector<Entry> batch;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                batch.swap(m_Pending);
            }
            return RunBatch(batch);
        }

        /**
         * Give up on everything still queued, reporting how many native handles leak. Used when
         * the RHI instance dies with handles still pending: running their callbacks would call
         * into a backend that no longer exists.
         */
        size_t AbandonPending(const char* reason)
        {
            std::vector<Entry> dropped;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                dropped.swap(m_Pending);
            }

            if (dropped.empty())
            {
                return 0;
            }

            ME_LOG(
                LogRHI,
                Error,
                "RHIDeferredDestroyQueue: dropping {} native handle(s) undestroyed: {}.",
                dropped.size(),
                reason);
            m_AbandonedCount.fetch_add(static_cast<uint64_t>(dropped.size()), std::memory_order_relaxed);
            return dropped.size();
        }

        bool HasPending() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return !m_Pending.empty();
        }

        /** Handles waiting for a batch tail right now. */
        size_t GetPendingCount() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Pending.size();
        }

        /** How long the oldest queued handle has been waiting, in milliseconds. */
        uint64_t GetOldestPendingAgeMs() const
        {
            const uint64_t nowMs = NowMilliseconds();
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Pending.empty())
            {
                return 0;
            }
            return nowMs > m_Pending.front().PostedAtMs ? nowMs - m_Pending.front().PostedAtMs : 0;
        }

        /** Native handles posted for deferred destruction since construction. */
        uint64_t GetRetiredCount() const { return m_RetiredCount.load(std::memory_order_relaxed); }
        /** Native handles actually destroyed by a drain. */
        uint64_t GetDestroyedCount() const { return m_DestroyedCount.load(std::memory_order_relaxed); }
        /** Handles a callback could not destroy at its first tail and had to keep queued. */
        uint64_t GetDeferredCount() const { return m_DeferredCount.load(std::memory_order_relaxed); }
        uint64_t GetRefusedDrainCount() const { return m_RefusedDrainCount.load(std::memory_order_relaxed); }
        uint64_t GetAbandonedCount() const { return m_AbandonedCount.load(std::memory_order_relaxed); }
        /** Command batches handed to the platform so far; the clock the criterion is measured in. */
        uint64_t GetCommittedBatchCount() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_CommittedBatchCount;
        }
        /** Frames that ended without a commit, i.e. opportunities the criterion did not get. */
        uint64_t GetSkippedSubmitCount() const
        {
            return m_SkippedSubmitCount.load(std::memory_order_relaxed);
        }
        /** Warnings emitted for handles that waited longer than the stall interval. */
        uint64_t GetStallWarnCount() const { return m_StallWarnCount.load(std::memory_order_relaxed); }

        /**
         * How long a queued handle may wait before the queue warns about the missing submit.
         * Zero means "warn on every evaluation". Tests set it explicitly; the default is long
         * enough that a hitch does not produce a line but a minimized window does.
         */
        void SetStallWarnIntervalMs(uint64_t intervalMs)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_StallWarnIntervalMs = intervalMs;
        }

        // ---------------------------------------------------------------- global route
        //
        // A resource does not carry the queue it belongs to: the reference count lives in the
        // resource and the queue belongs to the RHI device, so the RHI registers the queue it
        // owns and every late destructor posts through this one entry point. That keeps "who may
        // destroy this" a property of the device instead of a pointer hidden inside every buffer.

        /**
         * Called by the RHI instance that owns the queue. Only one device drives the graphics
         * API at a time; a second registration while the first is still alive is a contract
         * violation, reported rather than silently mis-routed.
         */
        static void InstallActiveQueue(const std::shared_ptr<RHIDeferredDestroyQueue>& queue)
        {
            std::lock_guard<std::mutex> lock(RegistryMutex());
            std::shared_ptr<RHIDeferredDestroyQueue> previous = s_ActiveQueue.lock();
            if (previous && previous != queue)
            {
                ME_LOG(
                    LogRHI,
                    Error,
                    "RHIDeferredDestroyQueue: a second RHI registered while another was still "
                    "alive; native handles of the older device now route to the newer queue.");
            }
            s_ActiveQueue = queue;
        }

        /** Called by the owning RHI while it is being torn down, before its queue is abandoned. */
        static void UninstallActiveQueue(const RHIDeferredDestroyQueue* queue)
        {
            std::lock_guard<std::mutex> lock(RegistryMutex());
            std::shared_ptr<RHIDeferredDestroyQueue> current = s_ActiveQueue.lock();
            if (current && current.get() == queue)
            {
                s_ActiveQueue.reset();
            }
        }

        /** The queue a resource destructor should post to, or null when no RHI is alive. */
        static std::shared_ptr<RHIDeferredDestroyQueue> GetActiveQueue()
        {
            std::lock_guard<std::mutex> lock(RegistryMutex());
            return s_ActiveQueue.lock();
        }

        /**
         * The global entry point a resource destructor uses. Returns false - and counts - when no
         * RHI is alive to destroy the handle: that is the one case where running the callback
         * would be undefined rather than deferred, so the leak is reported instead of hidden.
         */
        static bool PostToActiveQueue(std::function<bool()> destroy, const char* kind)
        {
            if (!destroy)
            {
                return false;
            }

            if (std::shared_ptr<RHIDeferredDestroyQueue> queue = GetActiveQueue())
            {
                queue->Post(std::move(destroy), kind);
                return true;
            }

            s_PostedWithoutQueueCount.fetch_add(1, std::memory_order_relaxed);
            ME_LOG(
                LogRHI,
                Error,
                "{}: last reference dropped while no RHI was alive to destroy it; the native "
                "object leaks (it is not destroyed on this thread).",
                kind != nullptr ? kind : "gpu object");
            return false;
        }

        /** Handles that were dropped with no live RHI at all; refused, never destroyed. */
        static uint64_t GetPostedWithoutQueueCount()
        {
            return s_PostedWithoutQueueCount.load(std::memory_order_relaxed);
        }

    private:
        struct Entry
        {
            /** Committed batches at post time: the entry dies at the first tail past this. */
            uint64_t RetiredBatch = 0;
            uint64_t PostedAtMs = 0;
            std::function<bool()> Destroy;
            const char* Kind = "gpu object";
        };

        using SteadyClock = std::chrono::steady_clock;

        static uint64_t NowMilliseconds()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    SteadyClock::now().time_since_epoch())
                    .count());
        }

        static bool IsOnRHIRole()
        {
            // Headless callers (tests, offline cook) never install frame roles; there is no role to
            // violate then, and inventing one would make every such caller refuse to destroy.
            return !FrameThreading::HasInstance() || FrameThreading::Get().IsInRHIThread();
        }

        /**
         * Reports a queue that is holding handles because no batch was committed. Rate limited to
         * one line per stall interval: a minimized window would otherwise log every tick.
         */
        void WarnIfStalled(uint64_t nowMs, size_t pending)
        {
            if (pending == 0)
            {
                return;
            }

            uint64_t ageMs = 0;
            uint64_t intervalMs = 0;
            bool bWarn = false;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                if (m_Pending.empty())
                {
                    return;
                }

                const uint64_t oldest = m_Pending.front().PostedAtMs;
                ageMs = nowMs > oldest ? nowMs - oldest : 0;
                intervalMs = m_StallWarnIntervalMs;
                if (ageMs < intervalMs)
                {
                    return;
                }
                if (nowMs - m_LastStallWarnMs >= intervalMs)
                {
                    m_LastStallWarnMs = nowMs;
                    bWarn = true;
                }
            }

            if (!bWarn)
            {
                return;
            }

            m_StallWarnCount.fetch_add(1, std::memory_order_relaxed);
            ME_LOG(
                LogRHI,
                Warn,
                "RHIDeferredDestroyQueue: {} native handle(s) queued, oldest waiting {} ms, no "
                "command batch committed in that time; nothing can be destroyed until the next "
                "submit batch or shutdown.",
                pending,
                ageMs);
        }

        /**
         * Runs the callbacks outside the lock: a callback may destroy a wrapper that owns another
         * native handle and posts it here, which would deadlock against a held lock.
         */
        size_t RunBatch(std::vector<Entry>& batch)
        {
            size_t destroyed = 0;
            std::vector<Entry> stillQueued;
            for (Entry& entry : batch)
            {
                if (entry.Destroy && entry.Destroy())
                {
                    ++destroyed;
                    continue;
                }

                stillQueued.push_back(std::move(entry));
            }

            if (!stillQueued.empty())
            {
                // Reported, not dropped, and not run outside the RHI role: the entries keep their
                // stamp, so every later batch tail retries them.
                ME_LOG(
                    LogRHI,
                    Warn,
                    "RHIDeferredDestroyQueue: {} native handle(s) could not be destroyed at this "
                    "batch tail (no graphics context current); they stay queued.",
                    stillQueued.size());
                m_DeferredCount.fetch_add(
                    static_cast<uint64_t>(stillQueued.size()),
                    std::memory_order_relaxed);

                // Back at the front and still oldest first: a handle that failed once keeps its
                // place instead of looking younger than everything queued behind it.
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Pending.insert(
                    m_Pending.begin(),
                    std::make_move_iterator(stillQueued.begin()),
                    std::make_move_iterator(stillQueued.end()));
            }

            m_DestroyedCount.fetch_add(static_cast<uint64_t>(destroyed), std::memory_order_relaxed);
            return destroyed;
        }

        static std::mutex& RegistryMutex()
        {
            static std::mutex s_Mutex;
            return s_Mutex;
        }

        /** Default long enough that an ordinary hitch is silent and a stalled loop is not. */
        static constexpr uint64_t kDefaultStallWarnIntervalMs = 2000;

        mutable std::mutex m_Mutex;
        std::vector<Entry> m_Pending;
        uint64_t m_CommittedBatchCount = 0;
        uint64_t m_LastStallWarnMs = 0;
        uint64_t m_StallWarnIntervalMs = kDefaultStallWarnIntervalMs;
        std::atomic<uint64_t> m_RetiredCount{ 0 };
        std::atomic<uint64_t> m_DestroyedCount{ 0 };
        std::atomic<uint64_t> m_DeferredCount{ 0 };
        std::atomic<uint64_t> m_RefusedDrainCount{ 0 };
        std::atomic<uint64_t> m_AbandonedCount{ 0 };
        std::atomic<uint64_t> m_SkippedSubmitCount{ 0 };
        std::atomic<uint64_t> m_StallWarnCount{ 0 };

        inline static std::weak_ptr<RHIDeferredDestroyQueue> s_ActiveQueue;
        inline static std::atomic<uint64_t> s_PostedWithoutQueueCount{ 0 };
    };

    /**
     * What a resource destructor calls instead of freeing the native object itself.
     *
     * Both RHIBuffer and RHIVertexInputLayout route through this one function, so a resource kind
     * cannot drift into its own destruction semantics. Returns false when there is no RHI left to
     * destroy it; that leak is reported and counted by the queue.
     */
    inline bool PostNativeDestroy(std::function<bool()> destroy, const char* kind)
    {
        return RHIDeferredDestroyQueue::PostToActiveQueue(std::move(destroy), kind);
    }
}

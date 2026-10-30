#pragma once

#include "Core.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace minEngine
{
    /**
     * The thread that owns the graphics context.
     *
     * Creating a GPU object drives the graphics API, and the API only works where the context is
     * current. Frame roles hand the context to the graphics thread and take it away from the game
     * thread, so "which thread am I on" is the question every create function has to answer
     * before it calls the driver. Both are graphics roles: the two are the same thread whenever
     * the topology combines them, and a backend that splits them still runs both roles' frame
     * bodies before anything is presented.
     */
    namespace RHIGraphicsRole
    {
        inline bool IsCurrent()
        {
            // No roles installed - tests, offline cook, headless tools - means there is no other
            // thread to defer to, so the caller is the graphics role. The same convention the
            // deferred-destroy queue uses, for the same reason: inventing a role would make every
            // headless caller refuse to create anything.
            if (!FrameThreading::HasInstance())
            {
                return true;
            }

            const FrameThreading& threading = FrameThreading::Get();
            // The Single topology binds all three roles to one thread on startup, so its game
            // thread answers true here and takes the immediate path with no added latency.
            return threading.IsInRenderingThread() || threading.IsInRHIThread();
        }
    }

    /** What a posted create request settled as. */
    enum class RHICreateOutcome : uint8_t
    {
        /** The native object now exists. */
        Created,
        /** Nobody is left to use it: the request produced no native object on purpose. */
        NotNeeded,
        /** The native object could not be created here; the request stays queued for the next frame. */
        Retry,
        /**
         * The native object cannot be created at all. The request leaves the queue instead of
         * being retried: a driver rejecting a program fails the same way at every frame start,
         * and retrying it would turn one reportable failure into an endless warning stream.
         */
        Failed,
    };

    /**
     * Marks the create-function overload that returns a handle whose native object does not exist
     * yet. Passing it means "take a copy of the data you need and wait for the graphics role".
     */
    struct DeferredNativeCreate
    {
    };

    /**
     * A GPU object whose native half can be created after the call that asked for it.
     *
     * The handle is CPU-side data - a description, a CPU copy of the initial contents, a copy of
     * the SPIR-V the program is specialized from - and needs no thread. The native object (a GL
     * buffer name, a vertex array, a texture, a linked program) needs the graphics context. A create call from another thread therefore returns a complete handle and
     * hands the native half to the graphics role, which runs it at the start of the next render
     * frame, before that frame records anything that could name it.
     *
     * Consumers can tell the two states apart through IsReady instead of inferring them from a
     * zero native name. A backend whose device calls are not context-affine keeps the defaults:
     * there is nothing left to create later, and every handle is ready.
     */
    class RHIDeferredCreateTarget
    {
    public:
        virtual ~RHIDeferredCreateTarget() = default;

        /**
         * Create the native object. Runs on the graphics role; must be safe to call again after a
         * failure and must do nothing if the object already exists.
         * @return true once the native object exists.
         */
        virtual bool CreateNativeResources() { return true; }

        /**
         * True when the native object exists. A consumer that draws with a handle whose answer is
         * false names nothing: the draw has to be skipped rather than submitted.
         */
        virtual bool IsReady() const { return true; }

        /**
         * True when the native creation ran and failed, whatever a later frame does.
         *
         * Read by a consumer that has to report the failure instead of waiting for an object that
         * is never coming, and by the queue to drop the request instead of retrying it. An object
         * answering true is expected to have said why, once, in the log already: the queue counts
         * the failure and stays silent.
         */
        virtual bool HasNativeCreateFailed() const { return false; }
    };

    /**
     * What a consumer can do with a handle this frame.
     *
     * "No native object yet" and "no native object ever" are the same zero inside the handle and
     * opposite instructions to a consumer: the first is a frame to skip, the second is a failure
     * to report. Reading liveness off IsValid() collapses the two and turns an ordinary queued
     * create into a load failure, which is why the answer is named here instead.
     */
    enum class RHIDeferredUse : uint8_t
    {
        /** The native object exists: name it in this frame. */
        Usable,
        /** Queued for the graphics role. Not an error: skip the frames that cannot name it. */
        Pending,
        /** There is no native object and none is coming: report once and stop trying. */
        Unusable,
    };

    /** The answer for one handle. A null handle is Unusable - there is nothing to wait for. */
    inline RHIDeferredUse GetDeferredUse(const RHIDeferredCreateTarget* target)
    {
        if (target == nullptr || target->HasNativeCreateFailed())
        {
            return RHIDeferredUse::Unusable;
        }
        return target->IsReady() ? RHIDeferredUse::Usable : RHIDeferredUse::Pending;
    }

    /**
     * A consumer's decision for one frame, taken over every handle that frame would name.
     *
     * The two rules it keeps are the two a per-frame caller gets wrong on its own: a queued handle
     * is the ordinary state right after a create off the graphics role, so the frame that skips it
     * must not report a failure; and a failure must not be repeated on every frame that follows.
     * The first skip reports, one skip per kSkipReportInterval frames keeps a long stall visible,
     * and a refusal reports exactly once.
     */
    class RHIDeferredConsumerGate
    {
    public:
        struct Decision
        {
            RHIDeferredUse Use = RHIDeferredUse::Pending;
            /** This call is the one that reports: the first skip, a stall reminder, or the refusal. */
            bool bReport = false;
        };

        /** Frames a consumer may skip in silence between two stall reminders. */
        static constexpr uint32_t kSkipReportInterval = 120;

        /** Ask for one frame, naming every handle that frame would use. */
        Decision Query(std::initializer_list<const RHIDeferredCreateTarget*> targets)
        {
            RHIDeferredUse combined = RHIDeferredUse::Usable;
            for (const RHIDeferredCreateTarget* target : targets)
            {
                const RHIDeferredUse use = GetDeferredUse(target);
                if (use == RHIDeferredUse::Unusable)
                {
                    combined = RHIDeferredUse::Unusable;
                    break;
                }
                if (use == RHIDeferredUse::Pending)
                {
                    combined = RHIDeferredUse::Pending;
                }
            }
            return Record(combined);
        }

        /** Forget what the previous handles reported, for when the caller replaces them. */
        void Reset()
        {
            m_SkippedFrames = 0;
            m_bFailureReported = false;
        }

        /** Frames skipped in a row because the handles were still queued. */
        uint32_t GetSkippedFrames() const { return m_SkippedFrames; }

    private:
        Decision Record(RHIDeferredUse use)
        {
            Decision decision;
            decision.Use = use;
            switch (use)
            {
            case RHIDeferredUse::Usable:
                m_SkippedFrames = 0;
                break;
            case RHIDeferredUse::Unusable:
                decision.bReport = !m_bFailureReported;
                m_bFailureReported = true;
                break;
            case RHIDeferredUse::Pending:
                ++m_SkippedFrames;
                decision.bReport =
                    m_SkippedFrames == 1 || (m_SkippedFrames % kSkipReportInterval) == 0;
                break;
            }
            return decision;
        }

        uint32_t m_SkippedFrames = 0;
        bool m_bFailureReported = false;
    };

    /**
     * Creation requests that were made off the graphics role, in the order they were made.
     *
     * One queue per RHI, mirroring the deferred-destroy queue: the same backend owns both ends of
     * a resource's life, so a handle cannot be created by one device and retired by another. The
     * queue is unbounded because a frame cannot wait for a slot it is not going to get - it is
     * drained once per render frame, before that frame records.
     *
     * Ordering: FIFO. Two resources created in sequence by one thread materialize in that
     * sequence, and a request that cannot create its object yet keeps its place for the next
     * frame instead of moving behind newer ones. A request that can never create it leaves the
     * queue: the object reports why, once, and the handle keeps answering "not ready".
     *
     * A queue that never reaches a frame start is the interesting stall: the requests stay, the
     * handles keep saying they are not ready, and a rate-limited warning names the condition once
     * it has lasted longer than the stall interval - the same watchdog the destroy queue runs for
     * a frame that never submits.
     */
    class RHIDeferredCreateQueue
    {
    public:
        RHIDeferredCreateQueue() = default;
        ~RHIDeferredCreateQueue() = default;

        RHIDeferredCreateQueue(const RHIDeferredCreateQueue&) = delete;
        RHIDeferredCreateQueue& operator=(const RHIDeferredCreateQueue&) = delete;

        /**
         * Any thread: hand over a creation that cannot run on the caller. The callback owns the
         * data it needs; the queue runs it on the graphics role.
         */
        void Post(std::function<RHICreateOutcome()> create, const char* kind)
        {
            if (!create)
            {
                return;
            }

            const uint64_t nowMs = NowMilliseconds();
            size_t pending = 0;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                m_Pending.push_back(Entry{ nowMs, std::move(create), kind });
                pending = m_Pending.size();
            }
            m_PostedCount.fetch_add(1, std::memory_order_relaxed);

            WarnIfStalled(nowMs, pending);
        }

        /**
         * Graphics role, once per render frame, before the frame records: create every native
         * object that was asked for since the previous frame.
         *
         * Running this off the graphics role is refused and counted instead of attempted: the
         * callbacks drive the driver, which would leave the objects at zero again and put every
         * resource back where this queue exists to take it from.
         */
        size_t DrainOnGraphicsRole()
        {
            if (!RHIGraphicsRole::IsCurrent())
            {
                const uint64_t refusals = m_RefusedDrainCount.fetch_add(1, std::memory_order_relaxed) + 1;
                if (refusals == 1 || refusals % 100 == 0)
                {
                    ME_LOG(
                        LogRHI,
                        Error,
                        "RHIDeferredCreateQueue: drain refused off the graphics role ({} refusal(s)); "
                        "{} creation request(s) stay queued.",
                        refusals,
                        GetPendingCount());
                }
                return 0;
            }

            std::vector<Entry> batch;
            {
                std::lock_guard<std::mutex> lock(m_Mutex);
                batch.swap(m_Pending);
            }

            // Outside the lock: a create callback allocates a wrapper that may post work back to
            // this queue (or to the destroy queue) and would deadlock against a held lock.
            std::vector<Entry> retry;
            size_t created = 0;
            size_t notNeeded = 0;
            size_t failed = 0;
            // Requests dropped as unwanted, per kind, for this drain only: the kind is what tells a
            // reader which object stopped being wanted, and a bare total cannot.
            std::vector<std::pair<std::string, size_t>> swallowed;
            for (Entry& entry : batch)
            {
                switch (entry.Create ? entry.Create() : RHICreateOutcome::Created)
                {
                case RHICreateOutcome::Created:
                    ++created;
                    break;
                case RHICreateOutcome::NotNeeded:
                    ++notNeeded;
                    AddKindCount(swallowed, entry.Kind);
                    break;
                case RHICreateOutcome::Failed:
                    // Whoever could not create the object has said so in the log with the reason
                    // the driver gave; a second line from here would only repeat it.
                    ++failed;
                    break;
                case RHICreateOutcome::Retry:
                    retry.push_back(std::move(entry));
                    break;
                }
            }

            if (!retry.empty())
            {
                const size_t stillQueued = retry.size();
                ME_LOG(
                    LogRHI,
                    Warn,
                    "RHIDeferredCreateQueue: {} creation request(s) could not create their native "
                    "object at this frame start; they keep their place for the next one.",
                    stillQueued);
                m_RetriedCount.fetch_add(static_cast<uint64_t>(stillQueued), std::memory_order_relaxed);

                std::lock_guard<std::mutex> lock(m_Mutex);
                // Back at the front, in their original order: a newer request never overtakes an
                // older one.
                m_Pending.insert(
                    m_Pending.begin(),
                    std::make_move_iterator(retry.begin()),
                    std::make_move_iterator(retry.end()));
            }

            m_CreatedCount.fetch_add(static_cast<uint64_t>(created), std::memory_order_relaxed);
            m_NotNeededCount.fetch_add(static_cast<uint64_t>(notNeeded), std::memory_order_relaxed);
            m_FailedCount.fetch_add(static_cast<uint64_t>(failed), std::memory_order_relaxed);
            ReportSwallowed(swallowed);
            return created;
        }

        /**
         * Shutdown: give up on everything still queued and report how many requests never
         * materialized.
         *
         * The requests are dropped, not run. A queued request has no frame left in which its
         * object could be used - the roles are stopping - so creating it would allocate a GPU
         * object only for the destroy queue to retire it immediately, on a device that is about
         * to be torn down. The handles that asked for them stay valid and report "not ready" for
         * the rest of their life, which is exactly what they are.
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
                "RHIDeferredCreateQueue: dropping {} creation request(s) that never reached the "
                "graphics role: {}.",
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

        size_t GetPendingCount() const
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Pending.size();
        }

        /** Creation requests posted from outside the graphics role since construction. */
        uint64_t GetPostedCount() const { return m_PostedCount.load(std::memory_order_relaxed); }
        /** Requests that produced their native object. */
        uint64_t GetCreatedCount() const { return m_CreatedCount.load(std::memory_order_relaxed); }
        /** Requests that produced nothing because nobody held the handle any more. */
        uint64_t GetNotNeededCount() const { return m_NotNeededCount.load(std::memory_order_relaxed); }

        /**
         * The same count, for one kind of object.
         *
         * A total says something was dropped; the kind says what to go and look at. A kind that is
         * dropped while its draw still names it is the defect this counter exists to expose, and a
         * shutdown total cannot tell the two apart.
         */
        uint64_t GetNotNeededCount(const char* kind) const
        {
            if (kind == nullptr)
            {
                return 0;
            }

            std::lock_guard<std::mutex> lock(m_Mutex);
            for (const std::pair<std::string, uint64_t>& entry : m_NotNeededByKind)
            {
                if (entry.first == kind)
                {
                    return entry.second;
                }
            }
            return 0;
        }
        /** Attempts that had to be repeated at a later frame start. */
        uint64_t GetRetriedCount() const { return m_RetriedCount.load(std::memory_order_relaxed); }
        /** Requests whose native creation failed for good; each was reported by its own object once. */
        uint64_t GetFailedCount() const { return m_FailedCount.load(std::memory_order_relaxed); }
        uint64_t GetRefusedDrainCount() const { return m_RefusedDrainCount.load(std::memory_order_relaxed); }
        /** Requests dropped by shutdown without ever creating their native object. */
        uint64_t GetAbandonedCount() const { return m_AbandonedCount.load(std::memory_order_relaxed); }
        /** Stall warnings emitted for requests that waited longer than the stall interval. */
        uint64_t GetStallWarnCount() const { return m_StallWarnCount.load(std::memory_order_relaxed); }

        /** How long the oldest pending request has been waiting, in milliseconds. */
        uint64_t GetOldestPendingAgeMs() const
        {
            const uint64_t nowMs = NowMilliseconds();
            std::lock_guard<std::mutex> lock(m_Mutex);
            if (m_Pending.empty())
            {
                return 0;
            }
            const uint64_t oldest = m_Pending.front().PostedAtMs;
            return nowMs > oldest ? nowMs - oldest : 0;
        }

        /**
         * A long stall is normal for a hitch and abnormal for a loop that never reaches a frame
         * start; shortening the interval only makes a test able to observe the warning.
         */
        void SetStallWarnIntervalMs(uint64_t intervalMs)
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_StallWarnIntervalMs = intervalMs;
        }

    private:
        struct Entry
        {
            uint64_t PostedAtMs = 0;
            std::function<RHICreateOutcome()> Create;
            const char* Kind = "gpu object";
        };

        /** Counts one dropped request under the kind its caller posted it with. */
        static void AddKindCount(std::vector<std::pair<std::string, size_t>>& counts, const char* kind)
        {
            const std::string name = kind != nullptr ? kind : "gpu object";
            for (std::pair<std::string, size_t>& entry : counts)
            {
                if (entry.first == name)
                {
                    ++entry.second;
                    return;
                }
            }
            counts.emplace_back(name, 1);
        }

        /**
         * Adds a drain's dropped requests to the per-kind totals and reports the kind once.
         *
         * Silent dropping is how a swallowed request stayed invisible: the totals line at shutdown
         * shows "492 not needed" and nothing else. The first drop of a kind reports, so a new kind
         * going unwanted is never silent; after that the count speaks, one line per report interval
         * per kind, so a steady stream cannot bury the first.
         */
        void ReportSwallowed(const std::vector<std::pair<std::string, size_t>>& counts)
        {
            if (counts.empty())
            {
                return;
            }

            for (const std::pair<std::string, size_t>& dropped : counts)
            {
                uint64_t total = 0;
                bool bReport = false;
                {
                    std::lock_guard<std::mutex> lock(m_Mutex);
                    uint64_t& slot = FindKindSlot(dropped.first);
                    const uint64_t before = slot;
                    slot += static_cast<uint64_t>(dropped.second);
                    total = slot;

                    if (before == 0)
                    {
                        bReport = true;
                    }
                    else if (slot / kNotNeededReportInterval > before / kNotNeededReportInterval)
                    {
                        bReport = true;
                    }
                }

                if (!bReport)
                {
                    continue;
                }

                ME_LOG(
                    LogRHI,
                    Warn,
                    "RHIDeferredCreateQueue: dropped {} creation request(s) of kind '{}' because "
                    "nothing referenced the handle any more ({} dropped for this kind so far). A "
                    "request is only dropped when the queue is its last holder, so no frame can name "
                    "it afterwards: a draw that still does was holding a handle nobody owned.",
                    dropped.second,
                    dropped.first,
                    total);
            }
        }

        /** The per-kind total for one kind, created on first use. Caller holds the mutex. */
        uint64_t& FindKindSlot(const std::string& kind)
        {
            for (std::pair<std::string, uint64_t>& entry : m_NotNeededByKind)
            {
                if (entry.first == kind)
                {
                    return entry.second;
                }
            }
            m_NotNeededByKind.emplace_back(kind, 0);
            return m_NotNeededByKind.back().second;
        }

        static uint64_t NowMilliseconds()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count());
        }

        /**
         * Reports a queue that is holding requests because no frame start drained them. Rate
         * limited to one line per stall interval, so a frame that never reaches the graphics role
         * is visible once instead of once per post.
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
                "RHIDeferredCreateQueue: {} creation request(s) queued, oldest waiting {} ms, and no "
                "frame start has run on the graphics role in that time; nothing can be created "
                "until it does.",
                pending,
                ageMs);
        }

        mutable std::mutex m_Mutex;
        std::vector<Entry> m_Pending;
        std::atomic<uint64_t> m_PostedCount{ 0 };
        std::atomic<uint64_t> m_CreatedCount{ 0 };
        std::atomic<uint64_t> m_NotNeededCount{ 0 };
        std::atomic<uint64_t> m_RetriedCount{ 0 };
        std::atomic<uint64_t> m_FailedCount{ 0 };
        std::atomic<uint64_t> m_RefusedDrainCount{ 0 };
        std::atomic<uint64_t> m_AbandonedCount{ 0 };
        std::atomic<uint64_t> m_StallWarnCount{ 0 };
        /** Requests dropped as unwanted, per kind; guarded by m_Mutex. */
        std::vector<std::pair<std::string, uint64_t>> m_NotNeededByKind;
        /** One report per kind per this many dropped requests. */
        static constexpr uint64_t kNotNeededReportInterval = 100;
        /** Default long enough that an ordinary hitch is silent and a stalled loop is not. */
        static constexpr uint64_t kDefaultStallWarnIntervalMs = 2000;
        uint64_t m_LastStallWarnMs = 0;
        uint64_t m_StallWarnIntervalMs = kDefaultStallWarnIntervalMs;
    };
}

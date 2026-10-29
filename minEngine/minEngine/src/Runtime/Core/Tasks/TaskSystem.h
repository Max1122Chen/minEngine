#pragma once

#include "EngineAPI.h"
#include "TaskFlowBuilder.h"
#include "TaskHandle.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace minEngine
{
    /**
     * Frame-role events that mean work was lost or a contract was broken: render commands and view
     * intents a role discarded, a command that threw while being applied, frames the roles failed or
     * threw away at shutdown, and contract violations the frame recorded. Sampled once per frame by
     * the caller so one bad frame produces one log line instead of one line per lost item.
     *
     * ContractViolations carries the process-wide total of ME_ENSURE/ME_ASSERT failures, not a
     * per-frame count: a violated thread contract is a fact about the run, and a reader who sees the
     * number move in the frame summary knows to look for the reports that named the site.
     */
    struct FrameWorkLossCounters
    {
        uint64_t DroppedCommands = 0;
        uint64_t DroppedViewIntents = 0;
        uint64_t ApplyFailures = 0;
        uint64_t FailedFrames = 0;
        uint64_t DroppedFrames = 0;
        uint64_t ContractViolations = 0;

        bool operator==(const FrameWorkLossCounters& other) const
        {
            return DroppedCommands == other.DroppedCommands
                && DroppedViewIntents == other.DroppedViewIntents
                && ApplyFailures == other.ApplyFailures
                && FailedFrames == other.FailedFrames
                && DroppedFrames == other.DroppedFrames
                && ContractViolations == other.ContractViolations;
        }

        bool operator!=(const FrameWorkLossCounters& other) const { return !(*this == other); }
    };

    /**
     * One summary line for what was lost between the two snapshots, or an empty string when nothing
     * was. Only non-zero deltas appear, so a normal frame stays silent and a frame that dropped N
     * commands reports a single line carrying N.
     *
     * A counter that moved backwards means a new frame-role instance replaced the old one, not that
     * work was recovered: that reading re-baselines instead of reporting a bogus delta.
     */
    inline std::string DescribeFrameWorkLossDelta(
        const FrameWorkLossCounters& previous,
        const FrameWorkLossCounters& current)
    {
        const uint64_t commandDelta = (current.DroppedCommands > previous.DroppedCommands)
            ? current.DroppedCommands - previous.DroppedCommands
            : 0u;
        const uint64_t intentDelta = (current.DroppedViewIntents > previous.DroppedViewIntents)
            ? current.DroppedViewIntents - previous.DroppedViewIntents
            : 0u;
        const uint64_t applyFailureDelta = (current.ApplyFailures > previous.ApplyFailures)
            ? current.ApplyFailures - previous.ApplyFailures
            : 0u;
        const uint64_t failedFrameDelta = (current.FailedFrames > previous.FailedFrames)
            ? current.FailedFrames - previous.FailedFrames
            : 0u;
        const uint64_t droppedFrameDelta = (current.DroppedFrames > previous.DroppedFrames)
            ? current.DroppedFrames - previous.DroppedFrames
            : 0u;
        const uint64_t contractDelta = (current.ContractViolations > previous.ContractViolations)
            ? current.ContractViolations - previous.ContractViolations
            : 0u;

        std::string line;
        const auto append = [&line](uint64_t delta, const char* label) {
            if (delta == 0)
            {
                return;
            }
            if (!line.empty())
            {
                line += ", ";
            }
            line += std::to_string(delta);
            line += ' ';
            line += label;
        };

        append(commandDelta, "render command(s) dropped");
        append(intentDelta, "view intent(s) dropped");
        append(applyFailureDelta, "render command(s) failed to apply");
        append(failedFrameDelta, "frame(s) failed");
        append(droppedFrameDelta, "queued frame(s) discarded");
        append(contractDelta, "contract violation(s) recorded");

        if (line.empty())
        {
            return {};
        }
        return "Frame work lost: " + line + ".";
    }

    /**
     * Execution facade: TaskFlow workers + Multi frame-role OS threads.
     *
     * - Submit / DrainCompletions: Game thread only; onComplete optional.
     * - Wait: blocks until worker CPU (or flow sink) finishes; NOT until onComplete.
     * - ParallelFor / RunAndWait: Game or Render orchestrator; join; no Drain.
     * - SubmitFlow: small worker DAG; same Wait / optional onComplete as Submit.
     * - Frame roles (Multi): dedicated Render/RHI threads (OpenGL may combine into one
     *   graphics thread that owns the GL context).
     * Workers: pure CPU only — no RHI, NewObject, AssetManager writes, Scene authority,
     *          Lua, ImGui, or TaskSystem control-plane re-entry.
     */
    class MINENGINE_API TaskSystem
    {
    public:
        using WorkFn = std::function<void()>;
        using CompletionFn = std::function<void(bool bSucceeded)>;
        using ParallelBodyFn = std::function<void(int32_t index)>;
        using RenderFrameTaskFn = std::function<void(float deltaTime)>;
        using RHIFrameTaskFn = std::function<void()>;

        /**
         * Frames allowed in flight end to end: from the game role handing a frame over to the frame's
         * RHI work finishing. The render queue, the RHI queue and the frame each role is executing
         * share this one bound, so a slow GPU throttles the game role instead of letting the two
         * drift apart by an unbounded number of frames.
         */
        static constexpr size_t kMaxInFlightFrames = 3;

        TaskSystem();
        ~TaskSystem();

        TaskSystem(const TaskSystem&) = delete;
        TaskSystem& operator=(const TaskSystem&) = delete;
        TaskSystem(TaskSystem&&) = delete;
        TaskSystem& operator=(TaskSystem&&) = delete;

        static TaskSystem& Get();
        static bool HasInstance();
        static void SetInstance(TaskSystem* instance);

        /** Start the worker pool. Records the calling thread as the game thread. */
        void Init(int workerCount = 0);

        /** Game-thread only. Stops frame roles, drains work, destroys pool. */
        void Shutdown();

        /** Game-thread only. Rejected off the game thread (invalid handle). */
        TaskHandle Submit(WorkFn work);
        TaskHandle Submit(WorkFn work, CompletionFn onComplete);

        /** Block until the work / flow finishes (not until onComplete runs). */
        void Wait(TaskHandle handle);

        /** Game-thread only. Dispatch completions already queued at call start. */
        void DrainCompletions();

        /**
         * Fan-out body(i) for i in [0, count). Returns when all finished.
         * Allowed on Game or Render (FrameThreading Render bind). No completion queue.
         */
        void ParallelFor(int32_t count, ParallelBodyFn body);

        /** Run one unit on the pool and join. Game or Render. */
        void RunAndWait(WorkFn work);

        /** Game-thread only. One-shot worker DAG; optional onComplete via Drain. */
        TaskHandle SubmitFlow(TaskFlowBuilder&& builder);
        TaskHandle SubmitFlow(TaskFlowBuilder&& builder, CompletionFn onComplete);

        /**
         * Multi only. Starts Render/RHI role threads (or one combined graphics thread).
         * combineGraphicsThreads: true for OpenGL (single context owner); false for Vulkan.
         */
        void StartFrameRoles(
            RenderFrameTaskFn renderTask,
            RHIFrameTaskFn rhiTask,
            bool combineGraphicsThreads);

        /**
         * Stops accepting new frames, lets the already queued ones run, then joins the role
         * threads. Frames still queued when the bounded drain gives up are counted and logged,
         * never silently reported as drawn.
         */
        void StopFrameRoles();

        /**
         * Render/RHI role body: report that the frame currently executing did not complete, for a
         * failure the body handled itself (for example an isolated command failure). The frame
         * still signals completion so a waiting producer is released, but it counts as failed.
         * A no-op when no frame is executing on this thread.
         */
        void MarkCurrentFrameIncomplete(const char* reason);

        /** Frames whose role body threw or reported incomplete. */
        uint64_t GetFailedFrameCount() const;

        /** Queued frames discarded because the role threads stopped before executing them. */
        uint64_t GetDroppedQueuedFrameCount() const;

        /**
         * Game posts a render frame. Blocks while kMaxInFlightFrames frames are already in flight
         * (queued for either role, or executing). Optional wait until that frame's RHI completes.
         */
        void SubmitRenderFrameTask(float deltaTime, bool waitForCompletion);

        /** Game: block until an in-flight slot frees up (or the roles stop). */
        void WaitForRenderQueueSpace();

        bool IsRunning() const;
        int GetWorkerCount() const;
        std::thread::id GetGameThreadId() const;
        bool AreFrameRolesRunning() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
        static TaskSystem* s_Instance;
    };
}

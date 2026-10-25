#pragma once

#include "EngineAPI.h"
#include "TaskFlowBuilder.h"
#include "TaskHandle.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace minEngine
{
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

        static constexpr size_t kMaxPendingRenderFrames = 3;

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

        void StopFrameRoles();

        /**
         * Game posts a render frame. Blocks while the pending-render queue is full
         * (kMaxPendingRenderFrames). Optional wait until that frame's RHI completes.
         */
        void SubmitRenderFrameTask(float deltaTime, bool waitForCompletion);

        /** Game: block until the pending-render queue has a free slot (or roles stop). */
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

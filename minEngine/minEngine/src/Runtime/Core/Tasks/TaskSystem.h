#pragma once

#include "EngineAPI.h"
#include "TaskHandle.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace minEngine
{
    /**
     * Thin TaskFlow wrapper: Submit CPU work on workers, DrainCompletions on the game thread.
     *
     * Usage contract (engine and game modules):
     * - Game thread = the thread that called Init (normally the logical/frame thread).
     * - Workers may only run pure CPU / local data work. Do not call NewObject, ObjectManager,
     *   AssetManager, Scene, RHI, Lua, ImGui, or Submit/Wait/Drain/Shutdown from a worker.
     * - onComplete runs only on the game thread via DrainCompletions (Engine drains once per
     *   logical tick after Input, before Fixed). Product code should not Drain ad hoc.
     * - Wait covers the worker function only; it does not mean onComplete has run.
     * - This pool is not a substitute for game/render/RHI frame-thread separation.
     */
    class MINENGINE_API TaskSystem
    {
    public:
        using WorkFn = std::function<void()>;
        using CompletionFn = std::function<void(bool bSucceeded)>;

        TaskSystem();
        ~TaskSystem();

        TaskSystem(const TaskSystem&) = delete;
        TaskSystem& operator=(const TaskSystem&) = delete;
        TaskSystem(TaskSystem&&) = delete;
        TaskSystem& operator=(TaskSystem&&) = delete;

        static TaskSystem& Get();
        static bool HasInstance();
        static void SetInstance(TaskSystem* instance);

        /**
         * Start the worker pool. Records the calling thread as the game thread.
         * workerCount <= 0 uses max(1, hardware_concurrency - 1).
         */
        void Init(int workerCount = 0);

        /** Game-thread only. Wait for accepted work, drain remaining completions, destroy pool. */
        void Shutdown();

        /** Game-thread only. Rejected off the game thread (invalid handle). */
        TaskHandle Submit(WorkFn work);
        TaskHandle Submit(WorkFn work, CompletionFn onComplete);

        /** Block until the work function finishes (not until onComplete runs). */
        void Wait(TaskHandle handle);

        /** Game-thread only. Dispatch completions already queued at call start. */
        void DrainCompletions();

        bool IsRunning() const;
        int GetWorkerCount() const;
        std::thread::id GetGameThreadId() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
        static TaskSystem* s_Instance;
    };
}

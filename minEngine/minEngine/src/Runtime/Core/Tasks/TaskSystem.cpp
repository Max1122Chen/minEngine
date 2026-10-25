#include "TaskSystem.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Profiling/Profile.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Function/Render/WindowSystem.h"

#include <taskflow/taskflow.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace minEngine
{
    namespace
    {
        int ResolveWorkerCount(int workerCount)
        {
            if (workerCount > 0)
            {
                return workerCount;
            }
            const unsigned hardware = std::thread::hardware_concurrency();
            if (hardware <= 1)
            {
                return 1;
            }
            return static_cast<int>(hardware - 1u);
        }

    }

    struct TaskSystem::Impl
    {
        enum class State : uint8_t
        {
            Stopped = 0,
            Running,
            ShuttingDown
        };

        struct TaskRecord
        {
            uint64_t Id = 0;
            std::mutex Mutex;
            std::condition_variable Cv;
            bool bWorkFinished = false;
            bool bSucceeded = false;
            CompletionFn OnComplete;
        };

        struct CompletionEvent
        {
            uint64_t Id = 0;
            bool bSucceeded = false;
            CompletionFn OnComplete;
        };

        struct RenderFrameTask
        {
            float DeltaTime = 0.0f;
            bool WaitForCompletion = false;
            uint64_t Sequence = 0;
        };

        struct RHIFrameTask
        {
            uint64_t Sequence = 0;
            bool SignalCompletion = false;
        };

        State StateValue = State::Stopped;
        int WorkerCount = 0;
        std::thread::id GameThreadId{};
        std::unique_ptr<tf::Executor> Executor;
        std::atomic<uint64_t> NextTaskId{1};

        std::mutex TasksMutex;
        std::unordered_map<uint64_t, std::shared_ptr<TaskRecord>> Tasks;

        std::mutex CompletionMutex;
        std::vector<CompletionEvent> Completions;

        bool bFrameRolesRunning = false;
        bool bCombineGraphicsThreads = false;
        std::atomic<bool> bFrameRolesStop{false};
        RenderFrameTaskFn RenderTaskFn;
        RHIFrameTaskFn RHITaskFn;
        std::thread RenderThread;
        std::thread RHIThread;
        std::thread GraphicsThread;

        std::mutex RenderMutex;
        std::condition_variable RenderCv;
        std::deque<RenderFrameTask> RenderTasks;

        std::mutex RHIMutex;
        std::condition_variable RHICv;
        std::deque<RHIFrameTask> RHITasks;

        std::mutex FrameCompleteMutex;
        std::condition_variable FrameCompleteCv;
        uint64_t NextFrameSequence = 1;
        uint64_t CompletedFrameSequence = 0;

        bool IsGameThread() const
        {
            return std::this_thread::get_id() == GameThreadId;
        }

        bool IsAllowedOrchestrator() const
        {
            if (IsGameThread())
            {
                return true;
            }
            if (FrameThreading::HasInstance() && FrameThreading::Get().IsInRenderingThread())
            {
                return true;
            }
            return false;
        }

        void PublishCompletion(const std::shared_ptr<TaskRecord>& record)
        {
            CompletionEvent event;
            event.Id = record->Id;
            event.bSucceeded = record->bSucceeded;
            event.OnComplete = record->OnComplete;

            std::lock_guard<std::mutex> lock(CompletionMutex);
            Completions.push_back(std::move(event));
        }

        void DrainCompletionsInternal()
        {
            std::vector<CompletionEvent> batch;
            {
                std::lock_guard<std::mutex> lock(CompletionMutex);
                batch.swap(Completions);
            }

            for (CompletionEvent& event : batch)
            {
                if (!event.OnComplete)
                {
                    continue;
                }

                try
                {
                    event.OnComplete(event.bSucceeded);
                }
                catch (const std::exception& ex)
                {
                    ME_LOG(LogCore, Error, "TaskSystem: onComplete threw: {}", ex.what());
                }
                catch (...)
                {
                    ME_LOG(LogCore, Error, "TaskSystem: onComplete threw an unknown exception.");
                }
            }
        }

        void FinishRecord(const std::shared_ptr<TaskRecord>& record, bool bSucceeded)
        {
            // Publish before signaling Wait so Drain after Wait always sees the event.
            record->bSucceeded = bSucceeded;
            PublishCompletion(record);
            {
                std::lock_guard<std::mutex> lock(record->Mutex);
                record->bWorkFinished = true;
            }
            record->Cv.notify_all();
        }

        TaskHandle SubmitInternal(WorkFn work, CompletionFn onComplete)
        {
            if (StateValue != State::Running)
            {
                return {};
            }
            if (!IsGameThread())
            {
                ME_LOG(LogCore, Error, "TaskSystem::Submit rejected: not on the game thread.");
                return {};
            }
            if (!Executor || !work)
            {
                return {};
            }

            auto record = std::make_shared<TaskRecord>();
            record->Id = NextTaskId.fetch_add(1, std::memory_order_relaxed);
            record->OnComplete = std::move(onComplete);

            {
                std::lock_guard<std::mutex> lock(TasksMutex);
                Tasks.emplace(record->Id, record);
            }

            const TaskHandle handle{record->Id};
            Executor->silent_async([this, record, work = std::move(work)]() mutable {
                bool bSucceeded = true;
                try
                {
                    work();
                }
                catch (const std::exception& ex)
                {
                    bSucceeded = false;
                    ME_LOG(LogCore, Error, "TaskSystem: work threw: {}", ex.what());
                }
                catch (...)
                {
                    bSucceeded = false;
                    ME_LOG(LogCore, Error, "TaskSystem: work threw an unknown exception.");
                }
                FinishRecord(record, bSucceeded);
            });

            return handle;
        }

        TaskHandle SubmitFlowInternal(TaskFlowBuilder&& builder, CompletionFn onComplete)
        {
            if (StateValue != State::Running)
            {
                return {};
            }
            if (!IsGameThread())
            {
                ME_LOG(LogCore, Error, "TaskSystem::SubmitFlow rejected: not on the game thread.");
                return {};
            }
            if (!Executor)
            {
                return {};
            }
            if (builder.GetNodeCount() == 0)
            {
                ME_LOG(LogCore, Error, "TaskSystem::SubmitFlow rejected: empty flow.");
                return {};
            }
            if (builder.ContainsCycle())
            {
                ME_LOG(LogCore, Error, "TaskSystem::SubmitFlow rejected: cycle in graph.");
                return {};
            }

            auto nodes = std::make_shared<std::vector<TaskFlowBuilder::Node>>(builder.ReleaseNodes());
            for (const TaskFlowBuilder::Node& node : *nodes)
            {
                if (!node.Fn)
                {
                    ME_LOG(LogCore, Error, "TaskSystem::SubmitFlow rejected: empty node.");
                    return {};
                }
            }

            auto record = std::make_shared<TaskRecord>();
            record->Id = NextTaskId.fetch_add(1, std::memory_order_relaxed);
            record->OnComplete = std::move(onComplete);

            {
                std::lock_guard<std::mutex> lock(TasksMutex);
                Tasks.emplace(record->Id, record);
            }

            const TaskHandle handle{record->Id};
            auto flow = std::make_shared<tf::Taskflow>();
            auto succeeded = std::make_shared<std::atomic<bool>>(true);

            std::vector<tf::Task> tfTasks;
            tfTasks.reserve(nodes->size());
            for (size_t i = 0; i < nodes->size(); ++i)
            {
                tfTasks.push_back(flow->emplace([nodes, i, succeeded]() {
                    try
                    {
                        (*nodes)[i].Fn();
                    }
                    catch (const std::exception& ex)
                    {
                        succeeded->store(false, std::memory_order_relaxed);
                        ME_LOG(LogCore, Error, "TaskSystem: flow node threw: {}", ex.what());
                    }
                    catch (...)
                    {
                        succeeded->store(false, std::memory_order_relaxed);
                        ME_LOG(LogCore, Error, "TaskSystem: flow node threw an unknown exception.");
                    }
                }));
            }
            for (size_t i = 0; i < nodes->size(); ++i)
            {
                for (uint32_t succ : (*nodes)[i].Successors)
                {
                    if (succ < tfTasks.size())
                    {
                        tfTasks[i].precede(tfTasks[succ]);
                    }
                }
            }

            // Schedule from the Game thread — do not nest run() inside a worker of this executor.
            Executor->run(*flow, [this, record, flow, nodes, succeeded]() {
                FinishRecord(record, succeeded->load(std::memory_order_relaxed));
            });

            return handle;
        }

        void MakeGraphicsContextCurrentIfNeeded()
        {
            if (WindowSystem::HasInstance())
            {
                WindowSystem::Get().MakeGraphicsContextCurrent();
            }
        }

        void ClearGraphicsContextIfNeeded()
        {
            if (WindowSystem::HasInstance())
            {
                WindowSystem::Get().ClearGraphicsContextCurrent();
            }
        }

        void SignalFrameComplete(uint64_t sequence)
        {
            {
                std::lock_guard<std::mutex> lock(FrameCompleteMutex);
                if (sequence > CompletedFrameSequence)
                {
                    CompletedFrameSequence = sequence;
                }
            }
            FrameCompleteCv.notify_all();
        }

        /** Caller must hold RenderMutex. */
        void WaitForRenderQueueSpaceLocked(std::unique_lock<std::mutex>& lock)
        {
            while (RenderTasks.size() >= TaskSystem::kMaxPendingRenderFrames && !bFrameRolesStop)
            {
                ME_PROFILE_SCOPE("Frame.Game.WaitRenderQueue");
                RenderCv.wait(lock);
            }
        }

        void RunRHIFrameTask(const RHIFrameTask& task)
        {
            if (RHITaskFn)
            {
                ME_PROFILE_SCOPE("Frame.RHI.Replay");
                RHITaskFn();
            }
            if (task.SignalCompletion)
            {
                SignalFrameComplete(task.Sequence);
            }
        }

        void CombinedGraphicsThreadMain()
        {
            FrameThreading::Get().BindCurrentThread(FrameThreadKind::Render);
            FrameThreading::Get().BindCurrentThread(FrameThreadKind::RHI);
            MakeGraphicsContextCurrentIfNeeded();

            while (!bFrameRolesStop)
            {
                RenderFrameTask task{};
                {
                    std::unique_lock<std::mutex> lock(RenderMutex);
                    RenderCv.wait(lock, [&]() {
                        return bFrameRolesStop || !RenderTasks.empty();
                    });
                    if (bFrameRolesStop && RenderTasks.empty())
                    {
                        break;
                    }
                    if (RenderTasks.empty())
                    {
                        continue;
                    }
                    task = RenderTasks.front();
                    RenderTasks.pop_front();
                }
                RenderCv.notify_all();

                ME_PROFILE_SCOPE("Frame.Graphics.Tick");
                if (RenderTaskFn)
                {
                    ME_PROFILE_SCOPE("Frame.Render.Tick");
                    RenderTaskFn(task.DeltaTime);
                }
                RunRHIFrameTask(RHIFrameTask{task.Sequence, task.WaitForCompletion});
            }

            ClearGraphicsContextIfNeeded();
        }

        void RenderThreadMain()
        {
            FrameThreading::Get().BindCurrentThread(FrameThreadKind::Render);

            while (!bFrameRolesStop)
            {
                RenderFrameTask task{};
                {
                    std::unique_lock<std::mutex> lock(RenderMutex);
                    RenderCv.wait(lock, [&]() {
                        return bFrameRolesStop || !RenderTasks.empty();
                    });
                    if (bFrameRolesStop && RenderTasks.empty())
                    {
                        break;
                    }
                    if (RenderTasks.empty())
                    {
                        continue;
                    }
                    task = RenderTasks.front();
                    RenderTasks.pop_front();
                }
                RenderCv.notify_all();

                if (RenderTaskFn)
                {
                    ME_PROFILE_SCOPE("Frame.Render.Tick");
                    RenderTaskFn(task.DeltaTime);
                }

                {
                    std::lock_guard<std::mutex> lock(RHIMutex);
                    RHITasks.push_back(RHIFrameTask{task.Sequence, task.WaitForCompletion});
                }
                RHICv.notify_one();
            }
        }

        void RHIThreadMain()
        {
            FrameThreading::Get().BindCurrentThread(FrameThreadKind::RHI);
            MakeGraphicsContextCurrentIfNeeded();

            while (!bFrameRolesStop)
            {
                RHIFrameTask task{};
                {
                    std::unique_lock<std::mutex> lock(RHIMutex);
                    RHICv.wait(lock, [&]() {
                        return bFrameRolesStop || !RHITasks.empty();
                    });
                    if (bFrameRolesStop && RHITasks.empty())
                    {
                        break;
                    }
                    if (RHITasks.empty())
                    {
                        continue;
                    }
                    task = RHITasks.front();
                    RHITasks.pop_front();
                }
                RunRHIFrameTask(task);
            }

            ClearGraphicsContextIfNeeded();
        }

        void StopFrameRolesInternal()
        {
            if (!bFrameRolesRunning)
            {
                return;
            }

            bFrameRolesStop = true;
            RenderCv.notify_all();
            RHICv.notify_all();
            FrameCompleteCv.notify_all();

            if (GraphicsThread.joinable())
            {
                GraphicsThread.join();
            }
            if (RenderThread.joinable())
            {
                RenderThread.join();
            }
            if (RHIThread.joinable())
            {
                RHIThread.join();
            }

            RenderTaskFn = {};
            RHITaskFn = {};
            RenderTasks.clear();
            RHITasks.clear();
            bFrameRolesRunning = false;
            bCombineGraphicsThreads = false;

            if (FrameThreading::HasInstance())
            {
                FrameThreading::Get().BindCurrentThreadAsAllRoles();
            }
            if (WindowSystem::HasInstance())
            {
                WindowSystem::Get().MakeGraphicsContextCurrent();
            }

            ME_LOG(LogCore, Info, "TaskSystem: frame roles stopped.");
        }

        void StartFrameRolesInternal(
            RenderFrameTaskFn renderTask,
            RHIFrameTaskFn rhiTask,
            bool combineGraphicsThreads)
        {
            ME_ASSERT(StateValue == State::Running, "StartFrameRoles requires Init");
            ME_ASSERT(static_cast<bool>(renderTask), "Render frame task required");
            ME_ASSERT(static_cast<bool>(rhiTask), "RHI frame task required");
            ME_ASSERT(FrameThreading::HasInstance() && FrameThreading::Get().IsMulti(),
                "StartFrameRoles requires Multi mode");
            ME_ASSERT(!bFrameRolesRunning, "Frame roles already running");

            RenderTaskFn = std::move(renderTask);
            RHITaskFn = std::move(rhiTask);
            bCombineGraphicsThreads = combineGraphicsThreads;
            bFrameRolesStop = false;
            NextFrameSequence = 1;
            CompletedFrameSequence = 0;
            bFrameRolesRunning = true;

            FrameThreading::Get().BindCurrentThread(FrameThreadKind::Game);
            if (WindowSystem::HasInstance())
            {
                WindowSystem::Get().ClearGraphicsContextCurrent();
            }

            if (bCombineGraphicsThreads)
            {
                GraphicsThread = std::thread([this]() { CombinedGraphicsThreadMain(); });
                ME_LOG(LogCore, Info, "TaskSystem: combined graphics frame role started (OpenGL-safe).");
            }
            else
            {
                RHIThread = std::thread([this]() { RHIThreadMain(); });
                RenderThread = std::thread([this]() { RenderThreadMain(); });
                ME_LOG(LogCore, Info, "TaskSystem: Render and RHI frame roles started.");
            }
            FrameThreading::Get().LogStartupIdentity();
        }
    };

    TaskSystem* TaskSystem::s_Instance = nullptr;

    void TaskSystem::SetInstance(TaskSystem* instance)
    {
        s_Instance = instance;
    }

    TaskSystem& TaskSystem::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "TaskSystem is not initialized");
        return *s_Instance;
    }

    bool TaskSystem::HasInstance()
    {
        return s_Instance != nullptr;
    }

    TaskSystem::TaskSystem()
        : m_Impl(std::make_unique<Impl>())
    {
    }

    TaskSystem::~TaskSystem()
    {
        if (m_Impl && m_Impl->StateValue != Impl::State::Stopped)
        {
            Shutdown();
        }
        if (s_Instance == this)
        {
            s_Instance = nullptr;
        }
    }

    void TaskSystem::Init(int workerCount)
    {
        Impl& impl = *m_Impl;
        if (impl.StateValue == Impl::State::Running)
        {
            ME_LOG(LogCore, Warn, "TaskSystem::Init ignored: already Running.");
            return;
        }
        if (impl.StateValue == Impl::State::ShuttingDown)
        {
            ME_LOG(LogCore, Error, "TaskSystem::Init rejected: ShuttingDown.");
            return;
        }

        impl.WorkerCount = ResolveWorkerCount(workerCount);
        impl.GameThreadId = std::this_thread::get_id();
        impl.Executor = std::make_unique<tf::Executor>(static_cast<unsigned>(impl.WorkerCount));
        impl.StateValue = Impl::State::Running;
        ME_LOG(LogCore, Info, "TaskSystem initialized (workers={}).", impl.WorkerCount);
    }

    void TaskSystem::Shutdown()
    {
        Impl& impl = *m_Impl;
        if (impl.StateValue == Impl::State::Stopped)
        {
            return;
        }
        if (impl.StateValue == Impl::State::ShuttingDown)
        {
            return;
        }
        if (!impl.IsGameThread())
        {
            ME_LOG(LogCore, Error, "TaskSystem::Shutdown rejected: not on the game thread.");
            return;
        }

        impl.StateValue = Impl::State::ShuttingDown;
        impl.StopFrameRolesInternal();

        if (impl.Executor)
        {
            impl.Executor->wait_for_all();
        }

        impl.DrainCompletionsInternal();

        {
            std::lock_guard<std::mutex> lock(impl.TasksMutex);
            impl.Tasks.clear();
        }
        {
            std::lock_guard<std::mutex> lock(impl.CompletionMutex);
            impl.Completions.clear();
        }

        impl.Executor.reset();
        impl.WorkerCount = 0;
        impl.GameThreadId = {};
        impl.StateValue = Impl::State::Stopped;
        ME_LOG(LogCore, Info, "TaskSystem shut down.");
    }

    TaskHandle TaskSystem::Submit(WorkFn work)
    {
        return Submit(std::move(work), {});
    }

    TaskHandle TaskSystem::Submit(WorkFn work, CompletionFn onComplete)
    {
        return m_Impl->SubmitInternal(std::move(work), std::move(onComplete));
    }

    void TaskSystem::Wait(TaskHandle handle)
    {
        if (!handle.IsValid())
        {
            return;
        }

        Impl& impl = *m_Impl;
        std::shared_ptr<Impl::TaskRecord> record;
        {
            std::lock_guard<std::mutex> lock(impl.TasksMutex);
            const auto it = impl.Tasks.find(handle.Id);
            if (it == impl.Tasks.end())
            {
                return;
            }
            record = it->second;
        }

        std::unique_lock<std::mutex> lock(record->Mutex);
        record->Cv.wait(lock, [&record]() { return record->bWorkFinished; });
    }

    void TaskSystem::DrainCompletions()
    {
        Impl& impl = *m_Impl;
        if (impl.StateValue == Impl::State::Stopped)
        {
            return;
        }
        if (!impl.IsGameThread())
        {
            ME_LOG(LogCore, Error, "TaskSystem::DrainCompletions rejected: not on the game thread.");
            return;
        }
        impl.DrainCompletionsInternal();
    }

    void TaskSystem::ParallelFor(int32_t count, ParallelBodyFn body)
    {
        Impl& impl = *m_Impl;
        if (impl.StateValue != Impl::State::Running || !impl.Executor)
        {
            ME_LOG(LogCore, Error, "TaskSystem::ParallelFor rejected: not running.");
            return;
        }
        if (!impl.IsAllowedOrchestrator())
        {
            ME_LOG(LogCore, Error, "TaskSystem::ParallelFor rejected: not Game or Render thread.");
            return;
        }
        if (count <= 0 || !body)
        {
            return;
        }

        if (count == 1)
        {
            try
            {
                body(0);
            }
            catch (const std::exception& ex)
            {
                ME_LOG(LogCore, Error, "TaskSystem::ParallelFor body threw: {}", ex.what());
            }
            catch (...)
            {
                ME_LOG(LogCore, Error, "TaskSystem::ParallelFor body threw an unknown exception.");
            }
            return;
        }

        // Fan-out via silent_async + join (avoids MinGW local-type issues with for_each_index).
        auto remaining = std::make_shared<std::atomic<int>>(count);
        std::mutex joinMutex;
        std::condition_variable joinCv;
        auto bodyPtr = std::make_shared<ParallelBodyFn>(std::move(body));

        for (int32_t index = 0; index < count; ++index)
        {
            impl.Executor->silent_async([bodyPtr, remaining, &joinMutex, &joinCv, index]() {
                try
                {
                    (*bodyPtr)(index);
                }
                catch (const std::exception& ex)
                {
                    ME_LOG(LogCore, Error, "TaskSystem::ParallelFor body threw: {}", ex.what());
                }
                catch (...)
                {
                    ME_LOG(LogCore, Error, "TaskSystem::ParallelFor body threw an unknown exception.");
                }

                if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    std::lock_guard<std::mutex> lock(joinMutex);
                    joinCv.notify_one();
                }
            });
        }

        std::unique_lock<std::mutex> lock(joinMutex);
        joinCv.wait(lock, [&]() { return remaining->load(std::memory_order_acquire) == 0; });
    }

    void TaskSystem::RunAndWait(WorkFn work)
    {
        if (!work)
        {
            return;
        }
        ParallelFor(1, [work = std::move(work)](int32_t) { work(); });
    }

    TaskHandle TaskSystem::SubmitFlow(TaskFlowBuilder&& builder)
    {
        return SubmitFlow(std::move(builder), {});
    }

    TaskHandle TaskSystem::SubmitFlow(TaskFlowBuilder&& builder, CompletionFn onComplete)
    {
        return m_Impl->SubmitFlowInternal(std::move(builder), std::move(onComplete));
    }

    bool TaskSystem::IsRunning() const
    {
        return m_Impl->StateValue == Impl::State::Running;
    }

    int TaskSystem::GetWorkerCount() const
    {
        return m_Impl->WorkerCount;
    }

    std::thread::id TaskSystem::GetGameThreadId() const
    {
        return m_Impl->GameThreadId;
    }

    bool TaskSystem::AreFrameRolesRunning() const
    {
        return m_Impl->bFrameRolesRunning;
    }

    void TaskSystem::StartFrameRoles(
        RenderFrameTaskFn renderTask,
        RHIFrameTaskFn rhiTask,
        bool combineGraphicsThreads)
    {
        m_Impl->StartFrameRolesInternal(
            std::move(renderTask),
            std::move(rhiTask),
            combineGraphicsThreads);
    }

    void TaskSystem::StopFrameRoles()
    {
        m_Impl->StopFrameRolesInternal();
    }

    void TaskSystem::WaitForRenderQueueSpace()
    {
        Impl& impl = *m_Impl;
        if (!impl.bFrameRolesRunning)
        {
            return;
        }
        std::unique_lock<std::mutex> lock(impl.RenderMutex);
        impl.WaitForRenderQueueSpaceLocked(lock);
    }

    void TaskSystem::SubmitRenderFrameTask(float deltaTime, bool waitForCompletion)
    {
        Impl& impl = *m_Impl;
        ME_ASSERT(impl.bFrameRolesRunning, "SubmitRenderFrameTask before StartFrameRoles");

        ME_PROFILE_SCOPE("Frame.Game.SubmitRenderFrame");

        uint64_t sequence = 0;
        {
            std::unique_lock<std::mutex> lock(impl.RenderMutex);
            impl.WaitForRenderQueueSpaceLocked(lock);
            if (impl.bFrameRolesStop)
            {
                return;
            }
            sequence = impl.NextFrameSequence++;
            impl.RenderTasks.push_back(Impl::RenderFrameTask{deltaTime, waitForCompletion, sequence});
        }
        impl.RenderCv.notify_one();

        if (!waitForCompletion)
        {
            return;
        }

        ME_PROFILE_SCOPE("Frame.Game.WaitFrameComplete");
        std::unique_lock<std::mutex> lock(impl.FrameCompleteMutex);
        impl.FrameCompleteCv.wait(lock, [&]() {
            return impl.bFrameRolesStop || impl.CompletedFrameSequence >= sequence;
        });
    }
}

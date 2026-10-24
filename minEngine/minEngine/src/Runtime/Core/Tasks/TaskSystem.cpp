#include "TaskSystem.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Log/LogSystem.h"

#include <taskflow/taskflow.hpp>

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
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

        State StateValue = State::Stopped;
        int WorkerCount = 0;
        std::thread::id GameThreadId{};
        std::unique_ptr<tf::Executor> Executor;
        std::atomic<uint64_t> NextTaskId{1};

        std::mutex TasksMutex;
        std::unordered_map<uint64_t, std::shared_ptr<TaskRecord>> Tasks;

        std::mutex CompletionMutex;
        std::vector<CompletionEvent> Completions;

        bool IsGameThread() const
        {
            return std::this_thread::get_id() == GameThreadId;
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

                {
                    std::lock_guard<std::mutex> lock(record->Mutex);
                    record->bSucceeded = bSucceeded;
                    record->bWorkFinished = true;
                }
                record->Cv.notify_all();
                PublishCompletion(record);
            });

            return handle;
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
}

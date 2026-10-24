#include "TaskSystemTest.h"

#include "Runtime/Core/Tasks/TaskSystem.h"

#include "doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using minEngine::TaskHandle;
using minEngine::TaskSystem;

namespace
{
    bool SkipPerfExperiment()
    {
        const char* value = std::getenv("MINENGINE_SKIP_TASK_PERF");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }

    uint64_t BusyHash(uint64_t seed, int iterations)
    {
        uint64_t x = seed;
        for (int i = 0; i < iterations; ++i)
        {
            x ^= x << 13;
            x ^= x >> 7;
            x ^= x << 17;
            x *= 0x9E3779B97F4A7C15ull;
        }
        return x;
    }
}

TEST_CASE("task-system: init shutdown repeat [smoke]")
{
    TaskSystem tasks;
    CHECK_FALSE(tasks.IsRunning());
    tasks.Init(2);
    CHECK(tasks.IsRunning());
    CHECK(tasks.GetWorkerCount() == 2);
    {
        const bool onGameThread = tasks.GetGameThreadId() == std::this_thread::get_id();
        CHECK(onGameThread);
    }
    tasks.Shutdown();
    CHECK_FALSE(tasks.IsRunning());
    tasks.Shutdown();
    CHECK_FALSE(tasks.IsRunning());
}

TEST_CASE("task-system: wait then drain completion [smoke]")
{
    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> workValue{0};
    std::atomic<bool> callbackRan{false};
    std::thread::id callbackThread{};

    const TaskHandle handle = tasks.Submit(
        [&workValue]() {
            workValue.store(42, std::memory_order_relaxed);
        },
        [&callbackRan, &callbackThread](bool bSucceeded) {
            CHECK(bSucceeded);
            callbackThread = std::this_thread::get_id();
            callbackRan.store(true, std::memory_order_relaxed);
        });

    CHECK(handle.IsValid());
    tasks.Wait(handle);
    CHECK(workValue.load(std::memory_order_relaxed) == 42);
    CHECK_FALSE(callbackRan.load(std::memory_order_relaxed));

    tasks.DrainCompletions();
    CHECK(callbackRan.load(std::memory_order_relaxed));
    {
        const bool callbackOnGameThread = callbackThread == tasks.GetGameThreadId();
        CHECK(callbackOnGameThread);
    }

    tasks.Wait(handle);
    tasks.Shutdown();
}

TEST_CASE("task-system: game-module style submit apply on drain [smoke]")
{
    // Recommended product pattern: Get() after SetInstance; worker writes local result only;
    // game state is updated in onComplete after Engine-style Drain (here explicit for the test).
    TaskSystem tasks;
    TaskSystem::SetInstance(&tasks);
    tasks.Init(2);

    std::atomic<uint64_t> workerHash{0};
    uint64_t appliedOnGameThread = 0;

    const TaskHandle handle = TaskSystem::Get().Submit(
        [&workerHash]() {
            uint64_t x = 0xC0FFEEULL;
            for (int i = 0; i < 1000; ++i)
            {
                x ^= x << 13;
                x ^= x >> 7;
                x ^= x << 17;
            }
            workerHash.store(x, std::memory_order_relaxed);
        },
        [&workerHash, &appliedOnGameThread](bool bSucceeded) {
            CHECK(bSucceeded);
            appliedOnGameThread = workerHash.load(std::memory_order_relaxed);
        });

    CHECK(handle.IsValid());
    TaskSystem::Get().Wait(handle);
    CHECK(appliedOnGameThread == 0);

    TaskSystem::Get().DrainCompletions();
    CHECK(appliedOnGameThread != 0);
    CHECK(appliedOnGameThread == workerHash.load(std::memory_order_relaxed));

    TaskSystem::SetInstance(nullptr);
    tasks.Shutdown();
}

TEST_CASE("task-system: async completion without guessing sleep [smoke]")
{
    TaskSystem tasks;
    tasks.Init(2);

    std::mutex mutex;
    std::condition_variable cv;
    bool bReady = false;
    std::thread::id callbackThread{};

    const TaskHandle handle = tasks.Submit(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        },
        [&](bool bSucceeded) {
            CHECK(bSucceeded);
            callbackThread = std::this_thread::get_id();
            {
                std::lock_guard<std::mutex> lock(mutex);
                bReady = true;
            }
            cv.notify_one();
        });

    CHECK(handle.IsValid());

    for (int i = 0; i < 200; ++i)
    {
        tasks.DrainCompletions();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (bReady)
            {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(cv.wait_for(lock, std::chrono::seconds(2), [&]() { return bReady; }));
    }
    {
        const bool callbackOnGameThread = callbackThread == tasks.GetGameThreadId();
        CHECK(callbackOnGameThread);
    }
    tasks.Shutdown();
}

TEST_CASE("task-system: work exception becomes failed [smoke]")
{
    TaskSystem tasks;
    tasks.Init(1);

    std::atomic<bool> sawFailure{false};
    const TaskHandle handle = tasks.Submit(
        []() {
            throw std::runtime_error("synthetic work failure");
        },
        [&sawFailure](bool bSucceeded) {
            CHECK_FALSE(bSucceeded);
            sawFailure.store(true, std::memory_order_relaxed);
        });

    CHECK(handle.IsValid());
    tasks.Wait(handle);
    tasks.DrainCompletions();
    CHECK(sawFailure.load(std::memory_order_relaxed));
    tasks.Shutdown();
}

TEST_CASE("task-system: callback exception isolates batch [smoke]")
{
    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> secondCallbackCount{0};

    const TaskHandle first = tasks.Submit(
        []() {},
        [](bool) {
            throw std::runtime_error("synthetic callback failure");
        });
    const TaskHandle second = tasks.Submit(
        []() {},
        [&secondCallbackCount](bool bSucceeded) {
            CHECK(bSucceeded);
            secondCallbackCount.fetch_add(1, std::memory_order_relaxed);
        });

    tasks.Wait(first);
    tasks.Wait(second);
    tasks.DrainCompletions();
    CHECK(secondCallbackCount.load(std::memory_order_relaxed) == 1);
    tasks.Shutdown();
}

TEST_CASE("task-system: shutdown drains and rejects new submit [smoke]")
{
    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<bool> callbackRan{false};
    const TaskHandle handle = tasks.Submit(
        []() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        },
        [&callbackRan](bool bSucceeded) {
            CHECK(bSucceeded);
            callbackRan.store(true, std::memory_order_relaxed);
        });

    CHECK(handle.IsValid());
    tasks.Shutdown();
    CHECK(callbackRan.load(std::memory_order_relaxed));
    CHECK_FALSE(tasks.IsRunning());

    const TaskHandle rejected = tasks.Submit([]() {});
    CHECK_FALSE(rejected.IsValid());
}

TEST_CASE("task-system: non-game-thread submit rejected [smoke]")
{
    TaskSystem tasks;
    tasks.Init(1);

    TaskHandle remoteHandle{};
    std::thread worker([&]() {
        remoteHandle = tasks.Submit([]() {});
    });
    worker.join();
    CHECK_FALSE(remoteHandle.IsValid());
    tasks.Shutdown();
}

TEST_CASE("task-system: worker one vs n wall clock [full]")
{
    if (SkipPerfExperiment())
    {
        MESSAGE("skipped via MINENGINE_SKIP_TASK_PERF");
        return;
    }

    constexpr int kTaskCount = 8;
    constexpr int kIterations = 250000;
    const int workerN = static_cast<int>((std::max)(2u, std::thread::hardware_concurrency()));

    auto runBatch = [&](int workers) -> std::pair<std::vector<uint64_t>, double> {
        TaskSystem tasks;
        tasks.Init(workers);

        std::vector<uint64_t> results(static_cast<size_t>(kTaskCount), 0);
        std::vector<TaskHandle> handles;
        handles.reserve(static_cast<size_t>(kTaskCount));

        const auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < kTaskCount; ++i)
        {
            handles.push_back(tasks.Submit([i, &results]() {
                results[static_cast<size_t>(i)] = BusyHash(static_cast<uint64_t>(i + 1), kIterations);
            }));
        }
        for (const TaskHandle& handle : handles)
        {
            tasks.Wait(handle);
        }
        const auto end = std::chrono::steady_clock::now();
        tasks.DrainCompletions();
        tasks.Shutdown();

        const double seconds = std::chrono::duration<double>(end - start).count();
        return {std::move(results), seconds};
    };

    const auto one = runBatch(1);
    const auto many = runBatch(workerN);

    CHECK(one.first.size() == many.first.size());
    for (size_t i = 0; i < one.first.size(); ++i)
    {
        CHECK(one.first[i] == many.first[i]);
    }

    MESSAGE(
        "task-system perf: workers1=",
        1,
        " time=",
        one.second,
        "s; workersN=",
        workerN,
        " time=",
        many.second,
        "s; speedup=",
        (many.second > 0.0 ? one.second / many.second : 0.0));
}

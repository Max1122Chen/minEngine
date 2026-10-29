#include "TaskSystemTest.h"

#include "Runtime/Core/Tasks/TaskFlowBuilder.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

#include "doctest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using minEngine::FrameThreadKind;
using minEngine::FrameThreading;
using minEngine::FrameThreadMode;
using minEngine::TaskFlowBuilder;
using minEngine::TaskHandle;
using minEngine::TaskSystem;

namespace
{
    bool SkipPerfExperiment()
    {
        const char* value = std::getenv("MINENGINE_SKIP_TASK_PERF");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }

    /** Bounded wait: a broken contract must fail the case, not wedge the whole test binary. */
    template <typename TPredicate>
    bool WaitUntil(TPredicate predicate, std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return predicate();
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

TEST_CASE("task-system: parallel for join on game [smoke]")
{
    TaskSystem tasks;
    tasks.Init(4);

    constexpr int32_t kCount = 64;
    std::vector<int> values(static_cast<size_t>(kCount), 0);
    tasks.ParallelFor(kCount, [&](int32_t i) {
        values[static_cast<size_t>(i)] = i + 1;
    });

    for (int32_t i = 0; i < kCount; ++i)
    {
        CHECK(values[static_cast<size_t>(i)] == i + 1);
    }
    tasks.Shutdown();
}

TEST_CASE("task-system: parallel for on render thread [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> sum{0};
    std::thread render([&]() {
        frameThreads.BindCurrentThread(FrameThreadKind::Render);
        tasks.ParallelFor(32, [&](int32_t i) {
            sum.fetch_add(i, std::memory_order_relaxed);
        });
    });
    render.join();

    CHECK(sum.load(std::memory_order_relaxed) == (31 * 32) / 2);
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: parallel for rejects foreign thread [smoke]")
{
    TaskSystem tasks;
    tasks.Init(1);

    std::atomic<bool> ran{false};
    std::thread foreign([&]() {
        tasks.ParallelFor(4, [&](int32_t) {
            ran.store(true, std::memory_order_relaxed);
        });
    });
    foreign.join();
    CHECK_FALSE(ran.load(std::memory_order_relaxed));
    tasks.Shutdown();
}

TEST_CASE("task-system: run and wait [smoke]")
{
    TaskSystem tasks;
    tasks.Init(2);
    std::atomic<int> value{0};
    tasks.RunAndWait([&]() { value.store(7, std::memory_order_relaxed); });
    CHECK(value.load(std::memory_order_relaxed) == 7);
    tasks.Shutdown();
}

TEST_CASE("task-system: submit flow diamond [smoke]")
{
    TaskSystem tasks;
    tasks.Init(4);

    std::atomic<int> stage{0};
    std::vector<int> order;
    std::mutex orderMutex;

    TaskFlowBuilder flow;
    const uint32_t load = flow.AddNode([&]() {
        stage.store(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(0);
    });
    const uint32_t left = flow.AddNode([&]() {
        CHECK(stage.load(std::memory_order_relaxed) >= 1);
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(1);
    });
    const uint32_t right = flow.AddNode([&]() {
        CHECK(stage.load(std::memory_order_relaxed) >= 1);
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(2);
    });
    const uint32_t merge = flow.AddNode([&]() {
        std::lock_guard<std::mutex> lock(orderMutex);
        order.push_back(3);
    });
    flow.Precede(load, {left, right});
    flow.Precede(left, merge);
    flow.Precede(right, merge);

    const TaskHandle handle = tasks.SubmitFlow(std::move(flow));
    CHECK(handle.IsValid());
    tasks.Wait(handle);
    CHECK(order.size() == 4);
    CHECK(order.front() == 0);
    CHECK(order.back() == 3);
    tasks.Shutdown();
}

TEST_CASE("task-system: submit flow cycle rejected [smoke]")
{
    TaskSystem tasks;
    tasks.Init(1);

    TaskFlowBuilder flow;
    const uint32_t a = flow.AddNode([]() {});
    const uint32_t b = flow.AddNode([]() {});
    flow.Precede(a, b);
    flow.Precede(b, a);

    const TaskHandle handle = tasks.SubmitFlow(std::move(flow));
    CHECK_FALSE(handle.IsValid());
    tasks.Shutdown();
}

TEST_CASE("task-system: frame roles combined wait [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> order{0};
    std::atomic<int> renderStep{0};
    std::atomic<int> rhiStep{0};

    tasks.StartFrameRoles(
        [&](float) {
            renderStep.store(order.fetch_add(1, std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        },
        [&]() {
            rhiStep.store(order.fetch_add(1, std::memory_order_relaxed) + 1, std::memory_order_relaxed);
        },
        /*combineGraphicsThreads=*/true);

    CHECK(tasks.AreFrameRolesRunning());
    tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/true);
    CHECK(renderStep.load(std::memory_order_relaxed) == 1);
    CHECK(rhiStep.load(std::memory_order_relaxed) == 2);

    tasks.StopFrameRoles();
    CHECK_FALSE(tasks.AreFrameRolesRunning());
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: frame roles split wait [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> renderCount{0};
    std::atomic<int> rhiCount{0};

    tasks.StartFrameRoles(
        [&](float) { renderCount.fetch_add(1, std::memory_order_relaxed); },
        [&]() { rhiCount.fetch_add(1, std::memory_order_relaxed); },
        /*combineGraphicsThreads=*/false);

    tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/true);
    CHECK(renderCount.load(std::memory_order_relaxed) == 1);
    CHECK(rhiCount.load(std::memory_order_relaxed) == 1);

    tasks.StopFrameRoles();
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: frame roles async multi-frame [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    constexpr int kFrames = 8;
    std::atomic<int> renderCount{0};
    std::atomic<int> rhiCount{0};

    tasks.StartFrameRoles(
        [&](float) { renderCount.fetch_add(1, std::memory_order_relaxed); },
        [&]() { rhiCount.fetch_add(1, std::memory_order_relaxed); },
        /*combineGraphicsThreads=*/true);

    for (int i = 0; i < kFrames; ++i)
    {
        tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/false);
    }
    // Drain pending work by waiting on a sentinel frame.
    tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/true);

    CHECK(renderCount.load(std::memory_order_relaxed) == kFrames + 1);
    CHECK(rhiCount.load(std::memory_order_relaxed) == kFrames + 1);

    tasks.StopFrameRoles();
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: frame roles backpressure [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::mutex permitMutex;
    std::condition_variable permitCv;
    std::atomic<int> permits{0};
    std::atomic<int> rendered{0};

    tasks.StartFrameRoles(
        [&](float) {
            std::unique_lock<std::mutex> lock(permitMutex);
            permitCv.wait(lock, [&]() { return permits.load(std::memory_order_relaxed) > 0; });
            permits.fetch_sub(1, std::memory_order_relaxed);
            rendered.fetch_add(1, std::memory_order_relaxed);
        },
        [&]() {},
        /*combineGraphicsThreads=*/true);

    // Fill every end-to-end slot: the frame executing (blocked on a permit) and the ones queued
    // behind it are all in flight, so the bound is reached and the next submit has to wait.
    constexpr int kFillCount = static_cast<int>(TaskSystem::kMaxInFlightFrames);
    for (int i = 0; i < kFillCount; ++i)
    {
        tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/false);
    }

    std::atomic<bool> extraDone{false};
    std::thread extraSubmit([&]() {
        tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/false);
        extraDone.store(true, std::memory_order_release);
    });

    // Extra submit should block while the queue is full and render is gated.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    CHECK_FALSE(extraDone.load(std::memory_order_acquire));

    {
        std::lock_guard<std::mutex> lock(permitMutex);
        permits.store(kFillCount + 1, std::memory_order_relaxed);
    }
    permitCv.notify_all();

    extraSubmit.join();
    CHECK(extraDone.load(std::memory_order_acquire));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (rendered.load(std::memory_order_relaxed) < kFillCount + 1
        && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(rendered.load(std::memory_order_relaxed) == kFillCount + 1);

    tasks.StopFrameRoles();
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: in-flight frames stay bounded when the RHI side is slow [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    constexpr auto kRHIDuration = std::chrono::milliseconds(8);
    constexpr int kFrames = 12;

    std::atomic<int> submitted{0};
    std::atomic<int> rhiSettled{0};
    std::atomic<int> maxSkew{0};

    // How far the game role is ahead of the GPU: frames handed over whose RHI work has not settled.
    const auto observeSkew = [&]() {
        const int skew =
            submitted.load(std::memory_order_acquire) - rhiSettled.load(std::memory_order_acquire);
        int seen = maxSkew.load(std::memory_order_relaxed);
        while (skew > seen && !maxSkew.compare_exchange_weak(seen, skew, std::memory_order_relaxed))
        {
        }
    };

    tasks.StartFrameRoles(
        [&](float) { observeSkew(); },
        [&]() {
            // A slow GPU: the frame is over only once this finished.
            std::this_thread::sleep_for(kRHIDuration);
            rhiSettled.fetch_add(1, std::memory_order_acq_rel);
            observeSkew();
        },
        /*combineGraphicsThreads=*/false);

    const auto start = std::chrono::steady_clock::now();
    for (int index = 0; index < kFrames; ++index)
    {
        tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/false);

        // Submitting returns only once this frame has an end-to-end slot, so the frames handed over
        // and not yet replayed never exceed the bound. A queue-only bound would let the loop run
        // every frame while the RHI side was still replaying the first ones. Counted after the
        // submit, because a frame waiting for a slot has not been handed over yet.
        const int handedOver = submitted.fetch_add(1, std::memory_order_acq_rel) + 1;
        const int settled = rhiSettled.load(std::memory_order_acquire);
        CHECK(settled >= handedOver - static_cast<int>(TaskSystem::kMaxInFlightFrames));
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    CHECK(maxSkew.load(std::memory_order_relaxed) <= static_cast<int>(TaskSystem::kMaxInFlightFrames));
    // The game role was held back instead of running ahead: everything past the first bound-many
    // frames had to wait for RHI work.
    CHECK(elapsed >= (kRHIDuration * (kFrames - static_cast<int>(TaskSystem::kMaxInFlightFrames))) / 2);

    tasks.StopFrameRoles();
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: frame work loss is reported as one summary line [smoke]")
{
    using minEngine::DescribeFrameWorkLossDelta;
    using minEngine::FrameWorkLossCounters;

    const FrameWorkLossCounters quiet;
    CHECK(DescribeFrameWorkLossDelta(quiet, quiet).empty());

    FrameWorkLossCounters lossy;
    lossy.DroppedCommands = 5;
    lossy.DroppedViewIntents = 1;
    lossy.ApplyFailures = 2;
    lossy.FailedFrames = 1;
    lossy.DroppedFrames = 3;

    const std::string line = DescribeFrameWorkLossDelta(quiet, lossy);
    REQUIRE_FALSE(line.empty());
    // One line for the frame as a whole, carrying each count instead of one line per event.
    CHECK(line.find('\n') == std::string::npos);
    CHECK(line.find("5 render command(s) dropped") != std::string::npos);
    CHECK(line.find("1 view intent(s) dropped") != std::string::npos);
    CHECK(line.find("2 render command(s) failed to apply") != std::string::npos);
    CHECK(line.find("1 frame(s) failed") != std::string::npos);
    CHECK(line.find("3 queued frame(s) discarded") != std::string::npos);

    // The next frame reports only what it lost itself.
    FrameWorkLossCounters next = lossy;
    next.DroppedCommands = 6;
    const std::string nextLine = DescribeFrameWorkLossDelta(lossy, next);
    CHECK(nextLine.find("1 render command(s) dropped") != std::string::npos);
    CHECK(nextLine.find("view intent") == std::string::npos);

    // A counter that went backwards is a new frame-role instance, not recovered work.
    CHECK(DescribeFrameWorkLossDelta(lossy, FrameWorkLossCounters{}).empty());

    // The summary is fed by the live counters, not by a private copy: a frame that failed in the
    // roles shows up through the same snapshot the engine logs.
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);
    TaskSystem tasks;
    tasks.Init(2);
    tasks.StartFrameRoles(
        [](float) { throw std::runtime_error("synthetic render role failure"); },
        []() {},
        /*combineGraphicsThreads=*/true);
    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    FrameWorkLossCounters live;
    live.FailedFrames = tasks.GetFailedFrameCount();
    live.DroppedFrames = tasks.GetDroppedQueuedFrameCount();
    CHECK(live.FailedFrames == 1u);
    CHECK(DescribeFrameWorkLossDelta(quiet, live).find("1 frame(s) failed") != std::string::npos);

    tasks.StopFrameRoles();
    tasks.Shutdown();
    frameThreads.Shutdown();
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
TEST_CASE("task-system: role exception settles the frame incomplete and the thread lives [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> renderCalls{0};
    tasks.StartFrameRoles(
        [&renderCalls](float) {
            if (renderCalls.fetch_add(1, std::memory_order_relaxed) == 0)
            {
                throw std::runtime_error("synthetic render role failure");
            }
        },
        []() {},
        /*combineGraphicsThreads=*/true);

    // A throwing role body must not escape the thread entry point; the frame is settled as not
    // completed and the waiting producer is released instead of hanging.
    std::atomic<bool> producerReleased{false};
    std::thread producer([&tasks, &producerReleased]() {
        tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);
        producerReleased.store(true, std::memory_order_release);
    });

    const bool bReleased = WaitUntil(
        [&producerReleased]() { return producerReleased.load(std::memory_order_acquire); },
        std::chrono::milliseconds(5000));
    CHECK(bReleased);
    if (bReleased)
    {
        producer.join();
    }
    else
    {
        // Unblock the waiter so the test can still report and exit: stop signals every waiter.
        tasks.StopFrameRoles();
        producer.join();
    }
    CHECK(bReleased);
    CHECK(tasks.GetFailedFrameCount() == 1);

    // The role thread survived the throw and keeps executing later frames.
    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);
    CHECK(renderCalls.load(std::memory_order_relaxed) == 2);
    CHECK(tasks.GetFailedFrameCount() == 1);

    tasks.StopFrameRoles();
    CHECK_FALSE(tasks.AreFrameRolesRunning());
    tasks.Shutdown();
    frameThreads.Shutdown();
}

TEST_CASE("task-system: stopping roles drains queued frames without hanging [smoke]")
{
    FrameThreading frameThreads;
    frameThreads.Initialize(FrameThreadMode::Multi);

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> rendered{0};
    tasks.StartFrameRoles(
        [&rendered](float) { rendered.fetch_add(1, std::memory_order_relaxed); },
        []() {},
        /*combineGraphicsThreads=*/true);

    constexpr int kQueuedFrames = 4;
    for (int i = 0; i < kQueuedFrames; ++i)
    {
        tasks.SubmitRenderFrameTask(0.016f, /*waitForCompletion=*/false);
    }

    // Shutdown may not discard frames it already accepted, and it may not wait forever on them.
    std::atomic<bool> stopped{false};
    std::thread stopper([&tasks, &stopped]() {
        tasks.StopFrameRoles();
        stopped.store(true, std::memory_order_release);
    });

    const bool bStopped = WaitUntil(
        [&stopped]() { return stopped.load(std::memory_order_acquire); },
        std::chrono::milliseconds(8000));
    CHECK(bStopped);
    stopper.join();

    CHECK(rendered.load(std::memory_order_relaxed) == kQueuedFrames);
    CHECK(tasks.GetDroppedQueuedFrameCount() == 0);

    // Nothing may still be ticking the world once StopFrameRoles returned.
    const int framesAtStop = rendered.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(rendered.load(std::memory_order_relaxed) == framesAtStop);

    tasks.Shutdown();
    frameThreads.Shutdown();
}

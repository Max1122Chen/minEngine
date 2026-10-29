#include "FrameThreadingTest.h"

#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"

#include "doctest.h"

#include <atomic>
#include <thread>

using minEngine::FrameThreadKind;
using minEngine::RenderCommandPipe;
using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::TaskSystem;

namespace
{
    // FrameThreading is a process-wide singleton. If a case leaked an installed instance (for
    // example by failing between Initialize and Shutdown), the next suite would compare thread
    // identities against a destroyed object, so acquisition always owns the release.
    struct FrameThreadingScope
    {
        FrameThreading Threading;

        explicit FrameThreadingScope(FrameThreadMode mode)
        {
            Threading.Initialize(mode);
        }

        ~FrameThreadingScope()
        {
            Threading.Shutdown();
        }

        FrameThreadingScope(const FrameThreadingScope&) = delete;
        FrameThreadingScope& operator=(const FrameThreadingScope&) = delete;
    };

    struct RenderCommandPipeScope
    {
        RenderCommandPipe Pipe;

        RenderCommandPipeScope()
        {
            Pipe.Initialize();
        }

        ~RenderCommandPipeScope()
        {
            Pipe.Shutdown();
        }

        RenderCommandPipeScope(const RenderCommandPipeScope&) = delete;
        RenderCommandPipeScope& operator=(const RenderCommandPipeScope&) = delete;
    };
}

TEST_CASE("frame-threading: parse mode tokens [smoke]")
{
    const auto single = FrameThreading::TryParseMode("single");
    REQUIRE(single.has_value());
    CHECK(*single == FrameThreadMode::Single);

    const auto singleUpper = FrameThreading::TryParseMode("Single");
    REQUIRE(singleUpper.has_value());
    CHECK(*singleUpper == FrameThreadMode::Single);

    const auto multi = FrameThreading::TryParseMode("multi");
    REQUIRE(multi.has_value());
    CHECK(*multi == FrameThreadMode::Multi);

    const auto multiUpper = FrameThreading::TryParseMode("MULTI");
    REQUIRE(multiUpper.has_value());
    CHECK(*multiUpper == FrameThreadMode::Multi);

    CHECK_FALSE(FrameThreading::TryParseMode("").has_value());
    CHECK_FALSE(FrameThreading::TryParseMode("parallel").has_value());
}

TEST_CASE("frame-threading: resolve mode defaults [smoke]")
{
    CHECK(FrameThreading::ResolveMode(std::nullopt) == FrameThreadMode::Single);
    CHECK(FrameThreading::ResolveMode(FrameThreadMode::Multi) == FrameThreadMode::Multi);
    CHECK(FrameThreading::ResolveMode(FrameThreadMode::Single) == FrameThreadMode::Single);
}

TEST_CASE("frame-threading: init binds game thread [smoke]")
{
    CHECK_FALSE(FrameThreading::HasInstance());
    FrameThreadingScope scope(FrameThreadMode::Single);
    FrameThreading& threading = scope.Threading;

    CHECK(FrameThreading::HasInstance());
    CHECK(threading.GetMode() == FrameThreadMode::Single);
    // Single topology: the one OS thread owns every role, so all three identities are the caller.
    CHECK(threading.IsInGameThread());
    CHECK(threading.IsInRenderingThread());
    CHECK(threading.IsInRHIThread());

    threading.Shutdown();
    CHECK_FALSE(FrameThreading::HasInstance());
    threading.Shutdown();
    CHECK_FALSE(FrameThreading::HasInstance());
}

TEST_CASE("frame-threading: multi binds only the game role at init [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    CHECK(threading.GetMode() == FrameThreadMode::Multi);
    CHECK(threading.IsInGameThread());
    // The Render/RHI threads do not exist yet: TaskSystem::StartFrameRoles claims those ids on
    // the threads it starts, so recording the caller here would name a thread that never runs
    // frame work and would make every Render-side assert pass for the wrong reason.
    CHECK_FALSE(threading.IsInRenderingThread());
    CHECK_FALSE(threading.IsInRHIThread());
    CHECK_FALSE(threading.IsInFrameThread(FrameThreadKind::Render));
    CHECK_FALSE(threading.IsInFrameThread(FrameThreadKind::RHI));
}

TEST_CASE("frame-threading: started frame roles own the render identity [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    TaskSystem tasks;
    tasks.Init(2);

    const std::thread::id gameThreadId = std::this_thread::get_id();
    std::atomic<bool> renderIdentityHeld{false};
    std::atomic<bool> renderThreadDiffers{false};
    std::thread::id renderThreadId{};

    tasks.StartFrameRoles(
        [&](float) {
            renderThreadId = std::this_thread::get_id();
            renderThreadDiffers.store(renderThreadId != gameThreadId, std::memory_order_relaxed);
            // Reaching the next line proves the Render identity is claimed on this thread.
            threading.AssertIsFrameThread(FrameThreadKind::Render, "frame-threading test");
            renderIdentityHeld.store(true, std::memory_order_relaxed);
        },
        [&]() {},
        /*combineGraphicsThreads=*/true);

    // Waiting for completion hands the render-thread writes over to this thread.
    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(renderThreadDiffers.load(std::memory_order_relaxed));
    CHECK(renderIdentityHeld.load(std::memory_order_relaxed));
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == renderThreadId);
    CHECK_FALSE(threading.IsInRenderingThread());

    tasks.StopFrameRoles();
    // Stopping hands the identity back to the game thread, so no dead worker id survives into
    // later work that consults the frame identity.
    CHECK(threading.IsInGameThread());
    CHECK(threading.IsInRenderingThread());
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == std::this_thread::get_id());

    tasks.Shutdown();
}
TEST_CASE("frame-threading: game role publishes the frame and the render role only drains it [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    RenderCommandPipeScope pipeScope;
    RenderCommandPipe& pipe = pipeScope.Pipe;

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<int> applied{0};
    pipe.Enqueue([&applied]() { applied.fetch_add(1, std::memory_order_relaxed); }); // tagged 0
    pipe.SetPublishedWorldGen(0);
    pipe.PublishGameFrame(0, 1); // the game role (this thread) announces frame 0 as complete

    std::atomic<uint64_t> renderDrainTarget{0};
    std::atomic<bool> renderDrainOk{false};
    std::atomic<bool> renderSawCompletedFrame{false};
    tasks.StartFrameRoles(
        [&](float) {
            const uint64_t completedFrame = pipe.GetCompletedGameFrame();
            renderDrainTarget.store(completedFrame, std::memory_order_relaxed);
            renderSawCompletedFrame.store(true, std::memory_order_relaxed);
            renderDrainOk.store(
                pipe.FlushApplyForFrameChecked(completedFrame),
                std::memory_order_relaxed);
        },
        []() {},
        /*combineGraphicsThreads=*/true);

    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);

    CHECK(renderSawCompletedFrame.load(std::memory_order_relaxed));
    CHECK(renderDrainTarget.load(std::memory_order_relaxed) == 0);
    CHECK(renderDrainOk.load(std::memory_order_relaxed));
    CHECK(applied.load(std::memory_order_relaxed) == 1);

    // The render role recorded its own progress and left the game-side numbers untouched.
    CHECK(pipe.GetDrawnFrame() == 0);
    CHECK(pipe.GetCompletedGameFrame() == 0);
    CHECK(pipe.GetCurrentGameFrame() == 1);

    tasks.StopFrameRoles();
    tasks.Shutdown();
}

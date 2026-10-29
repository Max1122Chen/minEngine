#include "FrameThreadingTest.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Assert/AssertLog.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"

#include "doctest.h"

#include <atomic>
#include <cstdint>
#include <thread>

using minEngine::FrameThreadKind;
using minEngine::RenderCommandPipe;
using minEngine::ScopedFrameThreadKind;
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

TEST_CASE("frame-threading: a scoped role is returned to unowned when the scope ends [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    const std::thread::id callerId = std::this_thread::get_id();
    REQUIRE(threading.IsInGameThread());
    // Nothing holds the graphics roles yet: TaskSystem claims them on the threads it starts.
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == std::thread::id{});
    CHECK(threading.GetThreadId(FrameThreadKind::RHI) == std::thread::id{});

    {
        ScopedFrameThreadKind renderRole(FrameThreadKind::Render);
        // Inside the scope this thread is the Render role, and no other role moved.
        CHECK(threading.GetThreadId(FrameThreadKind::Render) == callerId);
        CHECK(threading.IsInRenderingThread());
        CHECK_FALSE(threading.IsInRHIThread());
        CHECK(threading.IsInGameThread());
    }

    // The scope is over: the role is unowned again, not left pointing at this thread. A role left
    // behind here would let this thread pass every Render-side contract for the rest of the run.
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == std::thread::id{});
    CHECK_FALSE(threading.IsInRenderingThread());
    CHECK(threading.IsInGameThread());
}

TEST_CASE("frame-threading: nested scopes release one level at a time [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    const std::thread::id callerId = std::this_thread::get_id();

    {
        ScopedFrameThreadKind renderRole(FrameThreadKind::Render);
        REQUIRE(threading.IsInRenderingThread());
        {
            ScopedFrameThreadKind rhiRole(FrameThreadKind::RHI);
            CHECK(threading.GetThreadId(FrameThreadKind::Render) == callerId);
            CHECK(threading.GetThreadId(FrameThreadKind::RHI) == callerId);
            CHECK(threading.IsInRenderingThread());
            CHECK(threading.IsInRHIThread());
        }
        // The inner scope released RHI and left Render alone.
        CHECK(threading.IsInRenderingThread());
        CHECK_FALSE(threading.IsInRHIThread());
        CHECK(threading.GetThreadId(FrameThreadKind::RHI) == std::thread::id{});

        {
            // Re-entrant claim of a role this thread already holds: entering and leaving it must
            // not drop the role the outer scope granted.
            ScopedFrameThreadKind renderAgain(FrameThreadKind::Render);
            CHECK(threading.IsInRenderingThread());
        }
        CHECK(threading.IsInRenderingThread());
        CHECK(threading.GetThreadId(FrameThreadKind::Render) == callerId);
    }

    CHECK_FALSE(threading.IsInRenderingThread());
    CHECK_FALSE(threading.IsInRHIThread());
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == std::thread::id{});
    CHECK(threading.GetThreadId(FrameThreadKind::RHI) == std::thread::id{});
    CHECK(threading.IsInGameThread());
}

TEST_CASE("frame-threading: concurrent claims and queries never read a half-written identity [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    constexpr int kClaimCount = 2000;
    std::atomic<std::thread::id> claimantId{};
    std::atomic<bool> claimantFinished{false};
    std::atomic<std::uint64_t> tornReads{0};
    std::atomic<std::uint64_t> heldReads{0};
    std::atomic<std::uint64_t> unheldReads{0};
    std::atomic<std::uint64_t> claimsWithoutRole{0};

    std::thread claimant([&]() {
        claimantId.store(std::this_thread::get_id(), std::memory_order_release);
        for (int claim = 0; claim < kClaimCount; ++claim)
        {
            ScopedFrameThreadKind renderRole(FrameThreadKind::Render);
            if (!threading.IsInFrameThread(FrameThreadKind::Render))
            {
                // The claim was granted and the table does not say so: an identity that is only
                // half published is the failure this case exists for.
                claimsWithoutRole.fetch_add(1, std::memory_order_relaxed);
            }
        }
        claimantFinished.store(true, std::memory_order_release);
    });

    const auto query = [&]() {
        while (!claimantFinished.load(std::memory_order_acquire))
        {
            const std::thread::id claimant = claimantId.load(std::memory_order_acquire);
            if (claimant == std::thread::id{})
            {
                std::this_thread::yield();
                continue;
            }

            const std::thread::id reported = threading.GetThreadId(FrameThreadKind::Render);
            if (reported == std::thread::id{})
            {
                unheldReads.fetch_add(1, std::memory_order_relaxed);
            }
            else if (reported == claimant)
            {
                heldReads.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                // Held by a thread that never claimed it: ownership and identity disagree.
                tornReads.fetch_add(1, std::memory_order_relaxed);
            }

            // A thread that never held this role must never be told it holds it.
            if (threading.IsInFrameThread(FrameThreadKind::Render))
            {
                tornReads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    };

    std::thread firstReader(query);
    std::thread secondReader(query);
    std::thread thirdReader(query);

    claimant.join();
    firstReader.join();
    secondReader.join();
    thirdReader.join();

    CHECK(claimsWithoutRole.load(std::memory_order_relaxed) == 0);
    CHECK(tornReads.load(std::memory_order_relaxed) == 0);
    // The readers saw both states, so the two checks above ran across live transitions.
    CHECK(heldReads.load(std::memory_order_relaxed) > 0);
    CHECK(unheldReads.load(std::memory_order_relaxed) > 0);
}

TEST_CASE("frame-threading: a temporary claim on a role another thread holds is refused and counted [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    std::atomic<std::thread::id> ownerId{};
    std::atomic<bool> ownerHoldsRole{false};
    std::atomic<bool> ownerMayLeave{false};
    std::thread owner([&]() {
        threading.BindCurrentThread(FrameThreadKind::Render);
        ownerId.store(std::this_thread::get_id(), std::memory_order_release);
        ownerHoldsRole.store(true, std::memory_order_release);
        while (!ownerMayLeave.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
    });
    while (!ownerHoldsRole.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();

    {
        ScopedFrameThreadKind claimant(FrameThreadKind::Render);
        // One role, one owner: the claim is refused, so the role stays where it was.
        CHECK_FALSE(threading.IsInRenderingThread());
        CHECK(threading.GetThreadId(FrameThreadKind::Render) == ownerId.load(std::memory_order_acquire));
    }

    const minEngine::ContractViolationCounters after = minEngine::GetContractViolationCounters();
    CHECK_MESSAGE(after.RecordedOnContinue == 1, "counted ", after.RecordedOnContinue, " violations, not 1");
    CHECK_MESSAGE(after.RecordedOnTrap == 0, "a refused claim was counted as a trap");
    CHECK(threading.GetThreadId(FrameThreadKind::Render) == ownerId.load(std::memory_order_acquire));
    CHECK_FALSE(threading.IsInRenderingThread());

    ownerMayLeave.store(true, std::memory_order_release);
    owner.join();

    // The counters and the staging area are process-wide: a record left staged here would be
    // published by whichever suite drains next, and counted against that suite's own violation.
    (void)minEngine::DrainContractViolations();
}

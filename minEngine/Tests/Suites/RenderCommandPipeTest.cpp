#include "RenderCommandPipeTest.h"

#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include "doctest.h"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using minEngine::RenderCommandPipe;
using minEngine::SceneDrawDesc;

namespace
{
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
    };
}

TEST_CASE("render-command-pipe: drain applies every frame up to the target in order [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    std::vector<int> applied;
    int nextId = 0;
    const auto enqueueNext = [&]() {
        const int id = nextId++;
        pipe.Enqueue([&applied, id]() { applied.push_back(id); });
    };

    // The game role runs ahead: it tags a command per simulated frame and announces each boundary.
    enqueueNext();                     // tag 0, no frame completed yet
    pipe.PublishGameFrame(0, 1);       // frame 0 complete
    enqueueNext();                     // tag 1
    pipe.PublishGameFrame(1, 2);
    enqueueNext();                     // tag 2
    pipe.PublishGameFrame(2, 3);
    enqueueNext();                     // tag 3

    // The render role lags: draining frame 1 applies frames 0 and 1 in order and leaves the rest.
    CHECK(pipe.FlushApplyForFrameChecked(1));
    REQUIRE(applied.size() == 2);
    CHECK(applied[0] == 0);
    CHECK(applied[1] == 1);
    CHECK(pipe.GetDrawnFrame() == 1);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 0);

    // Catching up applies the backlog in order: nothing is stranded and nothing is reordered.
    CHECK(pipe.FlushApplyForFrameChecked(3));
    REQUIRE(applied.size() == 4);
    CHECK(applied[2] == 2);
    CHECK(applied[3] == 3);
    CHECK(pipe.GetDrawnFrame() == 3);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 0);

    // Nothing is left behind for the next drain.
    CHECK(pipe.FlushApplyForFrameChecked(3));
    CHECK(applied.size() == 4);
}

TEST_CASE("render-command-pipe: the render role never advances the game frame [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    CHECK(pipe.GetCompletedGameFrame() == UINT64_MAX);
    CHECK(pipe.GetCurrentGameFrame() == 0);
    CHECK(pipe.GetDrawnFrame() == UINT64_MAX);

    int applied = 0;
    pipe.Enqueue([&applied]() { ++applied; });
    pipe.PublishGameFrame(0, 1);

    CHECK(pipe.FlushApplyForFrameChecked(0));
    CHECK(applied == 1);
    // Draining recorded render-side progress only; both game-side numbers are exactly what the
    // game role published, and the drain has no way to move them.
    CHECK(pipe.GetDrawnFrame() == 0);
    CHECK(pipe.GetCompletedGameFrame() == 0);
    CHECK(pipe.GetCurrentGameFrame() == 1);

    // A second drain of the same frame changes nothing on the game side either.
    CHECK(pipe.FlushApplyForFrameChecked(0));
    CHECK(pipe.GetCompletedGameFrame() == 0);
    CHECK(pipe.GetCurrentGameFrame() == 1);

    // Advancing is a game-side act, and it is the only way either number moves.
    pipe.PublishGameFrame(1, 2);
    CHECK(pipe.GetCompletedGameFrame() == 1);
    CHECK(pipe.GetCurrentGameFrame() == 2);
}

TEST_CASE("render-command-pipe: drawing a frame the game never published trips the check [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    // Nothing drawn yet: the check has nothing to compare and stays quiet.
    std::string diagnostic;
    CHECK(pipe.CheckPublishedGenerationForDrawnFrame(&diagnostic));
    CHECK(diagnostic.empty());

    // State construction: the game role published generation 3, while the render role drains
    // frame 5 - a world the game never committed.
    pipe.SetPublishedWorldGen(3);
    pipe.PublishGameFrame(4, 5);
    CHECK(pipe.FlushApplyForFrameChecked(5));
    CHECK(pipe.GetDrawnFrame() == 5);
    CHECK(pipe.GetPublishedWorldGen() == 3);

    CHECK_FALSE(pipe.CheckPublishedGenerationForDrawnFrame(&diagnostic));
    // Both sides and the queue depth are readable at the failure site.
    CHECK(diagnostic.find("5") != std::string::npos);
    CHECK(diagnostic.find("3") != std::string::npos);
    CHECK(diagnostic.find("command queue depth") != std::string::npos);

    // Publishing the generation that frame 5 belongs to clears the condition again.
    pipe.SetPublishedWorldGen(5);
    CHECK(pipe.CheckPublishedGenerationForDrawnFrame(&diagnostic));
    CHECK(diagnostic.empty());
}

TEST_CASE("render-command-pipe: commands tagged with an already drained frame are dropped and counted [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    int applied = 0;
    pipe.PublishGameFrame(0, 1);

    // The render role drained frame 1, i.e. beyond what the game has completed. A command tagged
    // with the current game frame can no longer be applied in order.
    CHECK(pipe.FlushApplyForFrameChecked(1));
    CHECK(pipe.GetDrawnFrame() == 1);
    pipe.Enqueue([&applied]() { ++applied; });

    CHECK(applied == 0);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 1);

    // It is gone, not deferred: a later drain does not run it.
    CHECK(pipe.FlushApplyForFrameChecked(2));
    CHECK(applied == 0);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 1);
}

TEST_CASE("render-command-pipe: view intents are taken up to the target frame [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    SceneDrawDesc a{};
    a.ViewId = 11;
    pipe.SubmitViewIntent(a); // tag 0

    pipe.PublishGameFrame(0, 1);
    SceneDrawDesc b{};
    b.ViewId = 22;
    pipe.SubmitViewIntent(b); // tag 1

    // Taking frame 1 hands over both intents in submission order; the renderer keeps the newest
    // per view, so a backlog of intents must not simply vanish.
    std::vector<SceneDrawDesc> taken;
    pipe.TakeViewIntents(1, taken);
    REQUIRE(taken.size() == 2);
    CHECK(taken[0].ViewId == 11);
    CHECK(taken[1].ViewId == 22);

    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 0);

    // Nothing left behind, and a not-yet-completed game frame yields nothing at all.
    pipe.TakeViewIntents(1, taken);
    CHECK(taken.empty());
    pipe.TakeViewIntents(UINT64_MAX, taken);
    CHECK(taken.empty());
}

TEST_CASE("render-command-pipe: a throwing command does not drop the rest of the flush [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    int ranAfterThrow = 0;
    pipe.Enqueue([]() { throw std::runtime_error("synthetic command failure"); });
    pipe.Enqueue([&ranAfterThrow]() { ++ranAfterThrow; });
    pipe.Enqueue([]() { throw std::runtime_error("synthetic command failure"); });

    // The flush reports the frame as not cleanly applied, and every command still got its turn:
    // a half-applied frame ("removal applied, addition not") is exactly what this isolates.
    CHECK_FALSE(pipe.FlushApplyForFrameChecked(0));
    CHECK(ranAfterThrow == 1);
    CHECK(pipe.GetCounters().FlushFailureCount == 2);

    // The queue drained anyway, so the next frame starts from an empty pipe.
    CHECK(pipe.FlushApplyForFrameChecked(0));
    CHECK(pipe.GetCounters().FlushFailureCount == 2);
}

TEST_CASE("render-command-pipe: full command queue drops after a bounded wait [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    const size_t maxCommands = RenderCommandPipe::kMaxPendingRenderCommands;
    int ran = 0;
    for (size_t i = 0; i < maxCommands; ++i)
    {
        pipe.Enqueue([&ran]() { ++ran; });
    }
    CHECK(pipe.GetCounters().DroppedFullCommands == 0);

    // The producer must never spin here: in a single topology the producer is also the only
    // consumer, so waiting for space that nobody can free is a whole-process deadlock.
    const auto start = std::chrono::steady_clock::now();
    pipe.Enqueue([&ran]() { ++ran; });
    const double elapsedMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

    CHECK(pipe.GetCounters().DroppedFullCommands == 1);
    CHECK(elapsedMs < 5000.0);

    // The dropped command is not executed later: it was given up, not deferred.
    CHECK(pipe.FlushApplyForFrameChecked(0));
    CHECK(ran == static_cast<int>(maxCommands));
}

TEST_CASE("render-command-pipe: full view intent queue drops the oldest intent [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    const size_t maxIntents = RenderCommandPipe::kMaxPendingViewIntents;
    for (int32_t viewId = 1; viewId <= static_cast<int32_t>(maxIntents); ++viewId)
    {
        SceneDrawDesc desc{};
        desc.ViewId = viewId;
        pipe.SubmitViewIntent(desc);
    }

    // The producer must not block for a slot; the intent the renderer would draw next is the
    // newest one, so the oldest is the one to lose.
    SceneDrawDesc newest{};
    newest.ViewId = 99;
    pipe.SubmitViewIntent(newest);
    CHECK(pipe.GetCounters().DroppedFullViewIntents == 1);

    std::vector<SceneDrawDesc> taken;
    pipe.TakeViewIntents(0, taken);
    REQUIRE(taken.size() == maxIntents);
    CHECK(taken.front().ViewId == 2);
    CHECK(taken.back().ViewId == 99);
}

TEST_CASE("render-command-pipe: counters stay consistent with a concurrent producer and drain [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;

    // The game role completes frame 0 and tags frame 1; the render role keeps draining the
    // completed frame while the producer is still enqueueing, so the counters and the queue are
    // read and written from both threads at once.
    pipe.PublishGameFrame(0, 1);

    constexpr int kCommandCount = 400;
    std::atomic<int> applied{0};
    std::atomic<int> produced{0};
    std::atomic<bool> producerDone{false};

    std::thread producer([&pipe, &applied, &produced, &producerDone]() {
        for (int i = 0; i < kCommandCount; ++i)
        {
            pipe.Enqueue([&applied]() { applied.fetch_add(1, std::memory_order_relaxed); });
            produced.fetch_add(1, std::memory_order_relaxed);
        }
        producerDone.store(true, std::memory_order_release);
    });

    // Reader thread: exposes any unsynchronised counter access to ThreadSanitizer and to the
    // snapshot staying inside the range the writers can produce.
    std::atomic<bool> readerStop{false};
    std::thread reader([&pipe, &readerStop]() {
        while (!readerStop.load(std::memory_order_acquire))
        {
            const RenderCommandPipe::Counters counters = pipe.GetCounters();
            if (counters.DroppedStaleCommands > 1000000 || counters.DroppedFullCommands > 1000000)
            {
                break;
            }
        }
    });

    while (!producerDone.load(std::memory_order_acquire))
    {
        // Frame 0 is the completed game frame: draining it while frame 1 entries are being pushed
        // must not apply them out of order or drop them.
        CHECK(pipe.FlushApplyForFrameChecked(0));
    }
    readerStop.store(true, std::memory_order_release);
    producer.join();
    reader.join();

    CHECK(produced.load() == kCommandCount);
    // The backlog for frame 1 is now complete and applied in one drain.
    CHECK(pipe.FlushApplyForFrameChecked(1));
    CHECK(applied.load() == kCommandCount);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 0);
    CHECK(pipe.GetCounters().DroppedFullCommands == 0);
}

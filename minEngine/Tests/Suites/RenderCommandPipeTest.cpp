#include "RenderCommandPipeTest.h"

#include "Access/RenderCommandPipeTestAccess.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/RenderCommandPipe.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"

#include "doctest.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
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

    /** Counts the pipe's own reports while it is armed, so a silent discard is a failed check. */
    class PipeReportSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            if (!m_bArmed.load(std::memory_order_acquire))
            {
                return;
            }
            if (record.severity != minEngine::LogSeverity::Warn)
            {
                return;
            }
            if (record.message.find(m_Substring) == std::string::npos)
            {
                return;
            }
            m_Matches.fetch_add(1, std::memory_order_relaxed);
        }

        void Arm(const std::string& substring)
        {
            m_Substring = substring;
            m_Matches.store(0, std::memory_order_relaxed);
            m_bArmed.store(true, std::memory_order_release);
        }

        uint32_t Disarm()
        {
            m_bArmed.store(false, std::memory_order_release);
            return m_Matches.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<bool> m_bArmed{ false };
        std::atomic<uint32_t> m_Matches{ 0 };
        std::string m_Substring;
    };

    PipeReportSink& Reports()
    {
        static PipeReportSink s_Sink;
        return s_Sink;
    }

    /** Registers the report sink once per process; the log system owns the reference. */
    void InstallReportSink()
    {
        static std::once_flag s_Once;
        std::call_once(s_Once, []() {
            minEngine::LogSystem::AddSink(std::shared_ptr<minEngine::ILogSink>(
                &Reports(),
                [](minEngine::ILogSink*) {
                    // Process-lifetime probe: the log system keeps the reference, so it outlives
                    // every sink call including the ones after static destruction.
                }));
        });
    }
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

TEST_CASE("render-command-pipe: a producer that samples the tag mid-publish keeps its command [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;
    pipe.PublishGameFrame(0, 1);

    // The pair a producer samples when it lands between the publisher's two stores, with the drain
    // target already moved on: frame 1 is complete and the tag still says 1.
    minEngine::Testing::TestAccess<RenderCommandPipe>::StorePublishedFrames(pipe, /*completed*/ 1, /*current*/ 1);

    // The render role drains the frame it just saw completed.
    CHECK(pipe.FlushApplyForFrameChecked(pipe.GetCompletedGameFrame()));
    CHECK(pipe.GetDrawnFrame() == 1);

    int applied = 0;
    pipe.Enqueue([&applied]() { ++applied; });

    // The command belongs to a frame the drain has not reached yet, so it must be kept. Dropping it
    // here is how one scene change disappears while the game object it belonged to stays.
    CHECK(pipe.GetCounters().DroppedStaleCommands == 0);
    CHECK(applied == 0);

    // Kept, not lost: the drain that reaches its frame applies it, in order, once.
    pipe.PublishGameFrame(2, 3);
    CHECK(pipe.FlushApplyForFrameChecked(2));
    CHECK(applied == 1);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 0);
}

TEST_CASE("render-command-pipe: a view intent filed mid-publish is taken by the next frame [smoke]")
{
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;
    pipe.PublishGameFrame(0, 1);

    // Same window, intent queue: the drain has taken frame 1 while the tag still says 1.
    minEngine::Testing::TestAccess<RenderCommandPipe>::StorePublishedFrames(pipe, /*completed*/ 1, /*current*/ 1);

    std::vector<SceneDrawDesc> taken;
    pipe.TakeViewIntents(pipe.GetCompletedGameFrame(), taken);
    CHECK(taken.empty());

    SceneDrawDesc desc{};
    desc.ViewId = 42;
    pipe.SubmitViewIntent(desc);

    // The intent is for the next frame, not for the one that is already over.
    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 0);

    pipe.PublishGameFrame(2, 3);
    pipe.TakeViewIntents(2, taken);
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].ViewId == 42);
    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 0);
}

TEST_CASE("render-command-pipe: shutdown counts the queued work it discards [smoke]")
{
    InstallReportSink();
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;
    pipe.PublishGameFrame(0, 1);

    int applied = 0;
    pipe.Enqueue([&applied]() { ++applied; });
    pipe.Enqueue([&applied]() { ++applied; });
    pipe.Enqueue([&applied]() { ++applied; });
    SceneDrawDesc desc{};
    desc.ViewId = 7;
    pipe.SubmitViewIntent(desc);

    // Scene teardown has enqueued these after the render role recorded its last frame: shutting the
    // pipe down throws the work away, and the depth is the only record that it ever existed.
    Reports().Arm("shut down with");
    pipe.Shutdown();
    const uint32_t reports = Reports().Disarm();

    CHECK(applied == 0);
    CHECK(reports == 1);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 3);
    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 1);

    // Shutting down twice reports nothing twice: the counters stay where the first one left them.
    pipe.Shutdown();
    CHECK(pipe.GetCounters().DroppedStaleCommands == 3);
    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 1);
}

TEST_CASE("render-command-pipe: enqueue and intent submission are refused after shutdown [smoke]")
{
    InstallReportSink();
    RenderCommandPipeScope scope;
    RenderCommandPipe& pipe = scope.Pipe;
    pipe.Shutdown();

    int applied = 0;
    Reports().Arm("command refused");
    pipe.Enqueue([&applied]() { ++applied; });
    CHECK(Reports().Disarm() == 1);

    // Refused, not run on the caller's thread and not lost in silence.
    CHECK(applied == 0);
    CHECK(pipe.GetCounters().DroppedStaleCommands == 1);

    SceneDrawDesc desc{};
    desc.ViewId = 3;
    Reports().Arm("view intent refused");
    pipe.SubmitViewIntent(desc);
    CHECK(Reports().Disarm() == 1);
    CHECK(pipe.GetCounters().DroppedStaleViewIntents == 1);

    std::vector<SceneDrawDesc> taken;
    pipe.TakeViewIntents(0, taken);
    CHECK(taken.empty());

    // The pipe's own API refuses; the free enqueue is a different case and stays as it is: with no
    // pipe in the process it runs the command inline, which is the only drain a headless caller has.
    // A caller that never had a pipe and a caller whose engine has shut down are indistinguishable
    // from there, so the refusal cannot be narrowed to the second.
    minEngine::EnqueueRenderCommand([&applied]() { ++applied; });
    CHECK(applied == 1);
}

#include "ProfilerTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Profiling/Profile.h"

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{
    void ResetProfilerForTest()
    {
        minEngine::Profile::Shutdown();
        minEngine::Profile::Initialize();
        minEngine::Profile::SetAutoEngineSessionEnabled(false);
        minEngine::Profile::SetEnabled(false);
    }
}

TEST_CASE("profiler: nested scopes produce parent child spans [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    REQUIRE(StartSession() != 0);
    {
        ME_PROFILE_SCOPE("Test.Root");
        {
            ME_PROFILE_SCOPE("Test.Child");
        }
    }
    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);
    REQUIRE(session->Spans.size() >= 2);

    const ProfileSpan* root = nullptr;
    const ProfileSpan* child = nullptr;
    for (const ProfileSpan& span : session->Spans)
    {
        const char* name = GetNameText(span.Name);
        if (std::string(name) == "Test.Root")
        {
            root = &span;
        }
        if (std::string(name) == "Test.Child")
        {
            child = &span;
        }
    }

    REQUIRE(root != nullptr);
    REQUIRE(child != nullptr);
    CHECK(root->ParentSpanIndex == -1);
    CHECK(child->ParentSpanIndex >= 0);
    CHECK(session->Spans[static_cast<size_t>(child->ParentSpanIndex)].Name == root->Name);
    CHECK(root->Inclusive >= child->Inclusive);
    CHECK(root->Exclusive + child->Inclusive == root->Inclusive);

    const ProfileNameAggregate* childAgg = FindNameAggregate(*session, RegisterName("Test.Child"));
    REQUIRE(childAgg != nullptr);
    CHECK(childAgg->CallCount == 1);

    Shutdown();
}

TEST_CASE("profiler: startup phase scopes do not require frames [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    StartSession();
    {
        ME_PROFILE_PHASE("Test.Startup");
        ME_PROFILE_SCOPE("Test.Startup.Work");
    }
    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);
    REQUIRE(session->Phases.size() == 1);
    CHECK(std::string(GetNameText(session->Phases[0].Name)) == "Test.Startup");
    CHECK(session->Frames.empty());

    bool found = false;
    for (const ProfileSpan& span : session->Spans)
    {
        if (std::string(GetNameText(span.Name)) == "Test.Startup.Work")
        {
            found = true;
            CHECK(span.Phase == session->Phases[0].Id);
            CHECK(span.Frame == minEngine::Profile::kInvalidFrameIndex);
        }
    }
    CHECK(found);

    Shutdown();
}

TEST_CASE("profiler: frame scopes nest under runtime phase [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    StartSession();
    BeginPhase("Test.Runtime");
    BeginFrame();
    {
        ME_PROFILE_SCOPE("Test.Tick");
    }
    EndFrame();
    EndPhase();
    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);
    REQUIRE(session->Frames.size() == 1);

    bool found = false;
    for (const ProfileSpan& span : session->Spans)
    {
        if (std::string(GetNameText(span.Name)) == "Test.Tick")
        {
            found = true;
            CHECK(span.Frame == 0);
        }
    }
    CHECK(found);

    Shutdown();
}

TEST_CASE("profiler: chrome trace export writes file [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    StartSession();
    {
        ME_PROFILE_SCOPE("Test.Export");
    }
    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);

    const std::filesystem::path outPath =
        std::filesystem::temp_directory_path() / "minengine_profiler_chrome.json";
    REQUIRE(ExportChromeTrace(*session, outPath));
    CHECK(std::filesystem::exists(outPath));
    CHECK(std::filesystem::file_size(outPath) > 0);

    std::ifstream in(outPath);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(contents.find("\"ph\":\"X\"") != std::string::npos);
    CHECK(contents.find("Test.Export") != std::string::npos);

    Shutdown();
}

TEST_CASE("profiler: disabled compile path is compile-time gated [smoke]")
{
    CHECK(minEngine::Profile::IsCompileEnabled() == (ME_ENABLE_PROFILER != 0));
}

namespace
{
    // ProfileNameRegistry caches names by pointer, so every name recorded here must be a
    // process-lifetime literal: a per-test std::string would hand the registry a pointer that is
    // later reused.
    constexpr int kConcurrentThreadCount = 4;
    const char* const kConcurrentNames[kConcurrentThreadCount] = {
        "Test.Concurrent.0",
        "Test.Concurrent.1",
        "Test.Concurrent.2",
        "Test.Concurrent.3",
    };

    constexpr int kStressThreadCount = 8;
    const char* const kStressNames[kStressThreadCount] = {
        "Test.Stress.0",
        "Test.Stress.1",
        "Test.Stress.2",
        "Test.Stress.3",
        "Test.Stress.4",
        "Test.Stress.5",
        "Test.Stress.6",
        "Test.Stress.7",
    };

    struct NameSpanStats
    {
        uint32_t Count = 0;
        uint32_t ClosedCount = 0;
        std::set<minEngine::Profile::ProfileThreadId> Threads;
    };

    NameSpanStats CollectNameStats(
        const minEngine::Profile::ProfileSession& session,
        minEngine::Profile::ProfileNameId name)
    {
        NameSpanStats stats;
        for (const minEngine::Profile::ProfileSpan& span : session.Spans)
        {
            if (span.Name != name)
            {
                continue;
            }
            ++stats.Count;
            if (span.End != 0 && span.End >= span.Start)
            {
                ++stats.ClosedCount;
            }
            stats.Threads.insert(span.Thread);
        }
        return stats;
    }
}

TEST_CASE("profiler: concurrent scopes get one recording buffer per thread [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    constexpr int kScopesPerThread = 256;

    const ProfileNameId mainName = RegisterName("Test.Concurrent.Main");
    ProfileNameId workerNameIds[kConcurrentThreadCount] = {};
    for (int i = 0; i < kConcurrentThreadCount; ++i)
    {
        workerNameIds[i] = RegisterName(kConcurrentNames[i]);
    }

    ProfileSessionDesc desc;
    desc.MaxEventCount = 1'000'000;
    REQUIRE(StartSession(desc) != 0);

    {
        ME_PROFILE_SCOPE("Test.Concurrent.Main");
    }

    std::vector<std::thread> workers;
    for (int i = 0; i < kConcurrentThreadCount; ++i)
    {
        const char* name = kConcurrentNames[i];
        workers.emplace_back(
            [name]()
            {
                for (int n = 0; n < kScopesPerThread; ++n)
                {
                    ME_PROFILE_SCOPE(name);
                }
            });
    }
    for (std::thread& worker : workers)
    {
        worker.join();
    }

    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);

    // One recording buffer registers exactly one thread, so the thread count is a direct
    // observable of how many buffers were used.
    CHECK(session->Threads.size() == static_cast<size_t>(kConcurrentThreadCount + 1));

    const uint64_t expectedEvents =
        2ull + (2ull * static_cast<uint64_t>(kConcurrentThreadCount) * static_cast<uint64_t>(kScopesPerThread));
    CHECK(session->Stats.EventsRecorded == expectedEvents);
    CHECK(session->Events.size() == expectedEvents);

    std::set<ProfileThreadId> workerThreads;
    for (int i = 0; i < kConcurrentThreadCount; ++i)
    {
        const NameSpanStats stats = CollectNameStats(*session, workerNameIds[i]);
        CHECK(stats.Count == static_cast<uint32_t>(kScopesPerThread));
        CHECK(stats.ClosedCount == static_cast<uint32_t>(kScopesPerThread));
        REQUIRE(stats.Threads.size() == 1);
        workerThreads.insert(*stats.Threads.begin());
    }
    CHECK(workerThreads.size() == static_cast<size_t>(kConcurrentThreadCount));

    const NameSpanStats mainStats = CollectNameStats(*session, mainName);
    REQUIRE(mainStats.Threads.size() == 1);
    CHECK(workerThreads.count(*mainStats.Threads.begin()) == 0);

    Shutdown();
}

TEST_CASE("profiler: exited thread events survive until collection [smoke]")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    constexpr int kScopesOnWorker = 32;
    const ProfileNameId workerNameId = RegisterName("Test.ShortLived");
    const ProfileNameId mainName = RegisterName("Test.OutlivesWorker");

    REQUIRE(StartSession() != 0);

    {
        std::thread worker(
            []()
            {
                for (int n = 0; n < kScopesOnWorker; ++n)
                {
                    ME_PROFILE_SCOPE("Test.ShortLived");
                }
            });
        worker.join(); // thread storage is gone before the session is collected
    }

    {
        ME_PROFILE_SCOPE("Test.OutlivesWorker");
    }

    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);

    const NameSpanStats workerStats = CollectNameStats(*session, workerNameId);
    CHECK(workerStats.Count == static_cast<uint32_t>(kScopesOnWorker));
    CHECK(workerStats.ClosedCount == static_cast<uint32_t>(kScopesOnWorker));
    REQUIRE(workerStats.Threads.size() == 1);

    const NameSpanStats mainStats = CollectNameStats(*session, mainName);
    CHECK(mainStats.Count == 1);
    REQUIRE(mainStats.Threads.size() == 1);
    CHECK(*mainStats.Threads.begin() != *workerStats.Threads.begin());

    CHECK(session->Stats.EventsRecorded == static_cast<uint64_t>(2 * kScopesOnWorker) + 2ull);
    CHECK(session->Events.size() == session->Stats.EventsRecorded);

    Shutdown();
}

TEST_CASE("profiler: concurrent scope stress keeps buffers intact")
{
    using namespace minEngine::Profile;

    minEngine::LogSystem::Initialize();
    ResetProfilerForTest();

    constexpr int kScopesPerThread = 4000;

    ProfileNameId workerNameIds[kStressThreadCount] = {};
    for (int i = 0; i < kStressThreadCount; ++i)
    {
        workerNameIds[i] = RegisterName(kStressNames[i]);
    }
    const ProfileNameId mainName = RegisterName("Test.Stress.Main");

    ProfileSessionDesc desc;
    desc.MaxEventCount = 2'000'000;
    REQUIRE(StartSession(desc) != 0);

    std::vector<std::thread> workers;
    for (int i = 0; i < kStressThreadCount; ++i)
    {
        const char* name = kStressNames[i];
        workers.emplace_back(
            [name]()
            {
                for (int n = 0; n < kScopesPerThread; ++n)
                {
                    ME_PROFILE_SCOPE(name);
                }
            });
    }
    for (int n = 0; n < kScopesPerThread; ++n)
    {
        ME_PROFILE_SCOPE("Test.Stress.Main");
    }
    for (std::thread& worker : workers)
    {
        worker.join();
    }

    StopSession();

    const ProfileSession* session = GetLastCompletedSession();
    REQUIRE(session != nullptr);

    CHECK(session->Threads.size() == static_cast<size_t>(kStressThreadCount + 1));
    const uint64_t expectedEvents = 2ull * static_cast<uint64_t>(kStressThreadCount + 1) *
                                   static_cast<uint64_t>(kScopesPerThread);
    CHECK(session->Stats.EventsRecorded == expectedEvents);
    CHECK(session->Events.size() == expectedEvents);

    for (int i = 0; i < kStressThreadCount; ++i)
    {
        const NameSpanStats stats = CollectNameStats(*session, workerNameIds[i]);
        CHECK(stats.Count == static_cast<uint32_t>(kScopesPerThread));
        REQUIRE(stats.Threads.size() == 1);
    }
    const NameSpanStats mainStats = CollectNameStats(*session, mainName);
    CHECK(mainStats.Count == static_cast<uint32_t>(kScopesPerThread));
    REQUIRE(mainStats.Threads.size() == 1);

    Shutdown();
}

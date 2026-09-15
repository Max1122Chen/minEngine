#include "ProfilerTest.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Profiling/Profile.h"

#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

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

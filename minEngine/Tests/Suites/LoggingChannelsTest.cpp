#include "LoggingChannelsTest.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "doctest.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
    class SessionLogRestore final
    {
    public:
        explicit SessionLogRestore(const std::filesystem::path& directory)
        {
            minEngine::LogSystem::Shutdown();
            minEngine::LogSystem::SetSessionLogDirectory(directory);
            minEngine::LogSystem::Initialize();
        }

        ~SessionLogRestore()
        {
            minEngine::LogSystem::Shutdown();
            minEngine::LogSystem::SetSessionLogDirectory({});
            minEngine::LogSystem::Initialize();
        }

        SessionLogRestore(const SessionLogRestore&) = delete;
        SessionLogRestore& operator=(const SessionLogRestore&) = delete;
    };

    class CountingSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            ++WriteCount;
            LastChannelName = record.GetChannelName();
            LastSeverity = record.severity;
            LastMessage = record.message;
            LastChannel = record.channel;
        }

        int WriteCount = 0;
        std::string LastChannelName;
        std::string LastMessage;
        minEngine::LogSeverity LastSeverity = minEngine::LogSeverity::Info;
        const minEngine::LogChannelBase* LastChannel = nullptr;
    };
}

TEST_CASE("logging-channels: threshold suppresses lower severity [smoke]")
{
    using namespace minEngine;

    LogSystem::Initialize();
    LogConsoleStorage::Clear();

    const LogSeverity previous = LogCore.GetSeverity();
    LogCore.SetSeverity(LogSeverity::Error);

    const size_t before = LogConsoleStorage::Snapshot().size();
    ME_LOG(LogCore, Info, "should-be-suppressed");
    ME_LOG(LogCore, Error, "should-pass");

    const auto entries = LogConsoleStorage::Snapshot();
    REQUIRE(entries.size() >= before + 1);

    bool foundPass = false;
    bool foundSuppressed = false;
    for (size_t i = before; i < entries.size(); ++i)
    {
        if (entries[i].message == "should-pass")
        {
            foundPass = true;
            CHECK(entries[i].severity == LogSeverity::Error);
            CHECK(entries[i].channel == &LogCore);
            CHECK(std::string(entries[i].GetChannelName()) == "Core");
        }
        if (entries[i].message == "should-be-suppressed")
        {
            foundSuppressed = true;
        }
    }

    CHECK(foundPass);
    CHECK_FALSE(foundSuppressed);

    LogCore.SetSeverity(previous);
}

TEST_CASE("logging-channels: sink receives channel pointer [smoke]")
{
    using namespace minEngine;

    LogSystem::Initialize();

    auto sink = std::make_shared<CountingSink>();
    LogSystem::AddSink(sink);

    ME_LOG(LogTest, Warn, "channel-pointer-check");

    REQUIRE(sink->WriteCount >= 1);
    CHECK(sink->LastChannel == &LogTest);
    CHECK(sink->LastChannelName == "Test");
    CHECK(sink->LastSeverity == LogSeverity::Warn);
    CHECK(sink->LastMessage == "channel-pointer-check");
}

TEST_CASE("logging-channels: console ring capacity and displayTime [smoke]")
{
    using namespace minEngine;

    LogSystem::Initialize();
    LogConsoleStorage::Clear();

    const LogSeverity previous = LogCore.GetSeverity();
    LogCore.SetSeverity(LogSeverity::Trace);

    for (int i = 0; i < 2100; ++i)
    {
        ME_LOG(LogCore, Info, "ring-fill-{}", i);
    }

    const auto entries = LogConsoleStorage::Snapshot();
    CHECK(entries.size() == 2000);
    REQUIRE_FALSE(entries.empty());
    CHECK_FALSE(entries.front().displayTime.empty());
    CHECK_FALSE(entries.back().displayTime.empty());
    CHECK(entries.front().message == "ring-fill-100");
    CHECK(entries.back().message == "ring-fill-2099");
    CHECK(LogConsoleStorage::IsTruncated());

    uint64_t generationA = LogConsoleStorage::GetGeneration();
    std::vector<LogRecord> cached;
    LogConsoleStorage::CopyInto(cached);
    CHECK(cached.size() == entries.size());
    CHECK(LogConsoleStorage::GetGeneration() == generationA);

    LogConsoleStorage::Clear();
    CHECK_FALSE(LogConsoleStorage::IsTruncated());

    LogCore.SetSeverity(previous);
}

TEST_CASE("logging-channels: session file flushes warn and writes project pointer [smoke]")
{
    using namespace minEngine;
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "minengine-session-log";
    fs::remove_all(root);
    fs::create_directories(root);

    {
        SessionLogRestore session(root);
        LogConsoleStorage::Clear();
        ME_LOG(LogCore, Info, "session-info-marker");
        ME_LOG(LogCore, Warn, "session-warn-marker");

        const fs::path logPath = LogSystem::GetSessionLogPath();
        REQUIRE(fs::exists(logPath));
        CHECK(logPath.parent_path() == root);

        std::ifstream logFile(logPath);
        std::string contents((std::istreambuf_iterator<char>(logFile)), std::istreambuf_iterator<char>());
        CHECK(contents.find("session-warn-marker") != std::string::npos);

        const fs::path projectRoot = root / "project";
        REQUIRE(LogSystem::PublishSessionPointer(projectRoot));
        const fs::path pointerPath = LogSystem::GetSessionPointerPath();
        REQUIRE(fs::exists(pointerPath));
        CHECK(pointerPath.filename().string().rfind("latest-session-", 0) == 0);

        std::ifstream pointerFile(pointerPath);
        std::string pointerLine;
        std::getline(pointerFile, pointerLine);
        CHECK(pointerLine == logPath.string());

        LogConsoleStorage::Clear();
        ME_LOG(LogCore, Info, "tail-info");
        ME_LOG(LogCore, Warn, "tail-warn");
        const std::vector<LogRecord> warnTail = LogConsoleStorage::Tail(1, LogSeverity::Info);
        REQUIRE(warnTail.size() == 1);
        CHECK(warnTail.front().message == "tail-warn");
        const std::vector<LogRecord> warnOnly = LogConsoleStorage::Tail(10, LogSeverity::Warn);
        REQUIRE(warnOnly.size() == 1);
        CHECK(warnOnly.front().message == "tail-warn");
    }

    fs::remove_all(root);
}

TEST_CASE("logging-channels: session directory keeps twenty files [smoke]")
{
    using namespace minEngine;
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "minengine-session-log-prune";
    fs::remove_all(root);
    fs::create_directories(root);

    const auto now = fs::file_time_type::clock::now();
    for (int index = 0; index < 22; ++index)
    {
        const fs::path oldFile = root / ("minEngine-old-" + std::to_string(index) + ".log");
        std::ofstream output(oldFile);
        output << "old\n";
        output.close();
        fs::last_write_time(oldFile, now - std::chrono::hours(22 - index));
    }

    {
        SessionLogRestore session(root);
        const fs::path logPath = LogSystem::GetSessionLogPath();
        REQUIRE(fs::exists(logPath));

        int sessionFiles = 0;
        bool oldestRemains = false;
        for (const fs::directory_entry& entry : fs::directory_iterator(root))
        {
            const std::string name = entry.path().filename().string();
            if (name.rfind("minEngine-", 0) == 0 && entry.path().extension() == ".log")
            {
                ++sessionFiles;
            }
            if (name == "minEngine-old-0.log")
            {
                oldestRemains = true;
            }
        }
        CHECK(sessionFiles == 20);
        CHECK_FALSE(oldestRemains);
    }

    fs::remove_all(root);
}

TEST_CASE("logging-channels: unwritable session directory skips the file sink [smoke]")
{
    using namespace minEngine;
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "minengine-session-log-blocked";
    fs::remove_all(root);
    fs::create_directories(root);
    const fs::path notADirectory = root / "not-a-directory";
    {
        std::ofstream blocker(notADirectory);
        blocker << "x";
    }

    {
        SessionLogRestore session(notADirectory);
        LogConsoleStorage::Clear();
        ME_LOG(LogCore, Info, "still-in-ring");
        CHECK(LogSystem::GetSessionLogPath().empty());
        const std::vector<LogRecord> entries = LogConsoleStorage::Snapshot();
        bool found = false;
        for (const LogRecord& entry : entries)
        {
            if (entry.message == "still-in-ring")
            {
                found = true;
            }
        }
        CHECK(found);
    }

    fs::remove_all(root);
}

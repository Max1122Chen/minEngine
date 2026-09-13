#include "LoggingChannelsTest.h"

#include "Runtime/Core/Log/LogConsole.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "doctest.h"

#include <string>

namespace
{
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

    uint64_t generationA = LogConsoleStorage::GetGeneration();
    std::vector<LogRecord> cached;
    LogConsoleStorage::CopyInto(cached);
    CHECK(cached.size() == entries.size());
    CHECK(LogConsoleStorage::GetGeneration() == generationA);

    LogCore.SetSeverity(previous);
}

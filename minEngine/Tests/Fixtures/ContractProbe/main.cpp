#include "ReleaseGradeProbe.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Assert/AssertLog.h"
#include "Runtime/Core/Log/LogRecord.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

/**
 * Drives one contract check on request, in a process the suite is allowed to lose. A fatal assert
 * takes the process with it, so nothing about that case can be observed from inside the process it
 * happens in: the trace and the reports on the standard error handle are the evidence.
 *
 * Scenarios:
 *   fatal-assert     ME_ASSERT under the fatal grade: must stop the process
 *   release-assert   ME_ASSERT under the release grade: must record, count and continue
 *   log-lock-held    a violation raised from inside a log sink, i.e. with the log system's own mutex
 *                    held by the violating thread: must record without asking for that mutex again
 *
 * The trace is appended with a file handle rather than through the log system, so "the report path
 * returned to the log system" stays visible from outside the process.
 */

namespace
{
    const char* g_tracePath = nullptr;

    void TraceLine(const char* text)
    {
        if (g_tracePath == nullptr)
        {
            return;
        }

        const HANDLE file = ::CreateFileA(
            g_tracePath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return;
        }
        DWORD written = 0;
        ::WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
        ::WriteFile(file, "\n", 1, &written, nullptr);
        ::CloseHandle(file);
    }

    void TraceCounters(const minEngine::ContractViolationCounters& counters)
    {
        char line[192]{};
        std::snprintf(
            line,
            sizeof(line),
            "counters total=%llu continued=%llu trapped=%llu suppressed=%llu",
            static_cast<unsigned long long>(counters.Total()),
            static_cast<unsigned long long>(counters.RecordedOnContinue),
            static_cast<unsigned long long>(counters.RecordedOnTrap),
            static_cast<unsigned long long>(counters.Suppressed));
        TraceLine(line);
    }

    /** Fires on the first log record it sees, which is a record written with the log mutex held. */
    class ViolationSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            if (record.message.find("contract violation") != std::string::npos)
            {
                TraceLine("sink-saw-contract-violation");
            }

            if (::InterlockedIncrement(&m_Entries) != 1)
            {
                return;
            }

            // The log system calls every sink while holding its mutex, so this violation is raised
            // with that mutex held by this very thread. A report that asked for it again would wait
            // for itself; the suite gives this process a bounded time to show the difference.
            ME_ENSURE(false, "violation raised inside a log sink");
            TraceLine("sink-returned");
        }

    private:
        volatile long m_Entries = 0;
    };

    void TakeFatalAssert()
    {
        TraceLine("before-fatal-assert");
        ME_ASSERT(false, "fatal ME_ASSERT: the premise no longer holds");
        TraceLine("after-fatal-assert");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argv[1] == nullptr)
    {
        std::fprintf(stderr, "usage: contract-probe <fatal-assert|release-assert|log-lock-held> [trace]\n");
        return 2;
    }

    const char* scenario = argv[1];
    g_tracePath = argc > 2 ? argv[2] : nullptr;

    // A frame role exists so every report below can name the role of the reporting thread.
    minEngine::FrameThreading threading;
    threading.Initialize(minEngine::FrameThreadMode::Single);

    TraceLine("probe-started");

    if (std::strcmp(scenario, "fatal-assert") == 0)
    {
        TakeFatalAssert();
        TraceLine("survived");
        return 0;
    }

    if (std::strcmp(scenario, "release-assert") == 0)
    {
        const bool bContinued = contractprobe::RunReleaseGradeAssertViolation();
        TraceLine(bContinued ? "survived-release-assert" : "did-not-continue");

        const minEngine::ContractViolationFrameSample sample =
            minEngine::PublishAndSampleContractViolations();
        char line[64]{};
        std::snprintf(line, sizeof(line), "published=%u", sample.Published);
        TraceLine(line);
        TraceCounters(sample.Counters);
        return 0;
    }

    if (std::strcmp(scenario, "log-lock-held") == 0)
    {
        std::shared_ptr<ViolationSink> sink = std::make_shared<ViolationSink>();
        minEngine::LogSystem::AddSink(sink);
        ME_LOG(minEngine::LogTest, Info, "contract-probe: logging with a sink that violates a contract.");
        TraceLine("log-returned");

        const minEngine::ContractViolationFrameSample sample =
            minEngine::PublishAndSampleContractViolations();
        char line[64]{};
        std::snprintf(line, sizeof(line), "published=%u", sample.Published);
        TraceLine(line);
        TraceCounters(sample.Counters);
        TraceLine("survived-log-lock");
        return 0;
    }

    std::fprintf(stderr, "contract-probe: unknown scenario '%s'\n", scenario);
    return 2;
}

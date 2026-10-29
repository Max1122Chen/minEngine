#include "Platform/EditorCrashDiagnostics.h"

#include "Runtime/Core/Log/LogRecord.h"
#include "Runtime/Core/Log/LogSystem.h"

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
 * Faults a process on request, so the suite that spawned it can read back what the crash record
 * path did. A fault tears down the process it happens in, so nothing about the record can be
 * observed from inside that process: the record file and this trace file are the evidence.
 *
 * Scenarios:
 *   access-violation  the ordinary case
 *   stack-overflow    the faulting thread has no usable stack left
 *   log-lock-held     the fault is raised from inside a log sink, i.e. with the log system's own
 *                     mutex held by the faulting thread
 *
 * The trace is one line per log-system entry, appended with a file handle rather than through the
 * log system, so "the crash path returned to the log system" is visible from outside the process.
 */

namespace
{
    const char* g_tracePath = nullptr;
    volatile long g_logSinkEntries = 0;

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

    __attribute__((noinline)) void RecurseUntilStackOverflow()
    {
        volatile char padding[4096];
        padding[0] = static_cast<char>(g_logSinkEntries);
        padding[sizeof(padding) - 1] = padding[0];
        RecurseUntilStackOverflow();
        padding[0] = 0;
    }

    /**
     * LogSystem::Emit() calls every sink while holding its mutex, so the fault below is raised with
     * the log lock held by the faulting thread. Only the first entry faults: a later entry is the
     * crash path coming back for that same lock, which is what the trace has to show.
     */
    class FaultingSink final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord&) override
        {
            const long entry = ::InterlockedIncrement(&g_logSinkEntries);
            char line[64]{};
            std::snprintf(line, sizeof(line), "log-sink-entry=%ld", entry);
            TraceLine(line);

            if (entry == 1)
            {
                volatile int* nowhere = nullptr;
                *nowhere = 1;
            }
        }
    };

    void TakeAccessViolation()
    {
        volatile int* nowhere = nullptr;
        *nowhere = 1;
        std::fprintf(stderr, "TakeAccessViolation: unreachable\n");
    }
}

int main(int argc, char** argv)
{
    if (argc < 2 || argv[1] == nullptr)
    {
        std::fprintf(stderr, "usage: crash-probe <access-violation|stack-overflow|log-lock-held> [trace]\n");
        return 2;
    }

    const char* scenario = argv[1];
    g_tracePath = argc > 2 ? argv[2] : nullptr;

    const std::string recordPath = minEngine::InstallEditorCrashDiagnostics();
    std::fprintf(stderr, "crash-probe: scenario=%s record='%s'\n", scenario, recordPath.c_str());

    if (std::strcmp(scenario, "access-violation") == 0)
    {
        TakeAccessViolation();
        return 0;
    }

    if (std::strcmp(scenario, "stack-overflow") == 0)
    {
        RecurseUntilStackOverflow();
        return 0;
    }

    if (std::strcmp(scenario, "log-lock-held") == 0)
    {
        std::shared_ptr<FaultingSink> sink = std::make_shared<FaultingSink>();
        minEngine::LogSystem::AddSink(sink);
        ME_LOG(minEngine::LogTest, Info, "crash-probe: logging with a sink that faults.");
        TraceLine("log-sink-returned");
        return 0;
    }

    std::fprintf(stderr, "crash-probe: unknown scenario '%s'\n", scenario);
    return 2;
}

#include "Platform/EditorCrashDiagnostics.h"

#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <string>

/**
 * The crash record is written because a process is going down, so no test can observe the record
 * path from inside the process it runs in. Every case here spawns the probe that sits next to this
 * executable, lets it fault on request, and then reads the files it left behind: the record, and a
 * trace of every entry into the log system.
 *
 * The wait is bounded on purpose. A record path that asks for a lock the faulting thread already
 * holds does not fail, it blocks - so the failure that has to be visible is a child that never
 * stops.
 */

namespace
{
    constexpr DWORD kProbeTimeoutMs = 30000;
    constexpr DWORD kProbeKillTimeoutMs = 5000;

    std::string ExecutableDirectory()
    {
        wchar_t modulePath[MAX_PATH]{};
        const DWORD length = ::GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return std::string();
        }

        std::wstring directory(modulePath, length);
        const std::wstring::size_type separator = directory.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
        {
            return std::string();
        }
        directory.resize(separator + 1);

        char narrow[MAX_PATH * 2]{};
        const int written = ::WideCharToMultiByte(
            CP_UTF8, 0, directory.c_str(), -1, narrow, sizeof(narrow), nullptr, nullptr);
        if (written <= 0)
        {
            return std::string();
        }
        return std::string(narrow);
    }

    std::string CrashRecordPath()
    {
        return ExecutableDirectory() + "ed_crash.log";
    }

    std::string ProbeTracePath()
    {
        return ExecutableDirectory() + "ed_crash_probe_trace.log";
    }

    unsigned long long FileSize(const std::string& path)
    {
        const HANDLE file = ::CreateFileA(
            path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        LARGE_INTEGER size{};
        const BOOL asked = ::GetFileSizeEx(file, &size);
        ::CloseHandle(file);
        return asked != FALSE ? static_cast<unsigned long long>(size.QuadPart) : 0;
    }

    /** Bytes appended since `offset`: what this one child wrote, without earlier runs' output. */
    std::string ReadAppendedText(const std::string& path, unsigned long long offset)
    {
        const HANDLE file = ::CreateFileA(
            path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            return std::string();
        }

        LARGE_INTEGER size{};
        std::string text;
        if (::GetFileSizeEx(file, &size) != FALSE && size.QuadPart > static_cast<LONGLONG>(offset))
        {
            LARGE_INTEGER start{};
            start.QuadPart = static_cast<LONGLONG>(offset);
            if (::SetFilePointerEx(file, start, nullptr, FILE_BEGIN) != FALSE)
            {
                const DWORD remaining = static_cast<DWORD>(size.QuadPart - static_cast<LONGLONG>(offset));
                text.resize(remaining);
                DWORD read = 0;
                if (::ReadFile(file, text.data(), remaining, &read, nullptr) != FALSE)
                {
                    text.resize(read);
                }
                else
                {
                    text.clear();
                }
            }
        }
        ::CloseHandle(file);
        return text;
    }

    int Occurrences(const std::string& text, const char* needle)
    {
        int count = 0;
        const std::string pattern(needle);
        for (std::string::size_type at = text.find(pattern); at != std::string::npos;
             at = text.find(pattern, at + pattern.size()))
        {
            ++count;
        }
        return count;
    }

    struct ProbeResult
    {
        bool Started = false;
        bool Stopped = false;
        unsigned long ExitCode = 0;
        std::string Record;
        std::string Trace;
    };

    ProbeResult RunProbe(const char* scenario)
    {
        ProbeResult result;

        const std::string probe = ExecutableDirectory() + "minEngineCrashProbe.exe";
        const std::string recordPath = CrashRecordPath();
        const std::string tracePath = ProbeTracePath();
        const unsigned long long recordOffset = FileSize(recordPath);
        const unsigned long long traceOffset = FileSize(tracePath);

        std::wstring commandLine(probe.begin(), probe.end());
        commandLine += L" ";
        const std::string scenarioText(scenario);
        commandLine += std::wstring(scenarioText.begin(), scenarioText.end());
        commandLine += L" ";
        commandLine += std::wstring(tracePath.begin(), tracePath.end());

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (::CreateProcessW(
                nullptr,
                commandLine.data(),
                nullptr,
                nullptr,
                FALSE,
                CREATE_NO_WINDOW,
                nullptr,
                nullptr,
                &startup,
                &process) == FALSE)
        {
            return result;
        }
        result.Started = true;

        const DWORD waited = ::WaitForSingleObject(process.hProcess, kProbeTimeoutMs);
        if (waited == WAIT_OBJECT_0)
        {
            result.Stopped = true;
            ::GetExitCodeProcess(process.hProcess, &result.ExitCode);
        }
        else
        {
            // A record path that waits on a lock the faulting thread holds never returns. Kill the
            // child so one blocked case cannot leave the suite hanging behind it.
            ::TerminateProcess(process.hProcess, 1);
            ::WaitForSingleObject(process.hProcess, kProbeKillTimeoutMs);
        }

        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);

        result.Record = ReadAppendedText(recordPath, recordOffset);
        result.Trace = ReadAppendedText(tracePath, traceOffset);
        return result;
    }

    /** The whole record goes into the failure text: a missing field is only diagnosable with it. */
    std::string DescribeRecord(const ProbeResult& run, const char* note)
    {
        std::string text(note);
        text += "\n--- record ---\n";
        text += run.Record;
        text += "\n--- log-system trace ---\n";
        text += run.Trace;
        return text;
    }

    void CheckField(const ProbeResult& run, const char* field)
    {
        const std::string note = std::string("the record has no '") + field + "'";
        CHECK_MESSAGE(run.Record.find(field) != std::string::npos, DescribeRecord(run, note.c_str()).c_str());
    }

    void CheckStopped(const ProbeResult& run)
    {
        CHECK_MESSAGE(
            run.Stopped,
            DescribeRecord(run, "the probe did not stop within the timeout; the record path blocked")
                .c_str());
    }
}

TEST_CASE("crash-diagnostics: a fault with the log lock held records without re-entering the log")
{
    // The log system calls every sink while holding its own mutex, so the fault below is raised
    // with that lock taken by the faulting thread. Recording it must not ask for the lock again:
    // the trace names every entry into the log system, and a second entry would be the crash path
    // coming back for a lock it can never get.
    const ProbeResult run = RunProbe("log-lock-held");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");

    CheckStopped(run);
    CHECK_MESSAGE(
        Occurrences(run.Trace, "log-sink-entry=1") == 1,
        DescribeRecord(run, "the faulting log write never entered the sink").c_str());
    CHECK_MESSAGE(
        Occurrences(run.Trace, "log-sink-entry=2") == 0,
        DescribeRecord(run, "the crash record path re-entered the log system").c_str());
    CHECK_MESSAGE(
        Occurrences(run.Record, "=== Editor crash") == 1,
        DescribeRecord(run, "the record path ran more than once for one fault").c_str());

    CheckField(run, "LoadedModules (");
    CheckField(run, "FaultThread: id=");
    CheckField(run, "FaultStackWords (");
    CheckField(run, "MiniDump: ");
}

TEST_CASE("crash-diagnostics: a stack overflow still produces a record")
{
    // The faulting thread has consumed its last page when this arrives, so the record has to be
    // produced somewhere other than the stack the fault was raised on.
    const ProbeResult run = RunProbe("stack-overflow");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");

    CheckStopped(run);
    CHECK_MESSAGE(
        run.Record.find("ExceptionCode=0xC00000FD") != std::string::npos,
        DescribeRecord(run, "no stack overflow record").c_str());
    CheckField(run, "FaultStackWords (");
    CheckField(run, "FilterStack (");
    // The dump is the part that needs real stack and a real loader: it is the first thing lost
    // when the record path runs on the stack that just overflowed.
    CheckField(run, "MiniDump: ");
}

TEST_CASE("crash-diagnostics: the record keeps the fields a reader resolves")
{
    // Every field below is what makes an address symbolisable after the fact and what names the
    // thread the fault happened on; a record missing one of them sends the reader back to guessing.
    const ProbeResult run = RunProbe("access-violation");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");

    CheckStopped(run);
    CHECK_MESSAGE(
        Occurrences(run.Record, "=== Editor crash") == 1,
        DescribeRecord(run, "the record path ran more than once for one fault").c_str());

    CheckField(run, "=== Editor crash ");
    CheckField(run, "LoadedModules (");
    CheckField(run, "base=0x");
    CheckField(run, "imagebase=0x");
    CheckField(run, "timestamp=0x");
    CheckField(run, "FaultThread: id=");
    CheckField(run, " logid=");
    CheckField(run, " role=");
    CheckField(run, " start=");
    CheckField(run, "ExceptionCode=0xC0000005");
    CheckField(run, "FaultRIP=");
    CheckField(run, "FaultStackWords (");
    CheckField(run, "FilterStack (");
    CheckField(run, "MiniDump: ");

    // An address is only resolvable as "<module>+0x<RVA>"; a bare number returns the reader to a
    // guessing game against a load base that changes every run.
    CHECK_MESSAGE(
        run.Record.find("+0x") != std::string::npos,
        DescribeRecord(run, "no module-relative address in the record").c_str());
}
TEST_CASE("crash-diagnostics: the once guard records once and turns away a fault inside itself [smoke]")
{
    // This case stays in the living process: it drives the fault path directly, with a second fault
    // raised from inside it, and reads the record it leaves. The guard is the point - a record path
    // that recursed into itself would either nest on its own storage or never finish.
    const std::string recordPath = minEngine::InstallEditorCrashDiagnostics();
    REQUIRE_MESSAGE(!recordPath.empty(), "the crash record path was not installed");
    REQUIRE_MESSAGE(recordPath == CrashRecordPath(), "records go to '", recordPath.c_str(), "'");

    const unsigned long long offset = FileSize(recordPath);
    const minEngine::CrashRecordSelfCheckResult check = minEngine::RunCrashRecordSelfCheck(true);

    CHECK_MESSAGE(check.Admitted == 1, "the fault path ran ", check.Admitted, " times, not once");
    CHECK_MESSAGE(check.Rejected == 1, "a fault raised inside the record path was admitted");
    CHECK_MESSAGE(check.RecordWritten, "the record file was not written");

    const ProbeResult run{true, true, 0, ReadAppendedText(recordPath, offset), std::string()};
    CHECK_MESSAGE(
        Occurrences(run.Record, "=== Editor crash") == 1,
        DescribeRecord(run, "one fault did not produce exactly one record").c_str());

    CheckField(run, "LoadedModules (");
    CheckField(run, "FaultThread: id=");
    CheckField(run, "FaultRIP=");
    CheckField(run, "FaultStackWords (");
    CheckField(run, "FilterStack (");
    CheckField(run, "MiniDump: ");
}

#include "ContractViolationsTest.h"

#include "Runtime/Core/Assert/Assert.h"
#include "Runtime/Core/Assert/AssertLog.h"
#include "Runtime/Core/Log/LogRecord.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Tasks/TaskSystem.h"
#include "Runtime/Core/Threading/FrameThreading.h"
#include "Runtime/Engine.h"

#include "doctest.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

using minEngine::FrameThreadKind;
using minEngine::FrameThreadMode;
using minEngine::FrameThreading;
using minEngine::TaskSystem;

/**
 * The two grades of a contract check, and the four things that make them usable: the count, the
 * record, the budget that keeps a per-frame violation out of the log, and a report path that never
 * asks for the log mutex.
 *
 * Three of the cases cannot run in this process - a fatal assert ends the process it fires in, and a
 * report path that re-entered the log mutex would hang it rather than fail - so those spawn the
 * probe that sits next to this executable and read the trace it leaves behind.
 */

namespace
{
    constexpr DWORD kProbeTimeoutMs = 30000;
    constexpr DWORD kProbeKillTimeoutMs = 5000;

    /** Keeps the records this module publishes, so one case can read what the log system got. */
    class ContractRecorder final : public minEngine::ILogSink
    {
    public:
        void Write(const minEngine::LogRecord& record) override
        {
            if (!m_Active.load(std::memory_order_relaxed))
            {
                return;
            }
            if (record.message.find("contract violation") == std::string::npos)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Messages.push_back(record.message);
            m_Severities.push_back(record.severity);
        }

        void Begin()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Messages.clear();
            m_Severities.clear();
            m_Active.store(true, std::memory_order_relaxed);
        }

        void End() { m_Active.store(false, std::memory_order_relaxed); }

        std::vector<std::string> Messages()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Messages;
        }

        std::vector<minEngine::LogSeverity> Severities()
        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return m_Severities;
        }

    private:
        std::mutex m_Mutex;
        std::atomic<bool> m_Active{false};
        std::vector<std::string> m_Messages;
        std::vector<minEngine::LogSeverity> m_Severities;
    };

    /** The recorder outlives the process on purpose: the log system holds a sink for its lifetime. */
    ContractRecorder& Recorder()
    {
        static ContractRecorder* recorder = new ContractRecorder();
        static const bool s_Installed = [recorder]() {
            minEngine::LogSystem::AddSink(
                std::shared_ptr<minEngine::ILogSink>(recorder, [](minEngine::ILogSink*) {}));
            return true;
        }();
        (void)s_Installed;
        return *recorder;
    }

    void ViolateOnceAtOneSite()
    {
        ME_ENSURE(false, "one recoverable violation");
    }

    void ViolateRepeatedlyAtOneSite()
    {
        ME_ENSURE(false, "a violation that fires every frame");
    }

    /** Raised between two frames of a host that drives its own loop instead of calling Run. */
    void ViolateAtTheEditorBoundary()
    {
        ME_ENSURE(false, "a violation raised between two editor frames");
    }

    /** Same shape, distinct site: a site keeps its own report budget. */
    void ViolateAtTheSecondBoundary()
    {
        ME_ENSURE(false, "a violation raised before a repeated frame boundary");
    }

    std::size_t CountMessagesContaining(const std::vector<std::string>& messages, const char* needle)
    {
        std::size_t count = 0;
        for (const std::string& message : messages)
        {
            if (message.find(needle) != std::string::npos)
            {
                ++count;
            }
        }
        return count;
    }

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

    /** Bytes appended since the offset: what this one child wrote, without earlier runs' output. */
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

    bool Contains(const std::string& text, const char* needle)
    {
        return text.find(needle) != std::string::npos;
    }

    struct ProbeResult
    {
        bool Started = false;
        bool Stopped = false;
        unsigned long ExitCode = 0;
        std::string Trace;
        std::string ErrorText;
    };

    std::string Describe(const ProbeResult& run, const char* note)
    {
        std::string text(note);
        text += "\n--- trace ---\n";
        text += run.Trace;
        text += "\n--- standard error ---\n";
        text += run.ErrorText;
        return text;
    }

    /**
     * Runs the probe with its standard error redirected to a file: the report lines are the evidence
     * that the check reached the report path, and a dying process cannot be asked for them
     * afterwards.
     */
    ProbeResult RunProbe(const char* scenario)
    {
        ProbeResult result;

        const std::string probe = ExecutableDirectory() + "minEngineContractProbe.exe";
        const std::string tracePath = ExecutableDirectory() + "contract_probe_trace.log";
        const std::string errorPath = ExecutableDirectory() + "contract_probe_stderr.log";
        const unsigned long long traceOffset = FileSize(tracePath);
        const unsigned long long errorOffset = FileSize(errorPath);

        SECURITY_ATTRIBUTES attributes{};
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        const HANDLE errorFile = ::CreateFileA(
            errorPath.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &attributes,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (errorFile == INVALID_HANDLE_VALUE)
        {
            return result;
        }

        std::wstring commandLine(probe.begin(), probe.end());
        commandLine += L" ";
        const std::string scenarioText(scenario);
        commandLine += std::wstring(scenarioText.begin(), scenarioText.end());
        commandLine += L" ";
        commandLine += std::wstring(tracePath.begin(), tracePath.end());

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdInput = ::GetStdHandle(STD_INPUT_HANDLE);
        startup.hStdOutput = errorFile;
        startup.hStdError = errorFile;

        PROCESS_INFORMATION process{};
        const BOOL started = ::CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process);
        ::CloseHandle(errorFile);
        if (started == FALSE)
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
            // A report path that waits for a lock the violating thread holds never returns; one
            // hanging case must not leave the suite behind it hanging too.
            ::TerminateProcess(process.hProcess, 1);
            ::WaitForSingleObject(process.hProcess, kProbeKillTimeoutMs);
        }

        ::CloseHandle(process.hThread);
        ::CloseHandle(process.hProcess);

        result.Trace = ReadAppendedText(tracePath, traceOffset);
        result.ErrorText = ReadAppendedText(errorPath, errorOffset);
        return result;
    }

    struct FrameThreadingScope
    {
        FrameThreading Threading;

        explicit FrameThreadingScope(FrameThreadMode mode) { Threading.Initialize(mode); }
        ~FrameThreadingScope() { Threading.Shutdown(); }

        FrameThreadingScope(const FrameThreadingScope&) = delete;
        FrameThreadingScope& operator=(const FrameThreadingScope&) = delete;
    };
}


TEST_CASE("contract-violations: a recoverable violation is counted and published as one Error [smoke]")
{
    const minEngine::ContractViolationCounters shared = minEngine::GetContractViolationCounters();
    REQUIRE_MESSAGE(
        shared.bProcessWide,
        "the contract counters are not shared with the engine module; the frame summary would read zero");

    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();

    Recorder().Begin();
    ViolateOnceAtOneSite();
    const std::uint32_t published = minEngine::DrainContractViolations();
    Recorder().End();

    const minEngine::ContractViolationCounters after = minEngine::GetContractViolationCounters();
    CHECK_MESSAGE(after.RecordedOnContinue == 1, "counted ", after.RecordedOnContinue, " violations, not 1");
    CHECK_MESSAGE(after.RecordedOnTrap == 0, "a recoverable violation was counted as a trap");
    CHECK_MESSAGE(after.Suppressed == 0, "the first report of a site was already suppressed");
    CHECK_MESSAGE(published == 1, "published ", published, " records, not 1");

    const std::vector<std::string> messages = Recorder().Messages();
    const std::vector<minEngine::LogSeverity> severities = Recorder().Severities();
    REQUIRE_MESSAGE(messages.size() == 1, "the log received ", messages.size(), " records, not 1");
    CHECK_MESSAGE(severities[0] == minEngine::LogSeverity::Error, "the violation was not logged as an Error");

    // The record has to answer where, who and which role on its own: a violation without them sends
    // the reader back to guessing which thread broke which contract.
    CHECK_MESSAGE(Contains(messages[0], "ContractViolationsTest.cpp"), messages[0].c_str());
    CHECK_MESSAGE(Contains(messages[0], "thread "), messages[0].c_str());
    CHECK_MESSAGE(Contains(messages[0], "role "), messages[0].c_str());
    CHECK_MESSAGE(Contains(messages[0], "one recoverable violation"), messages[0].c_str());
}

TEST_CASE("contract-violations: a site reports a bounded number of times and keeps counting [smoke]")
{
    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();

    Recorder().Begin();
    constexpr int kAttempts = 20;
    for (int attempt = 0; attempt < kAttempts; ++attempt)
    {
        ViolateRepeatedlyAtOneSite();
    }
    const std::uint32_t published = minEngine::DrainContractViolations();
    Recorder().End();

    const minEngine::ContractViolationCounters after = minEngine::GetContractViolationCounters();
    CHECK_MESSAGE(
        after.RecordedOnContinue == static_cast<std::uint64_t>(kAttempts),
        "counted ", after.RecordedOnContinue, " of ", kAttempts, " violations");
    // One budget's worth of full reports, one line announcing the budget is spent, and every other
    // failure counted but not written: N failures must not become N lines.
    CHECK_MESSAGE(
        published == minEngine::kContractVerboseReportsPerViolation + 1,
        "one site wrote ", published, " lines for ", kAttempts, " failures");
    CHECK_MESSAGE(
        after.Suppressed
            == static_cast<std::uint64_t>(kAttempts - static_cast<int>(minEngine::kContractVerboseReportsPerViolation)),
        "counted ", after.Suppressed, " suppressed, published ", published, " of ", kAttempts);

    const std::size_t records = Recorder().Messages().size();
    CHECK_MESSAGE(records == published, "the log received ", records, " records, not ", published);
}

TEST_CASE("contract-violations: a role mismatch on a frame thread is recorded and the role runs on [smoke]")
{
    FrameThreadingScope scope(FrameThreadMode::Multi);
    FrameThreading& threading = scope.Threading;

    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();

    TaskSystem tasks;
    tasks.Init(2);

    std::atomic<bool> checked{false};
    std::atomic<bool> mismatchRecorded{false};

    // The violation is raised inside the engine: a frame role claims the wrong identity, which is the
    // case a reader meets first when a thread contract breaks. The engine module records it and this
    // module has to be able to read the same number.
    tasks.StartFrameRoles(
        [&](float) {
            const minEngine::ContractViolationCounters before = minEngine::GetContractViolationCounters();
            threading.AssertIsFrameThread(FrameThreadKind::Game, "contract-violations role check");
            const minEngine::ContractViolationCounters after = minEngine::GetContractViolationCounters();
            mismatchRecorded.store(
                after.RecordedOnContinue > before.RecordedOnContinue, std::memory_order_relaxed);
            checked.store(true, std::memory_order_relaxed);
        },
        [&]() {},
        /*combineGraphicsThreads=*/true);

    tasks.SubmitRenderFrameTask(0.0f, /*waitForCompletion=*/true);
    tasks.StopFrameRoles();
    tasks.Shutdown();

    CHECK(checked.load(std::memory_order_relaxed));
    CHECK(mismatchRecorded.load(std::memory_order_relaxed));

    const std::uint32_t published = minEngine::DrainContractViolations();
    const minEngine::ContractViolationCounters after = minEngine::GetContractViolationCounters();
    CHECK_MESSAGE(after.RecordedOnContinue == 1, "counted ", after.RecordedOnContinue, " violations, not 1");
    CHECK_MESSAGE(after.RecordedOnTrap == 0, "a role mismatch was counted as a trap");
    CHECK_MESSAGE(published == 1, "published ", published, " records, not 1");
}

TEST_CASE("contract-violations: the frame summary reports the contract count [smoke]")
{
    minEngine::FrameWorkLossCounters previous;
    minEngine::FrameWorkLossCounters current;
    current.ContractViolations = 2;

    const std::string line = minEngine::DescribeFrameWorkLossDelta(previous, current);
    CHECK_MESSAGE(Contains(line, "2 contract violation(s) recorded"), line.c_str());

    // A frame that broke no contract stays silent, which is what makes the line worth reading.
    CHECK(minEngine::DescribeFrameWorkLossDelta(current, current).empty());
    CHECK(minEngine::FrameWorkLossCounters{} != current);
}

/**
 * The editor drives its own frame loop - TickLogicalFrame / TickRendererFrame /
 * CompleteFrameReleases - and never calls Engine::Run, so a diagnostic that only Run reached was
 * reported by nothing at all in the editor: the records went to standard error and the log stayed
 * empty. This case drives the editor's frame boundary, in the editor's order and without Run, and
 * reads the log afterwards.
 */
TEST_CASE("contract-violations: the editor's frame boundary publishes the violation without Run [smoke]")
{
    // A host that owns no task pool and never starts Run: the same shape as Editor::Run, which
    // drives every frame itself.
    minEngine::Engine engine;

    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();

    // One frame with nothing staged: the work-loss line reports a delta, so the baseline has to be
    // this frame's counters rather than whatever the previous case left behind.
    engine.CompleteFrameReleases();

    Recorder().Begin();
    ViolateAtTheEditorBoundary();
    engine.CompleteFrameReleases();
    Recorder().End();

    const std::vector<std::string> messages = Recorder().Messages();
    REQUIRE_MESSAGE(!messages.empty(), "the editor's frame boundary published nothing to the log");

    // The staged record, with the file, thread and role it carries: the same text the failing
    // thread wrote to standard error.
    CHECK_MESSAGE(
        CountMessagesContaining(messages, "ContractViolationsTest.cpp") == 1,
        "the log received ", messages.size(), " record(s) and none names the failing site");
    CHECK_MESSAGE(
        CountMessagesContaining(messages, "a violation raised between two editor frames") == 1,
        "the log did not carry the violation message");

    // And the frame summary, which no editor frame printed before this boundary owned it.
    CHECK_MESSAGE(
        CountMessagesContaining(messages, "contract violation(s) recorded") == 1,
        "the frame summary never named the contract count");
}

/**
 * The release boundary is the frame's reporting point, so a host that reaches it twice in one frame
 * - or a future path that ends up calling both it and the release check - must not publish the same
 * record twice, and must not print the same summary twice.
 */
TEST_CASE("contract-violations: a repeated frame boundary publishes nothing twice [smoke]")
{
    minEngine::Engine engine;

    minEngine::ResetContractViolationCounters();
    (void)minEngine::DrainContractViolations();
    engine.CompleteFrameReleases();

    Recorder().Begin();
    ViolateAtTheSecondBoundary();
    engine.CompleteFrameReleases();
    const std::size_t afterFirst = Recorder().Messages().size();
    engine.CompleteFrameReleases();
    const std::size_t afterSecond = Recorder().Messages().size();
    Recorder().End();

    REQUIRE_MESSAGE(afterFirst > 0, "the first boundary published nothing");
    CHECK_MESSAGE(
        afterSecond == afterFirst,
        "a second boundary in the same frame added ", afterSecond - afterFirst, " record(s)");
}

TEST_CASE("contract-violations: a fatal assert still stops the process")
{
    const ProbeResult run = RunProbe("fatal-assert");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");
    REQUIRE_MESSAGE(run.Stopped, "the probe never stopped");

    // Every check below keeps its diagnostic in a named string: doctest streams a message only when
    // the check fails, which is after a temporary would already be gone.
    const std::string exitNote = Describe(run, "the probe exited cleanly after a fatal assert");
    CHECK_MESSAGE(run.ExitCode != 0, exitNote.c_str());

    const std::string reachedNote = Describe(run, "the probe never reached the check");
    CHECK_MESSAGE(Contains(run.Trace, "before-fatal-assert"), reachedNote.c_str());

    const std::string ranOnNote = Describe(run, "execution ran on past a fatal assert");
    CHECK_MESSAGE(!Contains(run.Trace, "after-fatal-assert"), ranOnNote.c_str());

    // The record is written before the trap: a process that dies must still have said where.
    const std::string reportNote = Describe(run, "no assert report");
    CHECK_MESSAGE(Contains(run.ErrorText, "ME_ASSERT contract violation at"), reportNote.c_str());

    const std::string roleNote = Describe(run, "the report names no frame role");
    CHECK_MESSAGE(Contains(run.ErrorText, "role "), roleNote.c_str());
}

TEST_CASE("contract-violations: the release grade records a failed assert and continues")
{
    const ProbeResult run = RunProbe("release-assert");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");
    REQUIRE_MESSAGE(run.Stopped, "the probe never stopped; the release grade trapped");

    const std::string exitNote = Describe(run, "the release grade ended the process");
    CHECK_MESSAGE(run.ExitCode == 0, exitNote.c_str());

    const std::string continuedNote = Describe(run, "execution did not continue past a release-grade assert");
    CHECK_MESSAGE(Contains(run.Trace, "survived-release-assert"), continuedNote.c_str());

    // The grade stays in the record: a reader has to see that this assert was meant to be fatal.
    const std::string gradeNote = Describe(run, "the release grade recorded nothing, or lost the grade");
    CHECK_MESSAGE(Contains(run.ErrorText, "ME_ASSERT (this build records instead of trapping)"), gradeNote.c_str());

    const std::string trapNote = Describe(run, "the release grade counted the failure as a trap");
    CHECK_MESSAGE(Contains(run.Trace, "trapped=0"), trapNote.c_str());

    const std::string countNote = Describe(run, "the release grade did not count the failure");
    CHECK_MESSAGE(Contains(run.Trace, "continued=1"), countNote.c_str());

    const std::string publishNote = Describe(run, "the violation never reached the log");
    CHECK_MESSAGE(Contains(run.Trace, "published=1"), publishNote.c_str());

    const std::string roleNote = Describe(run, "the report names no frame role");
    CHECK_MESSAGE(Contains(run.ErrorText, "role Game"), roleNote.c_str());
}

TEST_CASE("contract-violations: a violation raised while the log lock is held does not deadlock")
{
    // The log system calls every sink while holding its mutex, so the violation below is raised with
    // that mutex held by the violating thread. A report path that asked for it again would wait for
    // itself, which is why this case is bounded by the probe timeout instead of failing.
    const ProbeResult run = RunProbe("log-lock-held");
    REQUIRE_MESSAGE(run.Started, "the probe next to minEngineTests.exe did not start");
    REQUIRE_MESSAGE(run.Stopped, "the probe blocked: the report path re-entered the log lock");

    const std::string exitNote = Describe(run, "the probe failed");
    CHECK_MESSAGE(run.ExitCode == 0, exitNote.c_str());

    const std::string sinkNote = Describe(run, "the sink never returned");
    CHECK_MESSAGE(Contains(run.Trace, "sink-returned"), sinkNote.c_str());

    const std::string finishNote = Describe(run, "the probe never finished");
    CHECK_MESSAGE(Contains(run.Trace, "survived-log-lock"), finishNote.c_str());

    const std::string recordNote = Describe(run, "the violation never reached the log");
    CHECK_MESSAGE(Contains(run.Trace, "sink-saw-contract-violation"), recordNote.c_str());

    const std::string publishNote = Describe(run, "no record was published");
    CHECK_MESSAGE(Contains(run.Trace, "published=1"), publishNote.c_str());
}


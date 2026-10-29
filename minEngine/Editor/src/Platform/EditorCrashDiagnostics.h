#pragma once

#include <string>

namespace minEngine
{
    /**
     * Installs the crash filter and returns the record path it appends to (empty on a platform
     * without it). Everything the fault path needs - the record storage, the file paths and a
     * thread with a stack of its own - is reserved here, because none of it can be created once a
     * thread has faulted.
     */
    std::string InstallEditorCrashDiagnostics();

    /** One run of the crash-record path. Counters are process-wide, since the guard is too. */
    struct CrashRecordSelfCheckResult
    {
        bool RecordWritten = false;
        int Admitted = 0;
        int Rejected = 0;
        std::string RecordPath;
    };

    /**
     * Runs the fault path once as if the calling thread had faulted, from a caller that is still
     * alive, so a test can prove the record is written while some other lock is held.
     *
     * provokeReentry raises a second fault from inside the record path, which the once guard has to
     * turn away. The guard stays spent afterwards, exactly as after a real fault: a process records
     * one crash.
     */
    CrashRecordSelfCheckResult RunCrashRecordSelfCheck(bool provokeReentry);
}

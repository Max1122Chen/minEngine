#pragma once

#include "EngineAPI.h"
#include "LogRecord.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace minEngine
{
    class MINENGINE_API LogConsoleStorage
    {
    public:
        /** Copies once into the ring slot. Prefer this over Snapshot on hot UI paths. */
        static void Push(const LogRecord& record);
        /** Full chronological copy — prefer CopyInto / GetGeneration for hot UI paths. */
        static std::vector<LogRecord> Snapshot();
        static void CopyInto(std::vector<LogRecord>& out);
        static uint64_t GetGeneration();
        static void Clear();
        /** True after the ring has overwritten an older record since the last Clear. */
        static bool IsTruncated();
        /** Chronological tail of records at or above minSeverity, at most limit entries. */
        static std::vector<LogRecord> Tail(size_t limit, LogSeverity minSeverity);

    private:
        static void AppendChronologicalUnlocked(std::vector<LogRecord>& out);

        static constexpr size_t kMaxEntries = 2000;
        static std::mutex s_Mutex;
        static std::vector<LogRecord> s_Ring;
        static size_t s_Begin;
        static size_t s_Count;
        static uint64_t s_Generation;
        static bool s_Truncated;
    };

    class LogConsoleSink final : public ILogSink
    {
    public:
        void Write(const LogRecord& record) override;
        void Flush() override {}
    };
}

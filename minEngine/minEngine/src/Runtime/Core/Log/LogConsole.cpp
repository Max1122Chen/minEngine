#include "LogConsole.h"

namespace minEngine
{
    std::mutex LogConsoleStorage::s_Mutex;
    std::vector<LogRecord> LogConsoleStorage::s_Ring;
    size_t LogConsoleStorage::s_Begin = 0;
    size_t LogConsoleStorage::s_Count = 0;
    uint64_t LogConsoleStorage::s_Generation = 0;
    bool LogConsoleStorage::s_Truncated = false;

    void LogConsoleStorage::AppendChronologicalUnlocked(std::vector<LogRecord>& out)
    {
        out.resize(s_Count);
        for (size_t i = 0; i < s_Count; ++i)
        {
            out[i] = s_Ring[(s_Begin + i) % kMaxEntries];
        }
    }

    void LogConsoleStorage::Push(const LogRecord& record)
    {
        LogRecord stored = record;
        stored.EnsureLocalDisplayTime();

        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Ring.size() < kMaxEntries)
        {
            s_Ring.resize(kMaxEntries);
        }

        if (s_Count < kMaxEntries)
        {
            const size_t writeIndex = (s_Begin + s_Count) % kMaxEntries;
            s_Ring[writeIndex] = std::move(stored);
            ++s_Count;
        }
        else
        {
            s_Ring[s_Begin] = std::move(stored);
            s_Begin = (s_Begin + 1) % kMaxEntries;
            s_Truncated = true;
        }
        ++s_Generation;
    }

    std::vector<LogRecord> LogConsoleStorage::Snapshot()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        std::vector<LogRecord> copy;
        AppendChronologicalUnlocked(copy);
        return copy;
    }

    void LogConsoleStorage::CopyInto(std::vector<LogRecord>& out)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        AppendChronologicalUnlocked(out);
    }

    uint64_t LogConsoleStorage::GetGeneration()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Generation;
    }

    void LogConsoleStorage::Clear()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Begin = 0;
        s_Count = 0;
        s_Truncated = false;
        ++s_Generation;
    }

    bool LogConsoleStorage::IsTruncated()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Truncated;
    }

    std::vector<LogRecord> LogConsoleStorage::Tail(size_t limit, LogSeverity minSeverity)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        std::vector<LogRecord> chronological;
        AppendChronologicalUnlocked(chronological);

        std::vector<LogRecord> filtered;
        filtered.reserve(chronological.size());
        for (const LogRecord& record : chronological)
        {
            if (record.severity >= minSeverity)
            {
                filtered.push_back(record);
            }
        }

        if (limit < filtered.size())
        {
            filtered.erase(filtered.begin(), filtered.end() - static_cast<std::ptrdiff_t>(limit));
        }
        return filtered;
    }

    void LogConsoleSink::Write(const LogRecord& record)
    {
        LogConsoleStorage::Push(record);
    }
}

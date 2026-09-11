#include "LogConsole.h"

namespace minEngine
{
    std::mutex LogConsoleStorage::s_Mutex;
    std::vector<LogRecord> LogConsoleStorage::s_Entries;

    void LogConsoleStorage::Push(LogRecord record)
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        if (s_Entries.size() >= kMaxEntries)
        {
            s_Entries.erase(s_Entries.begin());
        }
        s_Entries.emplace_back(std::move(record));
    }

    std::vector<LogRecord> LogConsoleStorage::Snapshot()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        return s_Entries;
    }

    void LogConsoleStorage::Clear()
    {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Entries.clear();
    }

    void LogConsoleSink::Write(const LogRecord& record)
    {
        LogConsoleStorage::Push(record);
    }
}

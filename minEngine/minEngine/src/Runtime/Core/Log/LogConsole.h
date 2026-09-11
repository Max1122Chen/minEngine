#pragma once

#include "LogRecord.h"

#include <mutex>
#include <vector>

namespace minEngine
{
    class MINENGINE_API LogConsoleStorage
    {
    public:
        static void Push(LogRecord record);
        static std::vector<LogRecord> Snapshot();
        static void Clear();

    private:
        static constexpr size_t kMaxEntries = 2000;
        static std::mutex s_Mutex;
        static std::vector<LogRecord> s_Entries;
    };

    class LogConsoleSink final : public ILogSink
    {
    public:
        void Write(const LogRecord& record) override;
        void Flush() override {}
    };
}

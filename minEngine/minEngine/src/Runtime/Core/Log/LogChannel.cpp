#include "LogChannel.h"

#include "LogSystem.h"

namespace minEngine
{
    LogChannelBase::LogChannelBase(
        const char* name,
        LogSeverity defaultSeverity,
        LogSeverity compileTimeMaxSeverity)
        : m_Name(name != nullptr ? name : "")
        , m_RuntimeSeverity(defaultSeverity)
        , m_CompileTimeMaxSeverity(compileTimeMaxSeverity)
    {
        if (static_cast<uint8_t>(m_RuntimeSeverity.load(std::memory_order_relaxed)) <
            static_cast<uint8_t>(m_CompileTimeMaxSeverity))
        {
            m_RuntimeSeverity.store(m_CompileTimeMaxSeverity, std::memory_order_relaxed);
        }
        LogSystem::RegisterChannel(*this);
    }

    LogChannelBase::~LogChannelBase()
    {
        LogSystem::UnregisterChannel(*this);
    }

    void LogChannelBase::SetSeverity(LogSeverity severity)
    {
        if (static_cast<uint8_t>(severity) < static_cast<uint8_t>(m_CompileTimeMaxSeverity))
        {
            m_RuntimeSeverity.store(m_CompileTimeMaxSeverity, std::memory_order_relaxed);
            return;
        }
        m_RuntimeSeverity.store(severity, std::memory_order_relaxed);
    }

    bool LogChannelBase::IsSuppressed(LogSeverity messageSeverity) const
    {
        return static_cast<uint8_t>(messageSeverity) <
            static_cast<uint8_t>(m_RuntimeSeverity.load(std::memory_order_relaxed));
    }

    const char* LogChannelBase::SeverityToString(LogSeverity severity)
    {
        switch (severity)
        {
            case LogSeverity::Trace: return "Trace";
            case LogSeverity::Debug: return "Debug";
            case LogSeverity::Info: return "Info";
            case LogSeverity::Warn: return "Warn";
            case LogSeverity::Error: return "Error";
            case LogSeverity::Fatal: return "Fatal";
            default: return "Unknown";
        }
    }
}

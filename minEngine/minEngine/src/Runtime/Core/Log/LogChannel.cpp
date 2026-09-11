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
        if (static_cast<uint8_t>(m_RuntimeSeverity) < static_cast<uint8_t>(m_CompileTimeMaxSeverity))
        {
            m_RuntimeSeverity = m_CompileTimeMaxSeverity;
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
            m_RuntimeSeverity = m_CompileTimeMaxSeverity;
            return;
        }
        m_RuntimeSeverity = severity;
    }

    bool LogChannelBase::IsSuppressed(LogSeverity messageSeverity) const
    {
        return static_cast<uint8_t>(messageSeverity) < static_cast<uint8_t>(m_RuntimeSeverity);
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

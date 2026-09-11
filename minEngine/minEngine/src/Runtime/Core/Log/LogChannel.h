#pragma once

#include "EngineAPI.h"

#include <cstdint>

namespace minEngine
{
    enum class LogSeverity : uint8_t
    {
        Trace = 0,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    class MINENGINE_API LogChannelBase
    {
    public:
        LogChannelBase(const char* name, LogSeverity defaultSeverity, LogSeverity compileTimeMaxSeverity);
        ~LogChannelBase();

        LogChannelBase(const LogChannelBase&) = delete;
        LogChannelBase& operator=(const LogChannelBase&) = delete;

        const char* GetName() const { return m_Name; }
        LogSeverity GetSeverity() const { return m_RuntimeSeverity; }
        LogSeverity GetCompileTimeMaxSeverity() const { return m_CompileTimeMaxSeverity; }

        void SetSeverity(LogSeverity severity);
        bool IsSuppressed(LogSeverity messageSeverity) const;

        static const char* SeverityToString(LogSeverity severity);

    private:
        const char* m_Name = "";
        LogSeverity m_RuntimeSeverity = LogSeverity::Info;
        LogSeverity m_CompileTimeMaxSeverity = LogSeverity::Trace;
    };

    template <LogSeverity DefaultSeverity, LogSeverity CompileTimeMaxSeverity>
    class TLogChannel : public LogChannelBase
    {
    public:
        explicit TLogChannel(const char* name)
            : LogChannelBase(name, DefaultSeverity, CompileTimeMaxSeverity)
        {
        }
    };
}

// ChannelObj lives in ::minEngine (use inside that namespace in .cpp DEFINE).
#define ME_DECLARE_LOG_CHANNEL_EXTERN(ChannelObj, DefaultSev, CompileMax)                          \
    class MINENGINE_API FLogChannel_##ChannelObj final                                             \
        : public ::minEngine::TLogChannel<::minEngine::LogSeverity::DefaultSev,                     \
                                          ::minEngine::LogSeverity::CompileMax>                    \
    {                                                                                              \
    public:                                                                                        \
        FLogChannel_##ChannelObj();                                                                \
    };                                                                                             \
    extern MINENGINE_API FLogChannel_##ChannelObj ChannelObj

#define ME_DEFINE_LOG_CHANNEL(ChannelObj, NameLiteral, DefaultSev, CompileMax)                     \
    FLogChannel_##ChannelObj::FLogChannel_##ChannelObj()                                           \
        : ::minEngine::TLogChannel<::minEngine::LogSeverity::DefaultSev,                            \
                                   ::minEngine::LogSeverity::CompileMax>(NameLiteral)              \
    {                                                                                              \
    }                                                                                              \
    FLogChannel_##ChannelObj ChannelObj

namespace minEngine
{
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogCore, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogApp, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogPlatform, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogRender, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogRHI, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogPhysics, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogAsset, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogSerialization, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogAnimation, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogUI, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogAudio, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogScript, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogEditor, Info, Trace);
    ME_DECLARE_LOG_CHANNEL_EXTERN(LogTest, Info, Trace);
}

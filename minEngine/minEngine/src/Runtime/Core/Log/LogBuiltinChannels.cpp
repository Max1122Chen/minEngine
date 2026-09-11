#include "LogChannel.h"

namespace minEngine
{
    ME_DEFINE_LOG_CHANNEL(LogCore, "Core", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogApp, "App", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogPlatform, "Platform", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogRender, "Render", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogRHI, "RHI", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogPhysics, "Physics", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogAsset, "Asset", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogSerialization, "Serialization", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogAnimation, "Animation", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogUI, "UI", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogAudio, "Audio", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogScript, "Script", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogEditor, "Editor", Info, Trace);
    ME_DEFINE_LOG_CHANNEL(LogTest, "Test", Info, Trace);
}

#pragma once

#include "Runtime/Core/Profiling/ProfileMacros.h"
#include "Runtime/Core/Profiling/ProfileTypes.h"

#include "EngineAPI.h"

#include <filesystem>

namespace minEngine::Profile
{
    MINENGINE_API void Initialize();
    MINENGINE_API void Shutdown();

    MINENGINE_API void SetEnabled(bool enabled);
    MINENGINE_API bool IsEnabled();
    MINENGINE_API bool IsCompileEnabled();

    MINENGINE_API void SetAutoEngineSessionEnabled(bool enabled);
    MINENGINE_API bool IsAutoEngineSessionEnabled();

    MINENGINE_API ProfileSessionId StartSession(const ProfileSessionDesc& desc = {});
    MINENGINE_API void StopSession();
    MINENGINE_API bool IsSessionActive();
    MINENGINE_API ProfileSessionId GetActiveSessionId();
    MINENGINE_API const ProfileSession* GetActiveSession();
    MINENGINE_API const ProfileSession* GetLastCompletedSession();

    MINENGINE_API uint64_t GetDroppedEventCount();
    MINENGINE_API uint64_t GetNoopHitCount();

    MINENGINE_API void BeginPhase(const char* staticName);
    MINENGINE_API void EndPhase();
    MINENGINE_API void BeginFrame();
    MINENGINE_API void EndFrame();

    MINENGINE_API bool AnalyzeSession(ProfileSession& session);

    MINENGINE_API bool ExportChromeTrace(
        const ProfileSession& session,
        const std::filesystem::path& path,
        const ProfileExportDesc& desc = {});

    MINENGINE_API bool ExportStatsText(
        const ProfileSession& session,
        const std::filesystem::path& path);

    MINENGINE_API const ProfileStatsSummary* GetStatsSummary(const ProfileSession& session);

    inline bool SpanMatchesFilter(const ProfileSpan& span, const ProfileSpanFilter& filter)
    {
        if (filter.ConstrainPhase && span.Phase != filter.Phase)
        {
            return false;
        }
        if (filter.ConstrainFrame && span.Frame != filter.Frame)
        {
            return false;
        }
        if (filter.ConstrainThread && span.Thread != filter.Thread)
        {
            return false;
        }
        if (filter.ConstrainName && span.Name != filter.Name)
        {
            return false;
        }
        return true;
    }

    template <typename TFn>
    void ForEachSpan(const ProfileSession& session, const ProfileSpanFilter& filter, TFn&& fn)
    {
        for (const ProfileSpan& span : session.Spans)
        {
            if (SpanMatchesFilter(span, filter))
            {
                fn(span);
            }
        }
    }

    MINENGINE_API const ProfileNameAggregate* FindNameAggregate(
        const ProfileSession& session,
        ProfileNameId name);

    MINENGINE_API const ProfileNameAggregate* FindNameAggregate(
        const ProfileSession& session,
        const char* staticName);

    MINENGINE_API const ProfilePhaseRecord* FindPhase(
        const ProfileSession& session,
        const char* staticName);

    MINENGINE_API const ProfileFrameRecord* FindFrame(
        const ProfileSession& session,
        ProfileFrameIndex index);

    MINENGINE_API size_t GetSpanCount(const ProfileSession& session);
    MINENGINE_API const ProfileSpan* GetSpan(const ProfileSession& session, size_t index);

    MINENGINE_API const char* GetNameText(ProfileNameId id);
    MINENGINE_API ProfileNameId RegisterName(const char* staticLiteral);

    MINENGINE_API void Detail_BeginScope(ProfileNameId name);
    MINENGINE_API void Detail_EndScope(ProfileNameId name);
}

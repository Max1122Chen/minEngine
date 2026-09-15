#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace minEngine::Profile
{
    using ProfileTimestamp = uint64_t;
    using ProfileNameId = uint32_t;
    using ProfileThreadId = uint32_t;
    using ProfilePhaseId = uint32_t;
    using ProfileSessionId = uint64_t;
    using ProfileFrameIndex = uint32_t;

    constexpr ProfileFrameIndex kInvalidFrameIndex = UINT32_MAX;
    constexpr ProfileNameId kInvalidNameId = 0;
    constexpr ProfilePhaseId kInvalidPhaseId = 0;

    enum class ProfileEventType : uint8_t
    {
        ScopeBegin = 0,
        ScopeEnd = 1,
        Instant = 2,
        Counter = 3,
        PhaseBegin = 4,
        PhaseEnd = 5,
        FrameBegin = 6,
        FrameEnd = 7,
    };

    enum class ProfileBufferOverflowPolicy : uint8_t
    {
        GrowChunks = 0,
        DropNewest = 1,
    };

    enum class ProfileSessionState : uint8_t
    {
        Idle = 0,
        Recording = 1,
        Analyzing = 2,
        Completed = 3,
    };

    struct ProfileEvent
    {
        ProfileEventType Type = ProfileEventType::ScopeBegin;
        uint8_t Depth = 0;
        uint16_t Pad = 0;
        ProfileNameId Name = kInvalidNameId;
        ProfileThreadId Thread = 0;
        ProfilePhaseId Phase = kInvalidPhaseId;
        ProfileFrameIndex Frame = kInvalidFrameIndex;
        ProfileTimestamp Timestamp = 0;
    };

    static_assert(sizeof(ProfileEvent) <= 32, "ProfileEvent must stay compact for hot path");

    struct ProfileSessionDesc
    {
        const char* Label = "default";
        uint32_t MaxEventCount = 2'000'000;
        ProfileBufferOverflowPolicy OverflowPolicy = ProfileBufferOverflowPolicy::GrowChunks;
        bool CaptureThreadNames = true;
    };

    struct ProfilePhaseRecord
    {
        ProfilePhaseId Id = kInvalidPhaseId;
        ProfileNameId Name = kInvalidNameId;
        ProfileTimestamp Start = 0;
        ProfileTimestamp End = 0;
        bool Open = false;
    };

    struct ProfileFrameRecord
    {
        ProfileFrameIndex Index = 0;
        ProfilePhaseId Phase = kInvalidPhaseId;
        ProfileTimestamp Start = 0;
        ProfileTimestamp End = 0;
    };

    struct ProfileThreadInfo
    {
        ProfileThreadId Id = 0;
        uint64_t OsThreadId = 0;
        std::string Name;
    };

    struct ProfileSessionStats
    {
        uint64_t EventsRecorded = 0;
        uint64_t EventsDropped = 0;
        uint64_t ChunksAllocated = 0;
        uint32_t MaxScopeDepth = 0;
        uint32_t UnbalancedScopes = 0;
        uint64_t NoopHits = 0;
    };

    struct ProfileSpan
    {
        ProfileNameId Name = kInvalidNameId;
        ProfileThreadId Thread = 0;
        ProfilePhaseId Phase = kInvalidPhaseId;
        ProfileFrameIndex Frame = kInvalidFrameIndex;
        ProfileTimestamp Start = 0;
        ProfileTimestamp End = 0;
        ProfileTimestamp Inclusive = 0;
        ProfileTimestamp Exclusive = 0;
        uint32_t Depth = 0;
        int32_t ParentSpanIndex = -1;
        uint32_t ChildCount = 0;
    };

    struct ProfileNameAggregate
    {
        ProfileNameId Name = kInvalidNameId;
        uint32_t CallCount = 0;
        ProfileTimestamp TotalInclusive = 0;
        ProfileTimestamp TotalExclusive = 0;
        ProfileTimestamp MinInclusive = 0;
        ProfileTimestamp MaxInclusive = 0;

        double AverageInclusiveMs() const;
    };

    struct ProfileStatsSummary
    {
        std::vector<ProfileNameAggregate> ByName;
        ProfileTimestamp SessionDuration = 0;
        uint32_t FrameCount = 0;
        ProfileTimestamp AvgFrameDuration = 0;
        ProfileTimestamp MinFrameDuration = 0;
        ProfileTimestamp MaxFrameDuration = 0;
    };

    struct ProfileSession
    {
        ProfileSessionId Id = 0;
        ProfileSessionState State = ProfileSessionState::Idle;
        ProfileSessionDesc Desc{};
        std::string Label;
        ProfileTimestamp SessionStart = 0;
        ProfileTimestamp SessionEnd = 0;

        std::vector<ProfileThreadInfo> Threads;
        std::vector<ProfilePhaseRecord> Phases;
        std::vector<ProfileFrameRecord> Frames;
        std::vector<ProfileEvent> Events;

        ProfileSessionStats Stats{};
        std::vector<ProfileSpan> Spans;
        ProfileStatsSummary StatsSummary{};

        uint32_t SchemaVersion = 1;
    };

    struct ProfileSpanFilter
    {
        bool ConstrainPhase = false;
        ProfilePhaseId Phase = kInvalidPhaseId;
        bool ConstrainFrame = false;
        ProfileFrameIndex Frame = kInvalidFrameIndex;
        bool ConstrainThread = false;
        ProfileThreadId Thread = 0;
        bool ConstrainName = false;
        ProfileNameId Name = kInvalidNameId;
    };

    struct ProfileExportDesc
    {
        bool PrettyPrint = false;
        bool IncludeStatsSummaryAsMetadata = true;
    };
}

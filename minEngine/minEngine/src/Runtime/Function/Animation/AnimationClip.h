#pragma once

#include "Core.h"
#include "Runtime/Function/Animation/AnimationTrack.h"
#include "Runtime/Function/Animation/Pose.h"
#include "Runtime/Resource/Asset.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class Skeleton;

    ME_CLASS()
    class AnimationClip : public Asset
    {
        ME_GENERATED_BODY()
    public:
        AnimationClip() = default;
        ~AnimationClip() override = default;

        float GetDuration() const { return m_Duration; }
        void SetDuration(float durationSeconds) { m_Duration = durationSeconds; }

        Skeleton* GetSkeleton() const { return m_Skeleton.get(); }
        void SetSkeleton(const std::shared_ptr<Skeleton>& skeleton) { m_Skeleton = skeleton; }

        const std::vector<AnimationTrack>& GetTracks() const { return m_Tracks; }
        void SetTracks(std::vector<AnimationTrack> tracks) { m_Tracks = std::move(tracks); }

        const std::vector<AnimationNamedFloatTrack>& GetNamedFloatTracks() const
        {
            return m_NamedFloatTracks;
        }
        void SetNamedFloatTracks(std::vector<AnimationNamedFloatTrack> tracks)
        {
            m_NamedFloatTracks = std::move(tracks);
        }

        // Writes local pose. Starts from skeleton bind; overlays keyed components.
        void Evaluate(float timeSeconds, Pose& outPose) const;

        // Samples named float tracks only — never mutates Component properties.
        bool TryGetNamedFloat(std::string_view name, float timeSeconds, float& outValue) const;

    private:
        static float SampleScalarKeys(
            const std::vector<AnimationFloatKey>& keys,
            float timeSeconds,
            bool& outFound);
        static Vector3 SampleVec3Keys(
            const std::vector<AnimationVec3Key>& keys,
            float timeSeconds,
            bool& outFound);
        static Quaternion SampleQuatKeys(
            const std::vector<AnimationQuatKey>& keys,
            float timeSeconds,
            bool& outFound);

        ME_PROPERTY()
        std::shared_ptr<Skeleton> m_Skeleton;

        ME_PROPERTY()
        float m_Duration = 0.0f;

        ME_PROPERTY()
        std::vector<AnimationTrack> m_Tracks;

        ME_PROPERTY()
        std::vector<AnimationNamedFloatTrack> m_NamedFloatTracks;
    };
}

#include "Generated/Reflection/AnimationClip.gen.h"

#include "Runtime/Function/Animation/AnimationClip.h"

#include "Runtime/Function/Animation/Skeleton.h"

#include <algorithm>
#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>

namespace minEngine
{
    namespace
    {
        template <typename KeyT, typename GetValueFn, typename LerpFn>
        bool SampleKeys(
            const std::vector<KeyT>& keys,
            float timeSeconds,
            GetValueFn getValue,
            LerpFn lerp,
            decltype(getValue(keys[0]))& outValue)
        {
            if (keys.empty())
            {
                return false;
            }

            if (timeSeconds <= keys.front().Time)
            {
                outValue = getValue(keys.front());
                return true;
            }

            if (timeSeconds >= keys.back().Time)
            {
                outValue = getValue(keys.back());
                return true;
            }

            size_t upper = 1;
            while (upper < keys.size() && keys[upper].Time < timeSeconds)
            {
                ++upper;
            }

            const size_t lower = upper - 1;
            const float span = keys[upper].Time - keys[lower].Time;
            const float alpha = (span > 1e-8f) ? ((timeSeconds - keys[lower].Time) / span) : 0.0f;
            outValue = lerp(getValue(keys[lower]), getValue(keys[upper]), alpha);
            return true;
        }
    }

    float AnimationClip::SampleScalarKeys(
        const std::vector<AnimationFloatKey>& keys,
        float timeSeconds,
        bool& outFound)
    {
        float value = 0.0f;
        outFound = SampleKeys(
            keys,
            timeSeconds,
            [](const AnimationFloatKey& key) { return key.Value; },
            [](float a, float b, float t) { return a + (b - a) * t; },
            value);
        return value;
    }

    Vector3 AnimationClip::SampleVec3Keys(
        const std::vector<AnimationVec3Key>& keys,
        float timeSeconds,
        bool& outFound)
    {
        Vector3 value(0.0f);
        outFound = SampleKeys(
            keys,
            timeSeconds,
            [](const AnimationVec3Key& key) { return key.Value; },
            [](const Vector3& a, const Vector3& b, float t) { return a + (b - a) * t; },
            value);
        return value;
    }

    Quaternion AnimationClip::SampleQuatKeys(
        const std::vector<AnimationQuatKey>& keys,
        float timeSeconds,
        bool& outFound)
    {
        Quaternion value = Quaternion::Identity();
        outFound = SampleKeys(
            keys,
            timeSeconds,
            [](const AnimationQuatKey& key) { return key.Value; },
            [](const Quaternion& a, const Quaternion& b, float t)
            {
                return Quaternion::FromGlm(glm::normalize(glm::slerp(a.ToGlm(), b.ToGlm(), t)));
            },
            value);
        return value;
    }

    void AnimationClip::Evaluate(float timeSeconds, Pose& outPose) const
    {
        Skeleton* skeleton = GetSkeleton();
        if (skeleton == nullptr || skeleton->GetBoneCount() <= 0)
        {
            outPose = {};
            return;
        }

        if (m_Duration <= 0.0f && m_Tracks.empty())
        {
            skeleton->FillBindPose(outPose);
            return;
        }

        skeleton->FillBindPose(outPose);

        const float sampleTime = std::max(timeSeconds, 0.0f);

        for (const AnimationTrack& track : m_Tracks)
        {
            if (track.BoneIndex < 0 || track.BoneIndex >= outPose.GetBoneCount())
            {
                continue;
            }

            Transform& local = outPose.At(track.BoneIndex);

            bool found = false;
            const Vector3 position = SampleVec3Keys(track.PositionKeys, sampleTime, found);
            if (found)
            {
                local.Position = position;
            }

            const Quaternion rotation = SampleQuatKeys(track.RotationKeys, sampleTime, found);
            if (found)
            {
                local.SetRotation(rotation);
            }

            const Vector3 scale = SampleVec3Keys(track.ScaleKeys, sampleTime, found);
            if (found)
            {
                local.Scale = scale;
            }
        }
    }

    bool AnimationClip::TryGetNamedFloat(
        std::string_view name,
        float timeSeconds,
        float& outValue) const
    {
        for (const AnimationNamedFloatTrack& track : m_NamedFloatTracks)
        {
            if (track.Name != name)
            {
                continue;
            }

            bool found = false;
            outValue = SampleScalarKeys(track.Keys, std::max(timeSeconds, 0.0f), found);
            return found;
        }
        return false;
    }
}

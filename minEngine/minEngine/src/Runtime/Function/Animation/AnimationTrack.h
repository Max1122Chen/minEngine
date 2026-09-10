#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"

#include <string>
#include <vector>

namespace minEngine
{
    ME_STRUCT()
    struct AnimationVec3Key
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        float Time = 0.0f;

        ME_PROPERTY()
        Vector3 Value{0.0f, 0.0f, 0.0f};
    };

    ME_STRUCT()
    struct AnimationQuatKey
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        float Time = 0.0f;

        ME_PROPERTY()
        Quaternion Value{};
    };

    ME_STRUCT()
    struct AnimationFloatKey
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        float Time = 0.0f;

        ME_PROPERTY()
        float Value = 0.0f;
    };

    // F02 MVP: bone transform track (transitional). Named float tracks are separate.
    ME_STRUCT()
    struct AnimationTrack
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        int32_t BoneIndex = -1;

        ME_PROPERTY()
        std::vector<AnimationVec3Key> PositionKeys;

        ME_PROPERTY()
        std::vector<AnimationQuatKey> RotationKeys;

        ME_PROPERTY()
        std::vector<AnimationVec3Key> ScaleKeys;
    };

    // Future: limited named float curves; consumers call TryGetNamedFloat (clip never writes properties).
    ME_STRUCT()
    struct AnimationNamedFloatTrack
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        std::string Name;

        ME_PROPERTY()
        std::vector<AnimationFloatKey> Keys;
    };
}

#include "Generated/Reflection/AnimationTrack.gen.h"

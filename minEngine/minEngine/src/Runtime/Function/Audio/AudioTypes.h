#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Audio/AudioLimits.h"

#include <cstdint>
#include <memory>
#include <string>

namespace minEngine
{
    class AudioClip;

    using AudioVoiceId = uint32_t;
    inline constexpr AudioVoiceId InvalidAudioVoiceId = UINT32_MAX;

    struct AudioVoiceHandle
    {
        AudioVoiceId Id{InvalidAudioVoiceId};

        bool IsValid() const { return Id != InvalidAudioVoiceId; }
        bool operator==(const AudioVoiceHandle& other) const { return Id == other.Id; }
        bool operator!=(const AudioVoiceHandle& other) const { return !(*this == other); }
    };

    ME_ENUM()
    enum class EAudioVoiceState : uint8_t
    {
        Stopped = 0,
        Playing,
        Paused,
    };

    ME_ENUM()
    enum class EAudioBusId : uint8_t
    {
        Master = 0,
        Music,
        SFX,
        UI,
        Count,
    };

    ME_ENUM()
    enum class EAudioAttenuationModel : uint8_t
    {
        Inverse = 0,
        Linear,
        None,
    };

    struct AudioSpatialSettings
    {
        bool bSpatialized{true};
        float MinDistance{kDefaultMinDistance};
        float MaxDistance{kDefaultMaxDistance};
        EAudioAttenuationModel AttenuationModel{EAudioAttenuationModel::Inverse};
    };

    struct AudioListenerState
    {
        Vector3 Position{};
        Vector3 Forward{0.0f, 0.0f, -1.0f};
        Vector3 Up{0.0f, 1.0f, 0.0f};
    };

    struct AudioPlayParams
    {
        std::shared_ptr<AudioClip> Clip;
        EAudioBusId Bus{EAudioBusId::SFX};
        float Volume{1.0f};
        float Pitch{1.0f};
        bool bLoop{false};
        AudioSpatialSettings Spatial{};
        Vector3 WorldPosition{};
        class AudioComponent* OwnerComponent{nullptr};
        class Scene* OwnerScene{nullptr};
    };

    struct AudioPlayResult
    {
        AudioVoiceHandle Voice{};
        bool bSuccess{false};
        std::string ErrorMessage;
    };
}

#include "Generated/Reflection/AudioTypes.gen.h"

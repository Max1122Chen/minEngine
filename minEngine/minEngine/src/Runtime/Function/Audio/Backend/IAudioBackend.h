#pragma once

#include "Runtime/Function/Audio/AudioTypes.h"
#include "Runtime/Function/Audio/Backend/AudioBackendTypes.h"

namespace minEngine
{
    class AudioClip;

    class IAudioBackend
    {
    public:
        virtual ~IAudioBackend() = default;

        virtual bool Initialize() = 0;
        virtual void Shutdown() = 0;
        virtual void Update() = 0;

        virtual BackendVoiceHandle CreateVoice(const AudioClip& clip) = 0;
        virtual void DestroyVoice(BackendVoiceHandle handle) = 0;
        virtual bool IsVoicePlaying(BackendVoiceHandle handle) const = 0;

        virtual void PlayVoice(BackendVoiceHandle handle, bool loop) = 0;
        virtual void StopVoice(BackendVoiceHandle handle) = 0;
        virtual void PauseVoice(BackendVoiceHandle handle) = 0;
        virtual void ResumeVoice(BackendVoiceHandle handle) = 0;

        virtual void SetVoiceVolume(BackendVoiceHandle handle, float linearGain) = 0;
        virtual void SetVoicePitch(BackendVoiceHandle handle, float pitch) = 0;

        virtual void SetListener(const AudioListenerState& listener) = 0;
        virtual void SetVoiceWorldPosition(BackendVoiceHandle handle, const Vector3& worldPosition) = 0;
        virtual void SetVoiceSpatialSettings(BackendVoiceHandle handle, const AudioSpatialSettings& settings) = 0;
        virtual void SetVoiceSpatializationEnabled(BackendVoiceHandle handle, bool enabled) = 0;
    };
}

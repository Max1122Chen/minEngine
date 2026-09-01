#pragma once

#include "Runtime/Function/Audio/Backend/IAudioBackend.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class MiniaudioBackend final : public IAudioBackend
    {
    public:
        MiniaudioBackend();
        ~MiniaudioBackend() override;

        bool Initialize() override;
        void Shutdown() override;
        void Update() override;

        BackendVoiceHandle CreateVoice(const AudioClip& clip) override;
        void DestroyVoice(BackendVoiceHandle handle) override;
        bool IsVoicePlaying(BackendVoiceHandle handle) const override;

        void PlayVoice(BackendVoiceHandle handle, bool loop) override;
        void StopVoice(BackendVoiceHandle handle) override;
        void PauseVoice(BackendVoiceHandle handle) override;
        void ResumeVoice(BackendVoiceHandle handle) override;

        void SetVoiceVolume(BackendVoiceHandle handle, float linearGain) override;
        void SetVoicePitch(BackendVoiceHandle handle, float pitch) override;

        void SetListenerEnabled(bool enabled) override;
        bool IsListenerEnabled() const override;
        void SetListener(const AudioListenerState& listener) override;
        void SetVoiceWorldPosition(BackendVoiceHandle handle, const Vector3& worldPosition) override;
        void SetVoiceSpatialSettings(BackendVoiceHandle handle, const AudioSpatialSettings& settings) override;
        void SetVoiceSpatializationEnabled(BackendVoiceHandle handle, bool enabled) override;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}

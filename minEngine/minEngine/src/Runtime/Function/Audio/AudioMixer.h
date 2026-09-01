#pragma once

#include "Runtime/Function/Audio/AudioTypes.h"

namespace minEngine
{
    class AudioMixer
    {
    public:
        AudioMixer();

        float GetBusVolume(EAudioBusId bus) const;
        void SetBusVolume(EAudioBusId bus, float volume);

        bool IsBusMuted(EAudioBusId bus) const;
        void SetBusMuted(EAudioBusId bus, bool muted);

        float ComputeEffectiveGain(EAudioBusId bus, float voiceVolume) const;

    private:
        struct BusState
        {
            float Volume{1.0f};
            bool bMuted{false};
        };

        BusState m_Buses[static_cast<size_t>(EAudioBusId::Count)];
    };
}

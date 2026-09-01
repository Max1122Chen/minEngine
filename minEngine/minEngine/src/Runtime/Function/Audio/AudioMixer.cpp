#include "Runtime/Function/Audio/AudioMixer.h"

#include <algorithm>

namespace minEngine
{
    AudioMixer::AudioMixer()
    {
        for (BusState& bus : m_Buses)
        {
            bus.Volume = 1.0f;
            bus.bMuted = false;
        }
    }

    float AudioMixer::GetBusVolume(EAudioBusId bus) const
    {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(EAudioBusId::Count))
        {
            return 1.0f;
        }

        return m_Buses[index].Volume;
    }

    void AudioMixer::SetBusVolume(EAudioBusId bus, float volume)
    {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(EAudioBusId::Count))
        {
            return;
        }

        m_Buses[index].Volume = std::clamp(volume, kMinAudioVolume, kMaxAudioVolume);
    }

    bool AudioMixer::IsBusMuted(EAudioBusId bus) const
    {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(EAudioBusId::Count))
        {
            return false;
        }

        return m_Buses[index].bMuted;
    }

    void AudioMixer::SetBusMuted(EAudioBusId bus, bool muted)
    {
        const size_t index = static_cast<size_t>(bus);
        if (index >= static_cast<size_t>(EAudioBusId::Count))
        {
            return;
        }

        m_Buses[index].bMuted = muted;
    }

    float AudioMixer::ComputeEffectiveGain(EAudioBusId bus, float voiceVolume) const
    {
        const float clampedVoiceVolume = std::clamp(voiceVolume, kMinAudioVolume, kMaxAudioVolume);

        if (bus == EAudioBusId::Master)
        {
            return IsBusMuted(EAudioBusId::Master) ? 0.0f : clampedVoiceVolume * GetBusVolume(EAudioBusId::Master);
        }

        const float masterGain =
            IsBusMuted(EAudioBusId::Master) ? 0.0f : GetBusVolume(EAudioBusId::Master);
        const float busGain = IsBusMuted(bus) ? 0.0f : GetBusVolume(bus);
        return clampedVoiceVolume * busGain * masterGain;
    }
}

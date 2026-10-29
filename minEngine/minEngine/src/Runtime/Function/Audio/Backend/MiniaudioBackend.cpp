#include "Runtime/Function/Audio/Backend/MiniaudioBackend.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Audio/AudioClipLease.h"
#include "Runtime/Function/Audio/AudioLimits.h"
#include "Runtime/Resource/AudioClip.h"

#include "miniaudio.h"

#include <algorithm>
#include <cmath>

namespace minEngine
{
    struct MiniaudioBackend::Impl
    {
        // ma_sound keeps a pointer to the buffer of the slot that created it, and ma_audio_buffer
        // keeps the sample pointer the slot was handed. The pool is sized once in the constructor and
        // never resized, so a slot address stays valid for the life of the backend.
        struct VoiceSlot
        {
            bool bAllocated{false};
            ma_audio_buffer Buffer{};
            ma_sound Sound{};
            bool bSoundInitialized{false};

            // The clip whose samples this slot reads. Held for exactly as long as the buffer above can
            // be read on the device thread, because miniaudio does not copy the samples.
            AudioClipLease ClipLease{};
        };

        ma_engine Engine{};
        bool bEngineInitialized{false};
        std::vector<VoiceSlot> Voices;

        VoiceSlot* GetSlot(BackendVoiceHandle handle)
        {
            if (!handle.IsValid() || handle.Index >= Voices.size())
            {
                return nullptr;
            }

            VoiceSlot& slot = Voices[handle.Index];
            return slot.bAllocated ? &slot : nullptr;
        }

        const VoiceSlot* GetSlot(BackendVoiceHandle handle) const
        {
            if (!handle.IsValid() || handle.Index >= Voices.size())
            {
                return nullptr;
            }

            const VoiceSlot& slot = Voices[handle.Index];
            return slot.bAllocated ? &slot : nullptr;
        }
    };

    MiniaudioBackend::MiniaudioBackend()
        : m_Impl(std::make_unique<Impl>())
    {
        m_Impl->Voices.resize(kMaxAudioVoices);
    }

    MiniaudioBackend::~MiniaudioBackend()
    {
        Shutdown();
    }

    bool MiniaudioBackend::Initialize()
    {
        if (m_Impl->bEngineInitialized)
        {
            return true;
        }

        ma_engine_config engineConfig = ma_engine_config_init();
        const ma_result result = ma_engine_init(&engineConfig, &m_Impl->Engine);
        if (result != MA_SUCCESS)
        {
            ME_LOG(LogAudio, Error, "MiniaudioBackend: ma_engine_init failed ({})", static_cast<int>(result));
            return false;
        }

        m_Impl->bEngineInitialized = true;
        ma_engine_listener_set_enabled(&m_Impl->Engine, 0, MA_FALSE);
        return true;
    }

    void MiniaudioBackend::Shutdown()
    {
        if (!m_Impl->bEngineInitialized)
        {
            return;
        }

        for (Impl::VoiceSlot& slot : m_Impl->Voices)
        {
            if (!slot.bAllocated)
            {
                continue;
            }

            if (slot.bSoundInitialized)
            {
                ma_sound_uninit(&slot.Sound);
                slot.bSoundInitialized = false;
            }

            ma_audio_buffer_uninit(&slot.Buffer);
            slot.ClipLease.Reset();
            slot.bAllocated = false;
        }

        ma_engine_uninit(&m_Impl->Engine);
        m_Impl->bEngineInitialized = false;
    }

    void MiniaudioBackend::Update()
    {
        // miniaudio drives playback on its own thread; nothing required on the main thread for MVP.
    }

    BackendVoiceHandle MiniaudioBackend::CreateVoice(std::shared_ptr<const AudioClip> clip)
    {
        BackendVoiceHandle invalidHandle;
        if (!m_Impl->bEngineInitialized)
        {
            return invalidHandle;
        }

        AudioClipLease lease(std::move(clip));
        if (!lease.IsValid())
        {
            ME_LOG(LogAudio, Error, "MiniaudioBackend: refusing a voice for a clip that cannot be leased.");
            return invalidHandle;
        }

        const auto freeSlotIt = std::find_if(
            m_Impl->Voices.begin(),
            m_Impl->Voices.end(),
            [](const Impl::VoiceSlot& slot) { return !slot.bAllocated; });
        if (freeSlotIt == m_Impl->Voices.end())
        {
            ME_LOG(LogAudio, Warn, "MiniaudioBackend: backend voice pool exhausted.");
            return invalidHandle;
        }

        const size_t slotIndex = static_cast<size_t>(std::distance(m_Impl->Voices.begin(), freeSlotIt));
        Impl::VoiceSlot& slot = m_Impl->Voices[slotIndex];

        // The lease is handed to the slot before the buffer is initialised: from here on the device
        // thread can read these samples, and the slot is what keeps them allocated.
        slot.ClipLease = std::move(lease);

        const ma_uint64 frameCount = slot.ClipLease.GetFrameCount();
        const ma_uint32 channels = slot.ClipLease.GetChannelCount();
        const float* const pcm = slot.ClipLease.GetPcmData();
        if (pcm == nullptr)
        {
            slot.ClipLease.Reset();
            ME_LOG(LogAudio, Error, "MiniaudioBackend: clip lease has no readable pcm buffer.");
            return invalidHandle;
        }

        ma_audio_buffer_config bufferConfig =
            ma_audio_buffer_config_init(ma_format_f32, channels, frameCount, pcm, nullptr);
        bufferConfig.sampleRate = slot.ClipLease.GetSampleRate();

        ma_result bufferResult = ma_audio_buffer_init(&bufferConfig, &slot.Buffer);
        if (bufferResult != MA_SUCCESS)
        {
            slot.ClipLease.Reset();
            ME_LOG(LogAudio, Error, "MiniaudioBackend: ma_audio_buffer_init failed ({})", static_cast<int>(bufferResult));
            return invalidHandle;
        }

        ma_sound_config soundConfig = ma_sound_config_init();
        soundConfig.pDataSource = &slot.Buffer;

        const ma_result soundResult = ma_sound_init_ex(&m_Impl->Engine, &soundConfig, &slot.Sound);
        if (soundResult != MA_SUCCESS)
        {
            ma_audio_buffer_uninit(&slot.Buffer);
            slot.ClipLease.Reset();
            ME_LOG(LogAudio, Error, "MiniaudioBackend: ma_sound_init_ex failed ({})", static_cast<int>(soundResult));
            return invalidHandle;
        }

        slot.bAllocated = true;
        slot.bSoundInitialized = true;
        return BackendVoiceHandle{static_cast<uint32_t>(slotIndex)};
    }

    void MiniaudioBackend::DestroyVoice(BackendVoiceHandle handle)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr)
        {
            return;
        }

        if (slot->bSoundInitialized)
        {
            ma_sound_uninit(&slot->Sound);
            slot->bSoundInitialized = false;
        }

        // The buffer has to be gone before the clip is released, or the device thread could still hold
        // a sample pointer into freed memory.
        ma_audio_buffer_uninit(&slot->Buffer);
        slot->ClipLease.Reset();
        slot->bAllocated = false;
    }

    bool MiniaudioBackend::IsVoicePlaying(BackendVoiceHandle handle) const
    {
        const Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return false;
        }

        return ma_sound_is_playing(&slot->Sound) == MA_TRUE;
    }

    void MiniaudioBackend::PlayVoice(BackendVoiceHandle handle, bool loop)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_set_looping(&slot->Sound, loop ? MA_TRUE : MA_FALSE);
        ma_sound_start(&slot->Sound);
    }

    void MiniaudioBackend::StopVoice(BackendVoiceHandle handle)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_stop(&slot->Sound);
    }

    void MiniaudioBackend::PauseVoice(BackendVoiceHandle handle)
    {
        StopVoice(handle);
    }

    void MiniaudioBackend::ResumeVoice(BackendVoiceHandle handle)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_start(&slot->Sound);
    }

    void MiniaudioBackend::SetVoiceVolume(BackendVoiceHandle handle, float linearGain)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_set_volume(&slot->Sound, std::clamp(linearGain, 0.0f, 1.0f));
    }

    void MiniaudioBackend::SetVoicePitch(BackendVoiceHandle handle, float pitch)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_set_pitch(&slot->Sound, std::clamp(pitch, kMinAudioPitch, kMaxAudioPitch));
    }

    void MiniaudioBackend::SetListenerEnabled(bool enabled)
    {
        if (!m_Impl->bEngineInitialized)
        {
            return;
        }

        ma_engine_listener_set_enabled(&m_Impl->Engine, 0, enabled ? MA_TRUE : MA_FALSE);
    }

    bool MiniaudioBackend::IsListenerEnabled() const
    {
        if (!m_Impl->bEngineInitialized)
        {
            return false;
        }

        return ma_engine_listener_is_enabled(&m_Impl->Engine, 0) == MA_TRUE;
    }

    void MiniaudioBackend::SetListener(const AudioListenerState& listener)
    {
        if (!m_Impl->bEngineInitialized)
        {
            return;
        }

        ma_engine_listener_set_position(
            &m_Impl->Engine,
            0,
            listener.Position.x,
            listener.Position.y,
            listener.Position.z);
        ma_engine_listener_set_direction(
            &m_Impl->Engine,
            0,
            listener.Forward.x,
            listener.Forward.y,
            listener.Forward.z);
        ma_engine_listener_set_world_up(
            &m_Impl->Engine,
            0,
            listener.Up.x,
            listener.Up.y,
            listener.Up.z);
    }

    void MiniaudioBackend::SetVoiceWorldPosition(BackendVoiceHandle handle, const Vector3& worldPosition)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_set_position(&slot->Sound, worldPosition.x, worldPosition.y, worldPosition.z);
    }

    void MiniaudioBackend::SetVoiceSpatialSettings(BackendVoiceHandle handle, const AudioSpatialSettings& settings)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_attenuation_model attenuationModel = ma_attenuation_model_inverse;
        switch (settings.AttenuationModel)
        {
        case EAudioAttenuationModel::None:
            attenuationModel = ma_attenuation_model_none;
            break;
        case EAudioAttenuationModel::Linear:
            attenuationModel = ma_attenuation_model_linear;
            break;
        case EAudioAttenuationModel::Inverse:
        default:
            attenuationModel = ma_attenuation_model_inverse;
            break;
        }

        ma_sound_set_attenuation_model(&slot->Sound, attenuationModel);
        ma_sound_set_min_distance(&slot->Sound, std::max(settings.MinDistance, 0.0f));
        ma_sound_set_max_distance(&slot->Sound, std::max(settings.MaxDistance, settings.MinDistance));
    }

    void MiniaudioBackend::SetVoiceSpatializationEnabled(BackendVoiceHandle handle, bool enabled)
    {
        Impl::VoiceSlot* slot = m_Impl->GetSlot(handle);
        if (slot == nullptr || !slot->bSoundInitialized)
        {
            return;
        }

        ma_sound_set_spatialization_enabled(&slot->Sound, enabled ? MA_TRUE : MA_FALSE);
    }
}

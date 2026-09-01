#pragma once

#include <cstdint>

namespace minEngine
{
    inline constexpr uint32_t kMaxAudioVoices = 32;
    inline constexpr float kMinAudioVolume = 0.0f;
    inline constexpr float kMaxAudioVolume = 1.0f;
    inline constexpr float kMinAudioPitch = 0.25f;
    inline constexpr float kMaxAudioPitch = 4.0f;
    inline constexpr float kDefaultMinDistance = 1.0f;
    inline constexpr float kDefaultMaxDistance = 100.0f;
    inline constexpr bool kEnableSpatialAudioDiagnostics = false;
    inline constexpr float kSpatialAudioDiagnosticsLogIntervalSeconds = 0.5f;
}

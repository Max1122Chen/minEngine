#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>

namespace minEngine
{
    struct ShadowPassParamsUBO
    {
        int32_t UseLinearDepth = 0;
        float Pad0[3]{};
        float LightPos[3]{};
        float FarPlane = 0.0f;
    };

    struct SkyPassFrameUBO
    {
        Matrix4 Projection = Matrix4(1.0f);
        Matrix4 View = Matrix4(1.0f);
        float SkyIntensity = 1.0f;
        float Pad[3]{};
    };

    struct EnginePostParamsUBO
    {
        float InvResolution[2]{};
        float ReduceMin = 0.0f;
        float ReduceMul = 0.0f;
        float SpanMax = 0.0f;
        float Strength = 0.0f;
        float EdgeThreshold = 0.0f;
        float Pad[2]{};
    };

    struct EnvCaptureFrameUBO
    {
        Matrix4 Projection = Matrix4(1.0f);
        Matrix4 View = Matrix4(1.0f);
        float Roughness = 0.0f;
        float EnvironmentResolution = 0.0f;
        float Pad[2]{};
    };
}

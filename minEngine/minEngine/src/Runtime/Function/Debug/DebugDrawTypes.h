#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>
#include <vector>

namespace minEngine
{
    struct DebugVertex
    {
        Vector3 Position{};
        Vector4 Color{};
    };

    enum class EDebugDepthMode : uint8_t
    {
        Tested = 0,
        AlwaysVisible,
    };

    enum class EDebugLifetime : uint8_t
    {
        Transient = 0,
    };

    struct DebugLineCommand
    {
        Vector3 Start{};
        Vector3 End{};
        Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
        EDebugLifetime Lifetime = EDebugLifetime::Transient;
    };

    struct DebugPointCommand
    {
        Vector3 Position{};
        float Size = 0.05f;
        Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
        EDebugLifetime Lifetime = EDebugLifetime::Transient;
    };

    struct DebugBoxCommand
    {
        Matrix4 WorldTransform{1.0f};
        Vector3 HalfExtent{0.5f, 0.5f, 0.5f};
        Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
        EDebugLifetime Lifetime = EDebugLifetime::Transient;
    };
}

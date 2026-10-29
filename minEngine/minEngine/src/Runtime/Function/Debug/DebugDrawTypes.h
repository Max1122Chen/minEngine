#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>
#include <memory>
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

    struct DebugSphereCommand
    {
        Matrix4 WorldTransform{1.0f};
        float Radius = 0.5f;
        Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        uint32_t Segments = 16;
        EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
        EDebugLifetime Lifetime = EDebugLifetime::Transient;
    };

    struct DebugCapsuleCommand
    {
        Matrix4 WorldTransform{1.0f};
        float Radius = 0.5f;
        float HalfHeight = 0.5f;
        Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        uint32_t Segments = 12;
        EDebugDepthMode DepthMode = EDebugDepthMode::Tested;
        EDebugLifetime Lifetime = EDebugLifetime::Transient;
    };

    /**
     * One frame of debug geometry, produced where the world is owned and read where it is drawn.
     *
     * It holds plain values only - transform matrices, extents, colours - so the drawing side can
     * consume it while the producing side keeps mutating the scene it came from. Nothing here
     * points at a scene, a game object or a component, which is what makes that safe.
     */
    struct DebugDrawSnapshot
    {
        std::vector<DebugBoxCommand> Boxes;
        std::vector<DebugSphereCommand> Spheres;
        std::vector<DebugCapsuleCommand> Capsules;

        bool IsEmpty() const { return Boxes.empty() && Spheres.empty() && Capsules.empty(); }
    };

    /** Shared by value; a consumer holds it for exactly as long as it needs the geometry. */
    using DebugDrawSnapshotRef = std::shared_ptr<const DebugDrawSnapshot>;
}

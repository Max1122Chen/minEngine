#pragma once

#include "DebugDrawTypes.h"

namespace minEngine::DebugDraw
{
    void Line(
        const Vector3& start,
        const Vector3& end,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Point(
        const Vector3& position,
        float size,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Box(
        const Matrix4& worldTransform,
        const Vector3& halfExtent,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Sphere(
        const Matrix4& worldTransform,
        float radius,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    void Capsule(
        const Matrix4& worldTransform,
        float radius,
        float halfHeight,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    /** Index pairs map to line segments (indexCount must be even). Expanded to LineList on the CPU. */
    MINENGINE_API void IndexedLines(
        const DebugVertex* vertices,
        uint32_t vertexCount,
        const uint32_t* indices,
        uint32_t indexCount,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);

    MINENGINE_API void IndexedLines(
        const Vector3* positions,
        uint32_t positionCount,
        const uint32_t* indices,
        uint32_t indexCount,
        const Vector4& color,
        EDebugDepthMode depthMode = EDebugDepthMode::Tested);
}

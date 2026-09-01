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
}

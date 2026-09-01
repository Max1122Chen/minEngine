#pragma once

#include "DebugDrawTypes.h"

namespace minEngine
{
    class DebugGeometry
    {
    public:
        static void AppendLine(
            std::vector<DebugVertex>& outVertices,
            const Vector3& start,
            const Vector3& end,
            const Vector4& color);

        static void AppendPointCross(
            std::vector<DebugVertex>& outVertices,
            const Vector3& position,
            float halfSize,
            const Vector4& color);

        static void AppendBoxWireframe(
            std::vector<DebugVertex>& outVertices,
            const Matrix4& worldTransform,
            const Vector3& halfExtent,
            const Vector4& color);
    };
}

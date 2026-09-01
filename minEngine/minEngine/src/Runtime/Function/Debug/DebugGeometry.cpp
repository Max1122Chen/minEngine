#include "DebugGeometry.h"

namespace minEngine
{
    void DebugGeometry::AppendLine(
        std::vector<DebugVertex>& outVertices,
        const Vector3& start,
        const Vector3& end,
        const Vector4& color)
    {
        outVertices.push_back({start, color});
        outVertices.push_back({end, color});
    }

    void DebugGeometry::AppendPointCross(
        std::vector<DebugVertex>& outVertices,
        const Vector3& position,
        const float halfSize,
        const Vector4& color)
    {
        AppendLine(outVertices, position - Vector3(halfSize, 0.0f, 0.0f), position + Vector3(halfSize, 0.0f, 0.0f), color);
        AppendLine(outVertices, position - Vector3(0.0f, halfSize, 0.0f), position + Vector3(0.0f, halfSize, 0.0f), color);
        AppendLine(outVertices, position - Vector3(0.0f, 0.0f, halfSize), position + Vector3(0.0f, 0.0f, halfSize), color);
    }

    void DebugGeometry::AppendBoxWireframe(
        std::vector<DebugVertex>& outVertices,
        const Matrix4& worldTransform,
        const Vector3& halfExtent,
        const Vector4& color)
    {
        const Vector3 corners[8] = {
            {-halfExtent.x, -halfExtent.y, -halfExtent.z},
            { halfExtent.x, -halfExtent.y, -halfExtent.z},
            { halfExtent.x,  halfExtent.y, -halfExtent.z},
            {-halfExtent.x,  halfExtent.y, -halfExtent.z},
            {-halfExtent.x, -halfExtent.y,  halfExtent.z},
            { halfExtent.x, -halfExtent.y,  halfExtent.z},
            { halfExtent.x,  halfExtent.y,  halfExtent.z},
            {-halfExtent.x,  halfExtent.y,  halfExtent.z},
        };

        Vector3 worldCorners[8];
        for (int index = 0; index < 8; ++index)
        {
            const Vector4 world = worldTransform * Vector4(corners[index], 1.0f);
            worldCorners[index] = Vector3(world);
        }

        static constexpr int kEdges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7},
        };

        for (const auto& edge : kEdges)
        {
            AppendLine(outVertices, worldCorners[edge[0]], worldCorners[edge[1]], color);
        }
    }
}

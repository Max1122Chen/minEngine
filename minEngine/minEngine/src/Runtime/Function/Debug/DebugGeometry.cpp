#include "DebugGeometry.h"

#include <glm/gtc/constants.hpp>

namespace minEngine
{
    namespace
    {
        Vector3 TransformPoint(const Matrix4& worldTransform, const Vector3& localPoint)
        {
            const Vector4 world = worldTransform * Vector4(localPoint, 1.0f);
            return Vector3(world);
        }

        void AppendCircleInPlane(
            std::vector<DebugVertex>& outVertices,
            const Matrix4& worldTransform,
            const Vector4& color,
            uint32_t segments,
            const Vector3& axisU,
            const Vector3& axisV,
            const Vector3& center)
        {
            if (segments < 3)
            {
                segments = 3;
            }

            const float step = glm::two_pi<float>() / static_cast<float>(segments);
            Vector3 previousPoint = center + axisU;
            for (uint32_t segmentIndex = 1; segmentIndex <= segments; ++segmentIndex)
            {
                const float angle = step * static_cast<float>(segmentIndex);
                const Vector3 currentPoint = center + axisU * std::cos(angle) + axisV * std::sin(angle);
                DebugGeometry::AppendLine(
                    outVertices,
                    TransformPoint(worldTransform, previousPoint),
                    TransformPoint(worldTransform, currentPoint),
                    color);
                previousPoint = currentPoint;
            }
        }
    }
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

    void DebugGeometry::AppendSphereWireframe(
        std::vector<DebugVertex>& outVertices,
        const Matrix4& worldTransform,
        const float radius,
        const Vector4& color,
        uint32_t segments)
    {
        if (segments < 3)
        {
            segments = 3;
        }

        AppendCircleInPlane(outVertices, worldTransform, color, segments, Vector3(radius, 0.0f, 0.0f), Vector3(0.0f, radius, 0.0f), Vector3(0.0f));
        AppendCircleInPlane(outVertices, worldTransform, color, segments, Vector3(radius, 0.0f, 0.0f), Vector3(0.0f, 0.0f, radius), Vector3(0.0f));
        AppendCircleInPlane(outVertices, worldTransform, color, segments, Vector3(0.0f, radius, 0.0f), Vector3(0.0f, 0.0f, radius), Vector3(0.0f));
    }

    void DebugGeometry::AppendCapsuleWireframe(
        std::vector<DebugVertex>& outVertices,
        const Matrix4& worldTransform,
        const float radius,
        const float halfHeight,
        const Vector4& color,
        uint32_t segments)
    {
        if (segments < 3)
        {
            segments = 3;
        }

        const Vector3 topCenter(0.0f, halfHeight, 0.0f);
        const Vector3 bottomCenter(0.0f, -halfHeight, 0.0f);

        AppendCircleInPlane(
            outVertices,
            worldTransform,
            color,
            segments,
            Vector3(radius, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, radius),
            topCenter);
        AppendCircleInPlane(
            outVertices,
            worldTransform,
            color,
            segments,
            Vector3(radius, 0.0f, 0.0f),
            Vector3(0.0f, 0.0f, radius),
            bottomCenter);

        static constexpr int kMeridianCount = 4;
        for (int meridianIndex = 0; meridianIndex < kMeridianCount; ++meridianIndex)
        {
            const float meridianAngle = glm::half_pi<float>() * static_cast<float>(meridianIndex);
            const Vector3 radialDirection(std::cos(meridianAngle), 0.0f, std::sin(meridianAngle));
            const Vector3 sideOffset = radialDirection * radius;

            AppendLine(
                outVertices,
                TransformPoint(worldTransform, bottomCenter + sideOffset),
                TransformPoint(worldTransform, topCenter + sideOffset),
                color);

            const uint32_t arcSegments = std::max(segments / 2u, 3u);
            const float arcStep = glm::half_pi<float>() / static_cast<float>(arcSegments);
            Vector3 previousTopPoint = topCenter + sideOffset;
            Vector3 previousBottomPoint = bottomCenter + sideOffset;
            for (uint32_t arcIndex = 1; arcIndex <= arcSegments; ++arcIndex)
            {
                const float arcAngle = arcStep * static_cast<float>(arcIndex);
                const float horizontal = radius * std::cos(arcAngle);
                const float vertical = radius * std::sin(arcAngle);

                const Vector3 currentTopPoint = topCenter + radialDirection * horizontal + Vector3(0.0f, vertical, 0.0f);
                const Vector3 currentBottomPoint = bottomCenter + radialDirection * horizontal - Vector3(0.0f, vertical, 0.0f);

                AppendLine(
                    outVertices,
                    TransformPoint(worldTransform, previousTopPoint),
                    TransformPoint(worldTransform, currentTopPoint),
                    color);
                AppendLine(
                    outVertices,
                    TransformPoint(worldTransform, previousBottomPoint),
                    TransformPoint(worldTransform, currentBottomPoint),
                    color);

                previousTopPoint = currentTopPoint;
                previousBottomPoint = currentBottomPoint;
            }
        }
    }
}

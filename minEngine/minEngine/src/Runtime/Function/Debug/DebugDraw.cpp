#include "DebugDraw.h"

#include "DebugDrawService.h"

namespace minEngine::DebugDraw
{
    void Line(
        const Vector3& start,
        const Vector3& end,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        DebugLineCommand command;
        command.Start = start;
        command.End = end;
        command.Color = color;
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueLine(command);
    }

    void Point(
        const Vector3& position,
        const float size,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        DebugPointCommand command;
        command.Position = position;
        command.Size = size;
        command.Color = color;
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueuePoint(command);
    }

    void Box(
        const Matrix4& worldTransform,
        const Vector3& halfExtent,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        DebugBoxCommand command;
        command.WorldTransform = worldTransform;
        command.HalfExtent = halfExtent;
        command.Color = color;
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueBox(command);
    }

    void Sphere(
        const Matrix4& worldTransform,
        const float radius,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        DebugSphereCommand command;
        command.WorldTransform = worldTransform;
        command.Radius = radius;
        command.Color = color;
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueSphere(command);
    }

    void Capsule(
        const Matrix4& worldTransform,
        const float radius,
        const float halfHeight,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        DebugCapsuleCommand command;
        command.WorldTransform = worldTransform;
        command.Radius = radius;
        command.HalfHeight = halfHeight;
        command.Color = color;
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueCapsule(command);
    }

    void IndexedLines(
        const DebugVertex* vertices,
        const uint32_t vertexCount,
        const uint32_t* indices,
        const uint32_t indexCount,
        const EDebugDepthMode depthMode)
    {
        if (vertices == nullptr || indices == nullptr || vertexCount == 0u || indexCount == 0u)
        {
            return;
        }

        DebugIndexedLinesCommand command;
        command.Vertices.assign(vertices, vertices + vertexCount);
        command.Indices.assign(indices, indices + indexCount);
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueIndexedLines(command);
    }

    void IndexedLines(
        const Vector3* positions,
        const uint32_t positionCount,
        const uint32_t* indices,
        const uint32_t indexCount,
        const Vector4& color,
        const EDebugDepthMode depthMode)
    {
        if (positions == nullptr || indices == nullptr || positionCount == 0u || indexCount == 0u)
        {
            return;
        }

        DebugIndexedLinesCommand command;
        command.Vertices.resize(positionCount);
        for (uint32_t i = 0; i < positionCount; ++i)
        {
            command.Vertices[i].Position = positions[i];
            command.Vertices[i].Color = color;
        }
        command.Indices.assign(indices, indices + indexCount);
        command.DepthMode = depthMode;
        DebugDrawService::Get().EnqueueIndexedLines(command);
    }
}

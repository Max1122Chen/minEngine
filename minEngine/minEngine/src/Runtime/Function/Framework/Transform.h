#pragma once
#include "Core.h"

namespace minEngine
{
    struct Transform
    {
        Vector3 Position{ 0.0f, 0.0f, 0.0f };
        Vector3 Rotation{ 0.0f, 0.0f, 0.0f };
        Vector3 Scale{ 1.0f, 1.0f, 1.0f };

        Transform() = default;
        Transform(const Vector3& position, const Vector3& rotation, const Vector3& scale)
            : Position(position), Rotation(rotation), Scale(scale)
        {}

        Transform(const Transform& other)
        {
            Position = other.Position;
            Rotation = other.Rotation;
            Scale = other.Scale;
        };

        void SetPosition(const Vector3& position) { Position = position; }
        void SetRotation(const Vector3& rotation) { Rotation = rotation; }
        void SetScale(const Vector3& scale) { Scale = scale; }

        bool operator==(const Transform& other) const
        {
            return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
        }
    };
}
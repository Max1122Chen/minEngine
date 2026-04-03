#pragma once
#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    ME_CLASS(Category = "Math")
    struct Transform
    {
        ME_PROPERTY(EditAnywhere, Category = "Location")
        Vector3 Position{ 0.0f, 0.0f, 0.0f };
        ME_PROPERTY(EditAnywhere, Category = "Rotation")
        Vector3 Rotation{ 0.0f, 0.0f, 0.0f };   // TODO: use quaternion later
        ME_PROPERTY(EditAnywhere, Category = "Scale")
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
        
        Matrix4 ToMatrix() const
        {
            // In the game, we use axis x for forward, y for up , z for right
            // but in glm, axis x for right, y for up, -z for forward
            Matrix4 model = glm::mat4(1.0f);
            Vector3 renderingPosition = Vector3(Position.z, Position.y, -Position.x);
            model = glm::translate(model, renderingPosition);                                   // translation
            model = glm::rotate(model, glm::radians(Rotation.z), Vector3(1.0f, 0.0f, 0.0f));    // rotation x
            model = glm::rotate(model, glm::radians(Rotation.y), Vector3(0.0f, 1.0f, 0.0f));    // rotation y
            model = glm::rotate(model, glm::radians(-Rotation.x), Vector3(0.0f, 0.0f, 1.0f));    // rotation z
            model = glm::scale(model, Scale);                                                   // scale
            return model;
        }

        void SetPosition(const Vector3& position) { Position = position; }
        void SetRotation(const Vector3& rotation) { Rotation = rotation; }
        void SetScale(const Vector3& scale) { Scale = scale; }

        bool operator==(const Transform& other) const
        {
            return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
        }
    };
}

#include "Generated/Reflection/Transform.gen.h"
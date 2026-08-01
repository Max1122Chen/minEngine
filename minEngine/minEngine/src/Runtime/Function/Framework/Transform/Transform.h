#pragma once
#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"
#include "Runtime/Core/Math/Math.h"

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/matrix_decompose.hpp"

namespace minEngine
{
    enum class Space
    {
        Local,
        World
    };

    ME_STRUCT(ScriptType)
    struct Transform
    {
        ME_GENERATED_BODY(Transform)
        
        ME_PROPERTY(EditAnywhere, ScriptReadWrite)
        Vector3 Position{ 0.0f, 0.0f, 0.0f };

        ME_PROPERTY(EditAnywhere)
        Vector3 Rotation{ 0.0f, 0.0f, 0.0f };   // TODO: use quaternion later

        ME_PROPERTY(EditAnywhere)
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

        ME_FUNCTION(ScriptPure)
        static Transform MakeIdentity() { return Transform{}; }
        
        ME_FUNCTION(ScriptCallable)
        void SetPosition(const Vector3& position) { Position = position; }
        void SetRotation(const Vector3& rotation) { Rotation = rotation; }
        void SetScale(const Vector3& scale) { Scale = scale; }

        ME_FUNCTION(ScriptCallable)
        void Translate(const Vector3& delta) { Position += delta; }
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local)
        {
            glm::quat quatCurrent = glm::quat(glm::radians(Rotation));
            if (relativeTo == Space::Local)
            {
                // For local rotation, we can simply concatenate the delta rotation with the current rotation.
                glm::quat quatNew = delta * quatCurrent;
                Vector3 euler = glm::degrees(glm::eulerAngles(quatNew));
                Rotation = Vector3(euler.x, euler.y, euler.z);
            }
            else
            {
                // For world rotation, we need to convert the delta rotation from world space to local space before concatenating it with the current rotation.
                glm::quat quatNew = quatCurrent * delta;
                Vector3 euler = glm::degrees(glm::eulerAngles(quatNew));
                Rotation = Vector3(euler.x, euler.y, euler.z);
            }
        }
        void ScaleBy(const Vector3& scaleFactor) { Scale *= scaleFactor; }

        // Return the transformation matrix in the game coordinate system (x for forward, y for up, z for right)
        Matrix4 ToMatrix() const
        {
            Matrix4 model = glm::mat4(1.0f);
            model = glm::translate(model, Position);                                   // translation
            model = glm::rotate(model, glm::radians(Rotation.x), Vector3(1.0f, 0.0f, 0.0f));    // rotation x
            model = glm::rotate(model, glm::radians(Rotation.y), Vector3(0.0f, 1.0f, 0.0f));    // rotation y
            model = glm::rotate(model, glm::radians(Rotation.z), Vector3(0.0f, 0.0f, 1.0f));    // rotation z
            model = glm::scale(model, Scale);                                                   // scale
            return model;
        }

        bool operator==(const Transform& other) const
        {
            return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
        }
    };
}

#include "Generated/Reflection/Transform.gen.h"


#pragma once
#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Math/Quaternion.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace minEngine
{
    enum class Space
    {
        Local,
        World
    };

    ME_STRUCT()
    struct Transform
    {
        ME_GENERATED_BODY(Transform)
        
        ME_PROPERTY(EditAnywhere)
        Vector3 Position{ 0.0f, 0.0f, 0.0f };

        ME_PROPERTY(EditAnywhere)
        Quaternion Rotation{};

        ME_PROPERTY(EditAnywhere)
        Vector3 Scale{ 1.0f, 1.0f, 1.0f };

        Transform() = default;
        Transform(const Vector3& position, const Vector3& rotationEulerDegrees, const Vector3& scale)
            : Position(position)
            , Rotation(Quaternion::FromEulerDegreesXYZ(rotationEulerDegrees))
            , Scale(scale)
        {}

        Transform(const Transform& other)
            : Position(other.Position)
            , Rotation(other.Rotation)
            , Scale(other.Scale)
        {}
        
        void SetPosition(const Vector3& position) { Position = position; }
        const Quaternion& GetRotation() const { return Rotation; }
        void SetRotation(const Quaternion& rotation) { Rotation = Quaternion::FromGlm(glm::normalize(rotation.ToGlm())); }
        Vector3 GetRotationEulerDegrees() const { return Rotation.ToEulerDegreesXYZ(); }
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees)
        {
            Rotation = Quaternion::FromEulerDegreesXYZ(rotationEulerDegrees);
        }
        void SetScale(const Vector3& scale) { Scale = scale; }

        void Translate(const Vector3& delta) { Position += delta; }
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local)
        {
            glm::quat quatCurrent = Rotation.ToGlm();
            if (relativeTo == Space::Local)
            {
                Rotation = Quaternion::FromGlm(delta * quatCurrent);
            }
            else
            {
                Rotation = Quaternion::FromGlm(quatCurrent * delta);
            }
        }
        void ScaleBy(const Vector3& scaleFactor) { Scale *= scaleFactor; }

        // Return the transformation matrix in the game coordinate system (x for forward, y for up, z for right)
        Matrix4 ToMatrix() const
        {
            Matrix4 model = glm::translate(glm::mat4(1.0f), Position);
            model *= glm::mat4_cast(Rotation.ToGlm());
            model = glm::scale(model, Scale);
            return model;
        }

        bool operator==(const Transform& other) const
        {
            return Position == other.Position && Rotation == other.Rotation && Scale == other.Scale;
        }
    };
}

#include "Generated/Reflection/Transform.gen.h"

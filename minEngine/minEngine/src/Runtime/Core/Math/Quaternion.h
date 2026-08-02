#pragma once

#include "Core.h"
#include "Core/Reflection/ReflectionAnnotations.h"
#include "Runtime/Core/Math/Math.h"

#include <glm/gtc/quaternion.hpp>

namespace minEngine
{
    ME_STRUCT()
    struct Quaternion
    {
        ME_GENERATED_BODY(Quaternion)

        ME_PROPERTY(EditAnywhere)
        float W = 1.0f;

        ME_PROPERTY(EditAnywhere)
        float X = 0.0f;

        ME_PROPERTY(EditAnywhere)
        float Y = 0.0f;

        ME_PROPERTY(EditAnywhere)
        float Z = 0.0f;

        Quaternion() = default;
        Quaternion(float inW, float inX, float inY, float inZ)
            : W(inW)
            , X(inX)
            , Y(inY)
            , Z(inZ)
        {
        }

        static Quaternion Identity() { return Quaternion(1.0f, 0.0f, 0.0f, 0.0f); }

        glm::quat ToGlm() const;
        static Quaternion FromGlm(const glm::quat& quat);
        static Quaternion FromEulerDegreesXYZ(const Vector3& eulerDegrees);
        Vector3 ToEulerDegreesXYZ() const;

        static bool AreRotationsEqual(const Quaternion& a, const Quaternion& b, float epsilon = 1e-5f);

        bool operator==(const Quaternion& other) const;
        bool operator!=(const Quaternion& other) const { return !(*this == other); }
    };
}

#include "Generated/Reflection/Quaternion.gen.h"

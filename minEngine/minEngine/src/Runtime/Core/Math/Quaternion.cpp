#include "Runtime/Core/Math/Quaternion.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace minEngine
{
    glm::quat Quaternion::ToGlm() const
    {
        return glm::normalize(glm::quat(W, X, Y, Z));
    }

    Quaternion Quaternion::FromGlm(const glm::quat& quat)
    {
        const glm::quat normalized = glm::normalize(quat);
        return Quaternion(normalized.w, normalized.x, normalized.y, normalized.z);
    }

    Quaternion Quaternion::FromEulerDegreesXYZ(const Vector3& eulerDegrees)
    {
        const glm::quat rotationX = glm::angleAxis(glm::radians(eulerDegrees.x), Vector3(1.0f, 0.0f, 0.0f));
        const glm::quat rotationY = glm::angleAxis(glm::radians(eulerDegrees.y), Vector3(0.0f, 1.0f, 0.0f));
        const glm::quat rotationZ = glm::angleAxis(glm::radians(eulerDegrees.z), Vector3(0.0f, 0.0f, 1.0f));
        return FromGlm(rotationX * rotationY * rotationZ);
    }

    Vector3 Quaternion::ToEulerDegreesXYZ() const
    {
        const Matrix4 rotationMatrix = glm::mat4_cast(ToGlm());
        float eulerX = 0.0f;
        float eulerY = 0.0f;
        float eulerZ = 0.0f;
        glm::extractEulerAngleXYZ(rotationMatrix, eulerX, eulerY, eulerZ);
        return Vector3(glm::degrees(eulerX), glm::degrees(eulerY), glm::degrees(eulerZ));
    }

    bool Quaternion::AreRotationsEqual(const Quaternion& a, const Quaternion& b, float epsilon)
    {
        const float dotProduct = glm::abs(glm::dot(a.ToGlm(), b.ToGlm()));
        return dotProduct >= 1.0f - epsilon;
    }

    bool Quaternion::operator==(const Quaternion& other) const
    {
        return AreRotationsEqual(*this, other);
    }
}

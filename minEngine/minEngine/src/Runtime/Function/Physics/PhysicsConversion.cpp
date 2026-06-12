#include "PhysicsConversion.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace minEngine
{
    namespace
    {
        glm::mat3 EngineToJoltBasisMatrix()
        {
            // Maps engine basis vectors into Jolt coordinates:
            // engine +X (forward) -> Jolt +Z, +Y (up) -> +Y, +Z (right) -> +X.
            return glm::mat3(
                glm::vec3(0.0f, 0.0f, 1.0f),
                glm::vec3(0.0f, 1.0f, 0.0f),
                glm::vec3(1.0f, 0.0f, 0.0f));
        }
    }

    Vector3 PhysicsConversion::ToJoltPosition(const Vector3& enginePosition)
    {
        const glm::mat3 basis = EngineToJoltBasisMatrix();
        const glm::vec3 joltPosition = basis * glm::vec3(enginePosition);
        return Vector3(joltPosition.x, joltPosition.y, joltPosition.z);
    }

    Vector3 PhysicsConversion::FromJoltPosition(const Vector3& joltPosition)
    {
        const glm::mat3 basis = EngineToJoltBasisMatrix();
        const glm::vec3 enginePosition = glm::transpose(basis) * glm::vec3(joltPosition);
        return Vector3(enginePosition.x, enginePosition.y, enginePosition.z);
    }

    Quaternion PhysicsConversion::ToJoltQuaternion(const Quaternion& engineRotation)
    {
        const glm::mat3 basis = EngineToJoltBasisMatrix();
        const glm::quat engineQuat = engineRotation.ToGlm();
        const glm::mat3 engineMatrix = glm::mat3_cast(engineQuat);
        const glm::mat3 joltMatrix = basis * engineMatrix * glm::transpose(basis);
        return Quaternion::FromGlm(glm::quat_cast(joltMatrix));
    }

    Quaternion PhysicsConversion::FromJoltQuaternion(const Quaternion& joltRotation)
    {
        const glm::mat3 basis = EngineToJoltBasisMatrix();
        const glm::quat joltQuat = joltRotation.ToGlm();
        const glm::mat3 joltMatrix = glm::mat3_cast(joltQuat);
        const glm::mat3 engineMatrix = glm::transpose(basis) * joltMatrix * basis;
        return Quaternion::FromGlm(glm::quat_cast(engineMatrix));
    }
}

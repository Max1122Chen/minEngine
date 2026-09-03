#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"

#include "assimp/postprocess.h"

#include <cmath>

namespace minEngine
{
    unsigned int AssimpMeshImportUtil::GetDefaultPostProcessFlags()
    {
        return aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;
    }

    Vector4 AssimpMeshImportUtil::ComputeFallbackTangent(const Vector3& normal)
    {
        const Vector3 up =
            (std::abs(normal.y) < 0.999f) ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(1.0f, 0.0f, 0.0f);
        Vector3 tangent = glm::normalize(glm::cross(up, normal));
        return Vector4(tangent, 1.0f);
    }

    Vector4 AssimpMeshImportUtil::ComputeTangentWithHandedness(
        const Vector3& normal,
        const Vector3* tangent,
        const Vector3* bitangent)
    {
        if (tangent == nullptr)
        {
            return ComputeFallbackTangent(normal);
        }

        Vector3 tangentValue = *tangent;
        if (glm::dot(tangentValue, tangentValue) <= 1e-12f)
        {
            return ComputeFallbackTangent(normal);
        }

        tangentValue = glm::normalize(tangentValue - normal * glm::dot(normal, tangentValue));
        float handedness = 1.0f;
        if (bitangent != nullptr)
        {
            handedness =
                (glm::dot(glm::cross(normal, tangentValue), *bitangent) < 0.0f) ? -1.0f : 1.0f;
        }

        return Vector4(tangentValue, handedness);
    }
}

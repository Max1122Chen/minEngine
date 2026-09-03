#include "Runtime/Resource/Loaders/AssimpMeshImportUtil.h"

#include "assimp/matrix4x4.h"
#include "assimp/postprocess.h"

#include <cmath>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

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

    Matrix4 AssimpMeshImportUtil::ConvertMatrix(const aiMatrix4x4& matrix)
    {
        // Assimp row-major → glm column-major.
        return Matrix4(
            matrix.a1,
            matrix.b1,
            matrix.c1,
            matrix.d1,
            matrix.a2,
            matrix.b2,
            matrix.c2,
            matrix.d2,
            matrix.a3,
            matrix.b3,
            matrix.c3,
            matrix.d3,
            matrix.a4,
            matrix.b4,
            matrix.c4,
            matrix.d4);
    }

    void AssimpMeshImportUtil::DecomposeMatrix(const Matrix4& matrix, Transform& outTransform)
    {
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        if (!glm::decompose(matrix, scale, rotation, translation, skew, perspective))
        {
            outTransform = Transform{};
            return;
        }

        outTransform.Position = translation;
        outTransform.SetRotation(Quaternion::FromGlm(glm::normalize(rotation)));
        outTransform.Scale = scale;
    }
}

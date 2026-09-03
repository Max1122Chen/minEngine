#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include "assimp/matrix4x4.h"

namespace minEngine
{
    // Shared Assimp geometry helpers. Used by StaticMeshLoader and SkeletalMeshLoader.
    class AssimpMeshImportUtil
    {
    public:
        static unsigned int GetDefaultPostProcessFlags();

        static Vector4 ComputeFallbackTangent(const Vector3& normal);

        // tangent/bitangent may be null → fallback from normal.
        static Vector4 ComputeTangentWithHandedness(
            const Vector3& normal,
            const Vector3* tangent,
            const Vector3* bitangent);

        static Matrix4 ConvertMatrix(const aiMatrix4x4& matrix);
        static void DecomposeMatrix(const Matrix4& matrix, Transform& outTransform);
    };
}

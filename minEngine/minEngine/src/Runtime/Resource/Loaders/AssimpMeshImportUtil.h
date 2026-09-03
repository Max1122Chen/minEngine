#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    // Shared Assimp geometry helpers (no bone/skinning). Used by StaticMeshLoader;
    // SkeletalMeshLoader (S01) should reuse the same post-process flags and tangent math.
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
    };
}

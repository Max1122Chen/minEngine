#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Geometry/AABB.h"
#include "Runtime/Core/Math/Math.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    struct MeshImportVertex
    {
        Vector3 Position;
        Vector2 TexCoord;
        Vector3 Normal;
        Vector4 Tangent;
    };

    struct MeshImportSection
    {
        int32_t MaterialIndex = 0;
        uint32_t FirstIndex = 0;
        uint32_t NumIndices = 0;
    };

    struct MeshImportData
    {
        std::vector<MeshImportVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<MeshImportSection> Sections;
        Math::Geometry::AABB BoundingBox;

        bool IsValid() const { return !Vertices.empty(); }
    };

    class MeshLoader
    {
    public:
        static bool ImportFromFile(
            const std::string& path,
            MeshImportData& outData,
            std::string* outError = nullptr);
    };
}

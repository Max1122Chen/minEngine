#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Geometry/AABB.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Resource/AssetMeta.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class StaticMesh;

    struct StaticMeshImportVertex
    {
        Vector3 Position;
        Vector2 TexCoord;
        Vector3 Normal;
        Vector4 Tangent;
    };

    struct StaticMeshImportSection
    {
        int32_t MaterialIndex = 0;
        uint32_t FirstIndex = 0;
        uint32_t NumIndices = 0;
    };

    struct StaticMeshImportData
    {
        std::vector<StaticMeshImportVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<StaticMeshImportSection> Sections;
        Math::Geometry::AABB BoundingBox;

        bool IsValid() const { return !Vertices.empty(); }
    };

    class StaticMeshLoader
    {
    public:
        static bool ImportFromFile(
            const std::string& path,
            StaticMeshImportData& outData,
            std::string* outError = nullptr);

        static std::shared_ptr<StaticMesh> CreateFromImportData(
            const AssetMeta& meta,
            StaticMeshImportData& importData);

        static std::shared_ptr<StaticMesh> LoadFromAssetMeta(const AssetMeta& meta);
    };
}

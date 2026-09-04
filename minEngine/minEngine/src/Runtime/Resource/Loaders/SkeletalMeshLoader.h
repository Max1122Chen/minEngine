#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Geometry/AABB.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Animation/AnimationConstants.h"
#include "Runtime/Function/Animation/Skeleton.h"
#include "Runtime/Resource/AssetMeta.h"

#include <filesystem>
#include <string>
#include <vector>

namespace minEngine
{
    class SkeletalMesh;

    struct SkeletalMeshImportVertex
    {
        Vector3 Position;
        Vector2 TexCoord;
        Vector3 Normal;
        Vector4 Tangent;
        uint16_t BoneIndices[kMaxBoneInfluences]{};
        Vector4 BoneWeights{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct SkeletalMeshImportSection
    {
        int32_t MaterialIndex = 0;
        uint32_t FirstIndex = 0;
        uint32_t NumIndices = 0;
    };

    struct SkeletalMeshImportData
    {
        std::vector<SkeletalMeshImportVertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<SkeletalMeshImportSection> Sections;
        std::vector<SkeletonBone> Bones;
        Math::Geometry::AABB BoundingBox;

        bool IsValid() const { return !Vertices.empty() && !Bones.empty(); }
    };

    class SkeletalMeshLoader
    {
    public:
        static bool ImportFromFile(
            const std::string& path,
            SkeletalMeshImportData& outData,
            std::string* outError = nullptr);

        static std::shared_ptr<Skeleton> CreateSkeletonFromImport(
            const AssetMeta& meta,
            const SkeletalMeshImportData& data);

        static std::shared_ptr<SkeletalMesh> CreateFromImportData(
            const AssetMeta& meta,
            SkeletalMeshImportData& data,
            const std::shared_ptr<Skeleton>& skeleton);

        static std::string BuildBuddyRelativePath(std::string_view meshAssetPath);
        static bool SaveBuddy(const AssetMeta& meshMeta, const std::shared_ptr<Skeleton>& skeleton);
        static bool TryLoadBuddySkeleton(const AssetMeta& meshMeta, std::shared_ptr<Skeleton>& outSkeleton);

        static std::shared_ptr<SkeletalMesh> LoadFromAssetMeta(const AssetMeta& meta);

        /** After ImportExternalMesh writes .glb: skeleton + .meskmesh buddy. */
        static bool FinishSkeletalImportCook(
            const AssetMeta& meshMeta,
            const std::filesystem::path& sourceAbsolutePath,
            std::string* outError = nullptr);
    };
}

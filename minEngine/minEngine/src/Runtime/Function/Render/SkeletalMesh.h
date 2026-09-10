#pragma once

#include "Core.h"
#include "Math/Math.h"
#include "Math/Geometry/AABB.h"
#include "Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"
#include "Render/RHI/RHIBuffers.h"

#include <memory>
#include <vector>

namespace minEngine
{
    class Material;
    class Skeleton;

    struct SkeletalMeshSectionInfo
    {
        int32_t MaterialIndex{0};
        uint32_t FirstIndex{0};
        uint32_t NumIndices{0};
    };

    ME_CLASS()
    class SkeletalMesh : public Asset
    {
        ME_GENERATED_BODY()
    public:
        SkeletalMesh() = default;
        ~SkeletalMesh() override = default;

        Skeleton* GetSkeleton() const { return m_Skeleton.get(); }
        void SetSkeleton(const std::shared_ptr<Skeleton>& skeleton) { m_Skeleton = skeleton; }

        Math::Geometry::AABB m_BoundingBox;

        RHIBufferRef m_VertexBuffer;
        RHIVertexInputLayoutRef m_VertexInputLayout;
        RHIBufferRef m_IndexBuffer;

        std::vector<SkeletalMeshSectionInfo> m_Sections;
        std::vector<std::shared_ptr<Material>> m_Materials;

        ME_PROPERTY()
        std::shared_ptr<Skeleton> m_Skeleton;
    };
}

#include "Generated/Reflection/SkeletalMesh.gen.h"

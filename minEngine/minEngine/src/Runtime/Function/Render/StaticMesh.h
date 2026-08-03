#pragma once

#include "Core.h"
#include "Math/Math.h"
#include "Math/Geometry/AABB.h"
#include "Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"
#include "Render/RHI/RHIBuffers.h"

namespace minEngine
{
    class Material;

    struct StaticMeshSectionInfo
    {
        int32_t MaterialIndex {0};

        uint32_t FirstIndex {0};
        uint32_t NumIndices {0};
    };

    ME_CLASS()
    class StaticMesh : public Asset
    {
        ME_GENERATED_BODY(StaticMesh)
    public:
        StaticMesh() = default;
        StaticMesh(
            float* vertices,
            uint32_t verticesSize,
            uint32_t numVertices,
            std::initializer_list<RHIVertexElement> elements);
        StaticMesh(
            float* vertices,
            uint32_t verticesSize,
            uint32_t numVertices,
            std::initializer_list<RHIVertexElement> elements,
            uint32_t* indices,
            uint32_t numIndices);
        ~StaticMesh() = default;

        Math::Geometry::AABB m_BoundingBox;

        RHIBufferRef m_VertexBuffer;
        RHIVertexInputLayoutRef m_VertexInputLayout;
        RHIBufferRef m_IndexBuffer;

        std::vector<StaticMeshSectionInfo> m_Sections;
        std::vector<std::shared_ptr<Material>> m_Materials;
    };
}

#include "StaticMesh.gen.h"

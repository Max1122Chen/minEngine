#pragma once

#include "Core.h"
#include "Math/Math.h"
#include "Math/Geometry/AABB.h"
#include "Core/Object/MEObject.h"
#include "Runtime/Resource/Asset.h"


namespace minEngine
{
    class VertexBuffer;
    class IndexBuffer;
    class VertexElement;
    class VertexDefinition;
    class Material;

    struct StaticMeshSectionInfo
    {
        int32_t MaterialIndex {0};

        uint32_t FirstIndex {0};
        uint32_t NumIndices {0};
        // TODO: do we need a bounding box for each section? maybe we can use it for frustum culling later
    };



    ME_CLASS()
    class StaticMesh : public Asset
    {
        ME_GENERATED_BODY(StaticMesh)
    public:
        StaticMesh() = default;
        StaticMesh(float* vertices, 
                   uint32_t verticesSize,
                   uint32_t numVertices,
                   std::initializer_list<VertexElement> elements);    // create from vertex data only
        StaticMesh(float* vertices,
                   uint32_t verticesSize,
                   uint32_t numVertices,
                   std::initializer_list<VertexElement> elements,
                   uint32_t* indices, uint32_t numIndices);
        ~StaticMesh() = default;
        
        Math::Geometry::AABB m_BoundingBox;

        std::shared_ptr<VertexBuffer> m_VertexBuffer = nullptr;
        std::shared_ptr<VertexDefinition> m_VertexDefinition = nullptr;
        std::shared_ptr<IndexBuffer> m_IndexBuffer = nullptr;

        std::vector<StaticMeshSectionInfo> m_Sections;
        std::vector<std::shared_ptr<Material>> m_Materials;
    };
}

#include "StaticMesh.gen.h"
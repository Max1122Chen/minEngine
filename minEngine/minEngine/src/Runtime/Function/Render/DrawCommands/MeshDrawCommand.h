#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Math/Geometry/AABB.h"

#include <memory>

namespace minEngine
{
    class Material;
    class RHIBindingSet;
    class RHIBuffer;
    class RHIGraphicsPipelineState;
    class RHIVertexInputLayout;

    class MeshDrawCommandSortKey
    {
    public:
        MeshDrawCommandSortKey() = default;
        virtual ~MeshDrawCommandSortKey() = default;

        union
        {
            uint64_t m_Key = 0;
        };

        bool operator<(const MeshDrawCommandSortKey& other) const { return m_Key < other.m_Key; }
        bool operator==(const MeshDrawCommandSortKey& other) const { return m_Key == other.m_Key; }
        bool operator!=(const MeshDrawCommandSortKey& other) const { return m_Key != other.m_Key; }
    };

    class MeshDrawCommand
    {
    public:
        MeshDrawCommand() = default;
        virtual ~MeshDrawCommand() = default;

        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;
        std::shared_ptr<RHIGraphicsPipelineState> m_PipelineState;
        RHIBindingSet* m_MaterialBindingSet = nullptr;

        Matrix4 m_ModelMatrix;
        Math::Geometry::AABB m_BoundingBox;
        bool m_CastShadow = true;

        MeshDrawCommandSortKey m_SortKey;
    };
}

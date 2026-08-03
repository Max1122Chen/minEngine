#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class Material;
    class RHIBuffer;
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

    /** Pass-agnostic logical draw item built by ForwardRenderer::BuildRenderQueue. */
    class MeshDrawCommand
    {
    public:
        MeshDrawCommand() = default;
        virtual ~MeshDrawCommand() = default;

        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        Material* m_Material = nullptr;

        Matrix4 m_ModelMatrix;
        Math::Geometry::AABB m_BoundingBox;
        bool m_CastShadow = true;

        MeshDrawCommandSortKey m_SortKey;
    };
}


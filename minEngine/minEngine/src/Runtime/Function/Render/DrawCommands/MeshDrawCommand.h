#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class MaterialRenderProxy;
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

    /** Pass draw item built from a MeshBatch by a MeshPassProcessor. Proxies do not submit this type. */
    class MINENGINE_API MeshDrawCommand
    {
    public:
        MeshDrawCommand() = default;
        virtual ~MeshDrawCommand() = default;

        /**
         * The geometry this command draws, as owning handles - the buffers and the layout that says
         * how the vertex buffer is read.
         *
         * A command is collected in one step and submitted in a later one, and a pass may keep it
         * until the next frame. Holding the handles here is what makes the command safe to keep:
         * the proxy that emitted it can be removed in between, and the buffers must not be freed
         * with it while this command still names them.
         */
        RHIBufferRef m_VertexBufferRef;
        RHIVertexInputLayoutRef m_VertexInputLayoutRef;
        RHIBufferRef m_IndexBufferRef;

        /** Frame views of the handles above; nothing else owns them through these. */
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        /** Draw ranges copied from the mesh metadata via MeshBatch. */
        uint32_t m_IndexCount = 0;
        uint32_t m_VertexCount = 0;

        MaterialRenderProxy* m_MaterialProxy = nullptr;

        Matrix4 m_ModelMatrix;
        Math::Geometry::AABB m_BoundingBox;
        bool m_CastShadow = true;

        /** Optional skinning palette (null/empty = rigid). */
        const Matrix4* m_BonePalette = nullptr;
        uint32_t m_BoneCount = 0;
        bool m_bSkinned = false;

        MeshDrawCommandSortKey m_SortKey;
    };
}

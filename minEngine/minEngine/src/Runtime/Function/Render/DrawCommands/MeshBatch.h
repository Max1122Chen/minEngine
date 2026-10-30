#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Math/Geometry/AABB.h"

namespace minEngine
{
    class MaterialRenderProxy;
    class PrimitiveSceneProxy;
    class RHIBuffer;
    class RHIVertexInputLayout;

    /** Pass-agnostic mesh description emitted by a PrimitiveSceneProxy. */
    class MeshBatch
    {
    public:
        /**
         * The geometry this batch draws, as owning handles.
         *
         * The proxy that emits the batch holds these too, but the batch is what travels: it becomes
         * a draw command, that command becomes a packet, and the packet is submitted after the
         * proxy may already be gone. Carrying the handles along the way is what lets every one of
         * those owners keep the geometry alive instead of assuming the previous one still does.
         */
        RHIBufferRef m_VertexBufferRef;
        RHIBufferRef m_IndexBufferRef;

        /** Frame views of the handles above; nothing else owns them through these. */
        RHIBuffer* m_VertexBuffer = nullptr;
        RHIVertexInputLayout* m_VertexInputLayout = nullptr;
        RHIBuffer* m_IndexBuffer = nullptr;

        /**
         * Draw ranges from the mesh metadata. The proxy that emitted this batch owns the buffers
         * for as long as the batch is alive, but the counts still travel with the batch: reading
         * them back from the GPU object at submission time would tie the draw to an object the
         * frame only borrows.
         */
        uint32_t m_IndexCount = 0;
        uint32_t m_VertexCount = 0;

        MaterialRenderProxy* m_MaterialProxy = nullptr;

        Matrix4 m_LocalToWorld{ 1.0f };
        Math::Geometry::AABB m_BoundingBox;
        bool m_CastShadow = true;

        const Matrix4* m_BonePalette = nullptr;
        uint32_t m_BoneCount = 0;
        bool m_bSkinned = false;

        /** Sprite (and similar) can force the translucent queue without a translucent material. */
        bool m_bForceTranslucentPass = false;

        const PrimitiveSceneProxy* m_Proxy = nullptr;
    };
}

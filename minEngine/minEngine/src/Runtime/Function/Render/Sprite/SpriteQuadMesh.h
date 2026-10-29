#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    class RHI;

    /**
     * Process-wide unit quad shared by SpriteComponent (world) and ScreenUI widgets.
     * Model space: center origin, edge length 1, +Z normal; the -Y edge carries uv v=0
     * (texture bottom). A caller placing this quad in a Y-down space (ScreenUI pixels)
     * owns that space conversion.
     * Vertex layout matches StaticMeshLoader (a_Position/a_TexCoord/a_Normal/a_Tangent).
     */
    class MINENGINE_API SpriteQuadMesh
    {
    public:
        static constexpr uint32_t VertexCount = 4;
        /** pos3 + uv2 + normal3 + tangent4. */
        static constexpr uint32_t FloatsPerVertex = 12;
        /** Float offset of a_TexCoord inside one interleaved vertex. */
        static constexpr uint32_t TexCoordFloatOffset = 3;
        static constexpr uint32_t IndexCount = 6;

        static SpriteQuadMesh& Get();

        /** CPU-side quad definition consumed by EnsureInitialized; tests read it as the geometry source of truth. */
        static const float* GetInterleavedVertexData();
        static const uint32_t* GetIndexData();

        void EnsureInitialized(RHI& rhi);
        void Shutdown();

        RHIBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
        RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }
        RHIVertexInputLayout* GetVertexInputLayout() const { return m_VertexInputLayout.get(); }

        /** Strong handles for callers that must outlive this singleton's own bookkeeping. */
        const RHIBufferRef& GetVertexBufferRef() const { return m_VertexBuffer; }
        const RHIBufferRef& GetIndexBufferRef() const { return m_IndexBuffer; }
        const RHIVertexInputLayoutRef& GetVertexInputLayoutRef() const { return m_VertexInputLayout; }
        bool IsReady() const
        {
            return m_VertexBuffer != nullptr && m_IndexBuffer != nullptr && m_VertexInputLayout != nullptr;
        }

    private:
        SpriteQuadMesh() = default;

        RHIBufferRef m_VertexBuffer;
        RHIBufferRef m_IndexBuffer;
        RHIVertexInputLayoutRef m_VertexInputLayout;
    };
}
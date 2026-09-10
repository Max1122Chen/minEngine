#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    class RHI;

    /**
     * Process-wide unit quad for SpriteComponent (center origin, edge length 1, +Z normal).
     * Vertex layout matches StaticMeshLoader (a_Position/a_TexCoord/a_Normal/a_Tangent).
     */
    class SpriteQuadMesh
    {
    public:
        static SpriteQuadMesh& Get();

        void EnsureInitialized(RHI& rhi);
        void Shutdown();

        RHIBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
        RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }
        RHIVertexInputLayout* GetVertexInputLayout() const { return m_VertexInputLayout.get(); }
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

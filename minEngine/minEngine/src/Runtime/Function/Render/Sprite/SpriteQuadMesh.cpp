#include "SpriteQuadMesh.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    namespace
    {
        // Interleaved: Position3 + TexCoord2 + Normal3 + Tangent4
        // Unit square in XY, center at origin, facing +Z. The -Y edge carries uv v=0 (texture bottom).
        const float kQuadVertices[] = {
            // pos                    uv       n              t
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };

        const uint32_t kQuadIndices[] = { 0, 1, 2, 2, 3, 0 };
    }

    SpriteQuadMesh& SpriteQuadMesh::Get()
    {
        static SpriteQuadMesh instance;
        return instance;
    }

    const float* SpriteQuadMesh::GetInterleavedVertexData()
    {
        return kQuadVertices;
    }

    const uint32_t* SpriteQuadMesh::GetIndexData()
    {
        return kQuadIndices;
    }

    void SpriteQuadMesh::EnsureInitialized(RHI& rhi)
    {
        if (IsReady())
        {
            return;
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = sizeof(kQuadVertices);
        vbDesc.ElementCount = VertexCount;
        m_VertexBuffer = rhi.RHICreateBuffer(vbDesc, kQuadVertices);

        RHIBufferCreateDesc ibDesc;
        ibDesc.Usage = RHIBufferUsage::Index;
        ibDesc.ByteSize = sizeof(kQuadIndices);
        ibDesc.ElementCount = IndexCount;
        m_IndexBuffer = rhi.RHICreateBuffer(ibDesc, kQuadIndices);

        m_VertexInputLayout = rhi.RHICreateVertexInputLayout({
            RHIVertexElement("a_Position", VertexElementType::Float3),
            RHIVertexElement("a_TexCoord", VertexElementType::Float2),
            RHIVertexElement("a_Normal", VertexElementType::Float3),
            RHIVertexElement("a_Tangent", VertexElementType::Float4),
        });
    }

    void SpriteQuadMesh::Shutdown()
    {
        m_VertexBuffer.reset();
        m_IndexBuffer.reset();
        m_VertexInputLayout.reset();
    }
}

#include "SpriteQuadMesh.h"

#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"

namespace minEngine
{
    SpriteQuadMesh& SpriteQuadMesh::Get()
    {
        static SpriteQuadMesh instance;
        return instance;
    }

    void SpriteQuadMesh::EnsureInitialized(RHI& rhi)
    {
        if (IsReady())
        {
            return;
        }

        // Interleaved: Position3 + TexCoord2 + Normal3 + Tangent4
        // Unit square in XY, center at origin, facing +Z.
        float vertices[] = {
            // pos                    uv       n              t
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        };

        uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = sizeof(vertices);
        vbDesc.ElementCount = 4;
        m_VertexBuffer = rhi.RHICreateBuffer(vbDesc, vertices);

        RHIBufferCreateDesc ibDesc;
        ibDesc.Usage = RHIBufferUsage::Index;
        ibDesc.ByteSize = sizeof(indices);
        ibDesc.ElementCount = 6;
        m_IndexBuffer = rhi.RHICreateBuffer(ibDesc, indices);

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

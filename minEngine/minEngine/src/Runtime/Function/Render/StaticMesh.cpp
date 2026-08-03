#include "StaticMesh.h"

#include "Render/RenderSystem.h"
#include "Render/RHI/RHI.h"

namespace minEngine
{
    namespace
    {
        void UploadMeshGeometry(
            RHI* rhi,
            StaticMesh& mesh,
            float* vertices,
            uint32_t verticesSize,
            uint32_t numVertices,
            std::initializer_list<RHIVertexElement> elements,
            uint32_t* indices,
            uint32_t numIndices)
        {
            if (!rhi)
            {
                return;
            }

            RHIBufferCreateDesc vbDesc;
            vbDesc.Usage = RHIBufferUsage::Vertex;
            vbDesc.ByteSize = verticesSize;
            vbDesc.ElementCount = numVertices;
            mesh.m_VertexBuffer = rhi->RHICreateBuffer(vbDesc, vertices);
            mesh.m_VertexInputLayout = rhi->RHICreateVertexInputLayout(elements);

            if (indices != nullptr && numIndices > 0)
            {
                RHIBufferCreateDesc ibDesc;
                ibDesc.Usage = RHIBufferUsage::Index;
                ibDesc.ByteSize = numIndices * sizeof(uint32_t);
                ibDesc.ElementCount = numIndices;
                mesh.m_IndexBuffer = rhi->RHICreateBuffer(ibDesc, indices);
            }
        }
    }

    StaticMesh::StaticMesh(
        float* vertices,
        uint32_t verticesSize,
        uint32_t numVertices,
        std::initializer_list<RHIVertexElement> elements)
    {
        UploadMeshGeometry(
            RenderSystem::Get().GetRHI(),
            *this,
            vertices,
            verticesSize,
            numVertices,
            elements,
            nullptr,
            0);
    }

    StaticMesh::StaticMesh(
        float* vertices,
        uint32_t verticesSize,
        uint32_t numVertices,
        std::initializer_list<RHIVertexElement> elements,
        uint32_t* indices,
        uint32_t numIndices)
    {
        UploadMeshGeometry(
            RenderSystem::Get().GetRHI(),
            *this,
            vertices,
            verticesSize,
            numVertices,
            elements,
            indices,
            numIndices);
    }
}

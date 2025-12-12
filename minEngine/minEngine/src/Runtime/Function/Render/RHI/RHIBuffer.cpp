#include "RHIBuffer.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"

namespace minEngine
{
    std::shared_ptr<VertexBuffer> VertexBuffer::Create(float *vertices, uint32_t size, uint32_t numVertices)
    {
        return RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->GetRHI()->CreateVertexBuffer(vertices, size, numVertices);
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t *indices, uint32_t numIndices)
    {
        return RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->GetRHI()->CreateIndexBuffer(indices, numIndices);
    }

    std::shared_ptr<VertexDefinition> VertexDefinition::Create(std::initializer_list<VertexElement> elements)
    {
        return RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->GetRHI()->CreateVertexDefinition(elements);
    }
}
#include "RHIBuffers.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"

namespace minEngine
{
    // TODO: change these to RHICommand later ?
    std::shared_ptr<VertexBuffer> VertexBuffer::Create(float *vertices, uint32_t size, uint32_t numVertices)
    {
        return RenderSystem::GetRenderSystem().GetRHI()->CreateVertexBuffer(vertices, size, numVertices);
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t *indices, uint32_t numIndices)
    {
        return RenderSystem::GetRenderSystem().GetRHI()->CreateIndexBuffer(indices, numIndices);
    }

    std::shared_ptr<VertexDefinition> VertexDefinition::Create(std::initializer_list<VertexElement> elements)
    {
        return RenderSystem::GetRenderSystem().GetRHI()->CreateVertexDefinition(elements);
    }

    std::shared_ptr<FrameBuffer> FrameBuffer::Create(uint32_t width, uint32_t height)
    {
        return RenderSystem::GetRenderSystem().GetRHI()->CreateFrameBuffer(width, height);
    }

}
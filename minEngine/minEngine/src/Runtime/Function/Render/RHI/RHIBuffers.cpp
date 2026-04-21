#include "RHIBuffers.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"

namespace minEngine
{
    // TODO: do we really need these static Create functions in the buffer classes ? maybe we can just call the RHI's CreateBuffer functions directly without these wrapper functions, since they don't really add any abstraction or functionality on top of the RHI's CreateBuffer functions, and they just forward the parameters to the RHI's CreateBuffer functions. We can always add these wrapper functions later if we need them for some reason, but for now I think it's simpler to just call the RHI's CreateBuffer functions directly without these wrapper functions.
    // TODO: change these to RHICommand later ?
    std::shared_ptr<VertexBuffer> VertexBuffer::Create(float *vertices, uint32_t size, uint32_t numVertices)
    {
        return RenderSystem::Get().GetRHI()->CreateVertexBuffer(vertices, size, numVertices);
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::Create(uint32_t *indices, uint32_t numIndices)
    {
        return RenderSystem::Get().GetRHI()->CreateIndexBuffer(indices, numIndices);
    }

    std::shared_ptr<VertexDefinition> VertexDefinition::Create(std::initializer_list<VertexElement> elements)
    {
        return RenderSystem::Get().GetRHI()->CreateVertexDefinition(elements);
    }

    std::shared_ptr<FrameBuffer> FrameBuffer::Create(uint32_t width, uint32_t height)
    {
        return RenderSystem::Get().GetRHI()->CreateFrameBuffer(width, height);
    }

    std::shared_ptr<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t bindingPoint)
    {
        return RenderSystem::Get().GetRHI()->CreateUniformBuffer(size, bindingPoint);
    }

}
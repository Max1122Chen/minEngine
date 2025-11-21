#include "RHIBuffer.h"
#include "OpenGLBuffer.h"

namespace minEngine
{
    VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
    {
        // Here we would normally have logic to choose the appropriate
        // graphics API (like OpenGL, DirectX, Vulkan, etc.) and create
        // the corresponding VertexBuffer implementation.
        // For this example, we'll assume OpenGL is being used.
        switch(0) // Replace 0 with actual API detection logic
        {
            case 0: // OpenGL
                return new OpenGLVertexBuffer(vertices, size);
            // case 1: // DirectX
            //     return CreateDirectXVertexBuffer(vertices, size);
            // case 2: // Vulkan
            //     return CreateVulkanVertexBuffer(vertices, size);
            default:
                return nullptr;
        }
    }

    IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t count)
    {
        // Similar to VertexBuffer::Create, we would have logic here
        // to create the appropriate IndexBuffer implementation.
        // For this example, we'll assume OpenGL is being used.
        switch(0) // Replace 0 with actual API detection logic
        {
            case 0: // OpenGL
                return new OpenGLIndexBuffer(indices, count);
            // case 1: // DirectX
            //     return CreateDirectXIndexBuffer(indices, count);
            // case 2: // Vulkan
            //     return CreateVulkanIndexBuffer(indices, count);
            default:
                return nullptr;
        }
    }
}
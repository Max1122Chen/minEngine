#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHIBuffer.h"
#include "Runtime/Function/Render/OpenGL/OpenGLBuffer.h"
#include "OpenGL/OpenGLVertexArrayObject.h"
#include "Runtime/Function/Render/RHITexture.h"

namespace minEngine
{
    class VertexBuffer;
    class IndexBuffer;
    class VertexDefinition;

    class StaticMesh
    {
    public:
        StaticMesh(std::string path);
        StaticMesh(float* vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements);
        StaticMesh(float* vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements, uint32_t* indices, uint32_t indexCount);
        ~StaticMesh() = default;

        std::shared_ptr<VertexBuffer> m_VertexBuffer = nullptr;
        std::shared_ptr<VertexDefinition> m_VertexDefinition = nullptr;
        std::shared_ptr<IndexBuffer> m_IndexBuffer = nullptr;
    };
}
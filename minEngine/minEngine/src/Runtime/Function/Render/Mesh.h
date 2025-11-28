#pragma once

#include "Core.h"
#include "Runtime/Function/Render/RHIBuffer.h"
#include "Runtime/Function/Render/OpenGL/OpenGLBuffer.h"
#include "OpenGL/OpenGLVertexArrayObject.h"
#include "Runtime/Function/Render/RHITexture.h"

namespace minEngine
{
    class Mesh
    {
    public:
        Mesh(std::string path);
        Mesh(float* vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements);
        Mesh(float* vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements, uint32_t* indices, uint32_t indexCount);
        ~Mesh() = default;

        VertexBuffer* m_VertexBuffer = nullptr;
        VertexDefinition* m_VertexDefinition = nullptr;
        IndexBuffer* m_IndexBuffer = nullptr;
    };
}
#include "StaticMesh.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"



namespace minEngine
{
    StaticMesh::StaticMesh(float *vertices, uint32_t verticesSize, uint32_t numVertices, std::initializer_list<VertexElement> elements)
    {
        // Becareful: we must create vertex buffer before vertex definition. This is because
        // some RHI implementations (OpenGL) may need to query vertex buffer info when creating vertex definition.
        m_VertexBuffer = VertexBuffer::Create(vertices, verticesSize, numVertices);
        m_VertexDefinition = VertexDefinition::Create(elements);

    }

    StaticMesh::StaticMesh(float *vertices, uint32_t verticesSize, uint32_t numVertices, std::initializer_list<VertexElement> elements, uint32_t *indices, uint32_t numIndices)
    {
        m_VertexBuffer = VertexBuffer::Create(vertices, verticesSize, numVertices);
        m_VertexDefinition = VertexDefinition::Create(elements);
        m_IndexBuffer = IndexBuffer::Create(indices, numIndices);
    }
}
#include "StaticMesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "glm/glm.hpp"



namespace minEngine
{
    // TODO: Fix this
    StaticMesh::StaticMesh(std::string path)
    {
        
    }

    StaticMesh::StaticMesh(float *vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements)
    {
        // TODO: Use RHI factory to create buffers. This is an important change
        m_VertexBuffer = std::make_shared<OpenGLVertexBuffer>(vertices, vertexSize);
        m_VertexDefinition = std::make_shared<OpenGLVertexArrayObject>(elements);
    }

    StaticMesh::StaticMesh(float *vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements, uint32_t *indices, uint32_t indexCount)
    {
        // TODO: Use RHI factory to create buffers. This is an important change
        m_VertexBuffer = std::make_shared<OpenGLVertexBuffer>(vertices, vertexSize);
        m_VertexDefinition = std::make_shared<OpenGLVertexArrayObject>(elements);
        m_IndexBuffer = std::make_shared<OpenGLIndexBuffer>(indices, indexCount);
    }
}
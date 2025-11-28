#include "Mesh.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "glm/glm.hpp"



namespace minEngine
{
    // TODO: Fix this
    Mesh::Mesh(std::string path)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            ME_CORE_ERROR("Assimp Error: {}", importer.GetErrorString());
            return;
        }

        aiMesh* mesh = scene->mMeshes[0];

        struct Vertex 
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec2 TexCoords;
        };

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
            vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            vertices.push_back(vertex);

            for(unsigned int i = 0; i < mesh->mNumFaces; i++)
            {
                aiFace face = mesh->mFaces[i];
                for (unsigned int j = 0; j < face.mNumIndices; j++)
                {
                    indices.push_back(face.mIndices[j]);
                }
            }
        }

        // Convert to float array for OpenGL
        std::vector<float> vertexData;
        for (const auto& vertex : vertices)
        {
            vertexData.push_back(vertex.Position.x);
            vertexData.push_back(vertex.Position.y);
            vertexData.push_back(vertex.Position.z);

            vertexData.push_back(vertex.Normal.x);
            vertexData.push_back(vertex.Normal.y);
            vertexData.push_back(vertex.Normal.z);

            vertexData.push_back(vertex.TexCoords.x);
            vertexData.push_back(vertex.TexCoords.y);
        }

        m_VertexBuffer = new OpenGLVertexBuffer(vertexData.data(), vertexData.size() * sizeof(float));
        m_IndexBuffer = new OpenGLIndexBuffer(indices.data(), indices.size());

        m_VertexDefinition = new OpenGLVertexArrayObject({
            {"a_Position", VertexElementType::Float3, false},
            {"a_Normal", VertexElementType::Float3, false},
            {"a_TexCoords", VertexElementType::Float2, false}
        });
    }

    Mesh::Mesh(float *vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements)
    {
        m_VertexBuffer = new OpenGLVertexBuffer(vertices, vertexSize);
        m_VertexDefinition = new OpenGLVertexArrayObject(elements);
    }

    Mesh::Mesh(float *vertices, uint32_t vertexSize, std::initializer_list<VertexElement> elements, uint32_t *indices, uint32_t indexCount)
    {
        m_VertexBuffer = new OpenGLVertexBuffer(vertices, vertexSize);
        m_VertexDefinition = new OpenGLVertexArrayObject(elements);
        m_IndexBuffer = new OpenGLIndexBuffer(indices, indexCount);
    }
}
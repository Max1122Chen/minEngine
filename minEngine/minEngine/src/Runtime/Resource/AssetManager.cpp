#include "AssetManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"

#include "stb_image.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/RHIBuffer.h"


namespace minEngine
{
    AssetManager& AssetManager::GetAssetManager()
    {
        return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_AssetManager;
    }

    unsigned char *AssetManager::LoadImage(const std::string &path, int &width, int &height, int &channels, bool bFlip)
    {
        stbi_set_flip_vertically_on_load(bFlip);
        unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 0);
        if (data)
        {
            return data;
        }
        else
        {
            ME_CORE_ERROR("Failed to load image: {}. Failure reason: {}", path, stbi_failure_reason());
            return nullptr;
        }
    }

    void AssetManager::FreeImage(unsigned char *data)
    {
        stbi_image_free(data);
    }

    void AssetManager::LoadStaticMesh(const std::string &path, StaticMesh *outMesh)
    {
        struct Vertex
        {
            Vector3 Position;
            Vector2 TexCoord;
            Vector3 Normal;
        };

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            ME_CORE_ERROR("Assimp failed to load mesh: {}. Failure reason: {}", path, importer.GetErrorString());
            return;
        }

        RuntimeGlobalContext& context = RuntimeGlobalContext::GetRuntimeGlobalContext();
        RHI* rhi = context.m_RenderSystem->GetRHI();
        

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        aiNode* node = scene->mRootNode;
        std::queue<aiNode*> nodeQueue;
        nodeQueue.push(node);
        while(!nodeQueue.empty())
        {
            aiNode* node = nodeQueue.front();
            nodeQueue.pop();

            for (unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

                // vertices.resize(vertices.size() + mesh->mNumVertices); // should we resize the vector first?

                StaticMeshSectionInfo sectionInfo;
                sectionInfo.FirstIndex = static_cast<uint32_t>(indices.size());

                uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

                // Fill vertices
                for(unsigned int j = 0; j < mesh->mNumVertices; j++)
                {
                    Vertex vertex;
                    vertex.Position = Vector3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
                    vertex.Normal = Vector3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                    if(mesh->mTextureCoords[0])
                    {
                        vertex.TexCoord = Vector2(mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y);
                    }
                    else
                    {
                        vertex.TexCoord = Vector2(0.0f, 0.0f);
                    }
                    vertices.push_back(vertex);
                }

                // Fill indices
                unsigned int NumIndices = 0;
                for(unsigned int j = 0; j < mesh->mNumFaces; j++)
                {
                    aiFace face = mesh->mFaces[j];
                    NumIndices += face.mNumIndices;
                    for(unsigned int k = 0; k < face.mNumIndices; k++)
                    {
                        indices.push_back(face.mIndices[k] + baseVertex);   // Since we are merging multiple meshes, need to offset by baseVertex.( Assimp's indices are relative to each mesh )
                    }
                }
                sectionInfo.NumIndices = NumIndices;

                // TODO: handle material loading later
                if(mesh->mMaterialIndex >= 0)
                {
                    sectionInfo.MaterialIndex = static_cast<int32_t>(mesh->mMaterialIndex);
                    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
                }
                
                outMesh->m_Sections.push_back(sectionInfo);
            }


            
            for(unsigned int i = 0; i < node->mNumChildren; i++)
            {
                nodeQueue.push(node->mChildren[i]);
            }
        }

        // Create vertex buffer
        outMesh->m_VertexBuffer = VertexBuffer::Create(reinterpret_cast<float*>(vertices.data()),
                                                       static_cast<uint32_t>(vertices.size() * sizeof(Vertex)),
                                                       static_cast<uint32_t>(vertices.size()));
        outMesh->m_VertexDefinition = VertexDefinition::Create(
            {
                VertexElement("a_Position", VertexElementType::Float3),
                VertexElement("a_TexCoord", VertexElementType::Float2),
                VertexElement("a_Normal", VertexElementType::Float3)
            });
        outMesh->m_IndexBuffer = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
        
    }
}

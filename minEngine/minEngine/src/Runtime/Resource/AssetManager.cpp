#include "AssetManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"

#include "stb_image.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"


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

    void AssetManager::Shutdown()
    {
        m_LoadedTexture2DCache.clear();
        m_LoadedStaticMeshCache.clear();
    }

    std::shared_ptr<Texture2D> AssetManager::LoadTexture2D(const std::string &path, uint32_t unit)
    {
        const std::string cacheKey = path + "#" + std::to_string(unit);
        auto cached = m_LoadedTexture2DCache.find(cacheKey);
        if (cached != m_LoadedTexture2DCache.end())
        {
            return cached->second;
        }

        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* data = LoadImage(path, width, height, channels);
        if (!data)
        {
            return nullptr;
        }

        RHI* rhi = RenderSystem::GetRenderSystem().GetRHI();
        if (!rhi)
        {
            FreeImage(data);
            return nullptr;
        }

        auto texture = std::make_shared<Texture2D>();
        texture->m_Width = static_cast<uint32_t>(width);
        texture->m_Height = static_cast<uint32_t>(height);
        texture->m_Channels = static_cast<uint32_t>(channels);
        texture->m_RHITexture = rhi->CreateRHITexture2D(data, RHITextureDesc{
            .Width = texture->m_Width,
            .Height = texture->m_Height,
            .Format = (channels == 4) ? TextureFormat::RGBA8 : TextureFormat::RGB8,
            .Usage = TextureUsage::TextureBinding
        }, static_cast<int>(unit));

        FreeImage(data);

        m_LoadedTexture2DCache[cacheKey] = texture;
        return texture;
    }

    std::shared_ptr<StaticMesh> AssetManager::LoadStaticMesh(const std::string &path)
    {
        // Check if the static mesh has already been loaded
        auto it = m_LoadedStaticMeshCache.find(path);
        if (it != m_LoadedStaticMeshCache.end())
        {
            // Mesh already loaded, return the cached version
            return it->second;
        }

        auto outMesh = std::make_shared<StaticMesh>();
        outMesh->m_Path = path;

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
            return nullptr;
        }
        

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

        // TODO: change this to a RHICommand later
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
        
        // Cache the loaded static mesh
        m_LoadedStaticMeshCache[path] = outMesh;
        return outMesh;
    }
}

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

#include "AssetMeta.h"

#include <algorithm>
#include <cctype>


namespace minEngine
{
    namespace
    {
        std::string ToLowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        bool IsLikelySceneJson(const std::filesystem::path& path)
        {
            std::string fileName = ToLowerCopy(path.filename().string());
            constexpr const char* suffix = ".scene.json";
            constexpr size_t suffixLen = 11;
            if (fileName.size() < suffixLen)
            {
                return false;
            }

            return fileName.compare(fileName.size() - suffixLen, suffixLen, suffix) == 0;
        }
    }

    AssetManager& AssetManager::Get()
    {
        return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_AssetManager;
    }

    void AssetManager::Initialize()
    {}

    void AssetManager::Shutdown()
    {
        m_AssetPathByGuid.clear();
        m_AssetRegistry.clear();
        m_LoadedTexture2DCache.clear();
        m_LoadedStaticMeshCache.clear();
    }

    void AssetManager::ScanAssets(const std::string &directory)
    {
        const std::filesystem::path rootPath = std::filesystem::absolute(std::filesystem::path(directory)).lexically_normal();
        if (!std::filesystem::exists(rootPath))
        {
            ME_CORE_WARN("Skip scanning assets because directory does not exist: {}", rootPath.string());
            return;
        }

        if (!std::filesystem::is_directory(rootPath))
        {
            ME_CORE_WARN("Skip scanning assets because path is not a directory: {}", rootPath.string());
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(rootPath))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path assetPath = entry.path().lexically_normal();
            const std::string assetType = InferAssetType(assetPath);
            if (assetType.empty())
            {
                continue;
            }

            RegisterAsset(assetPath.string(), assetType);
        }
    }

    void AssetManager::RegisterAsset(const std::string &path, const std::string &type)
    {
        const std::string normalizedPath = NormalizeAssetPath(path);
        if (normalizedPath.empty())
        {
            return;
        }

        const bool alreadyRegistered = (m_AssetRegistry.find(normalizedPath) != m_AssetRegistry.end());

        std::filesystem::path assetPath(normalizedPath);
        const std::filesystem::path metaPath = BuildMetaPath(assetPath);

        AssetMeta meta;
        bool loadedExistingMeta = false;
        if (std::filesystem::exists(metaPath))
        {
            loadedExistingMeta = LoadMetaFromDisk(metaPath, meta);
            if (!loadedExistingMeta)
            {
                ME_CORE_WARN("Failed to parse asset meta, regenerate it: {}", metaPath.string());
            }
        }

        if (!loadedExistingMeta)
        {
            meta.AssetPath = normalizedPath;
            meta.AssetType = type;
            meta.Guid = GenerateGUID();

            if (!SaveMetaToDisk(metaPath, meta))
            {
                ME_CORE_WARN("Failed to save new asset meta: {}", metaPath.string());
            }
        }
        else
        {
            bool needsRewrite = false;
            if (meta.AssetPath != normalizedPath)
            {
                meta.AssetPath = normalizedPath;
                needsRewrite = true;
            }

            if (meta.AssetType.empty() || meta.AssetType != type)
            {
                meta.AssetType = type;
                needsRewrite = true;
            }

            if (meta.Guid.High == 0 && meta.Guid.Low == 0)
            {
                meta.Guid = GenerateGUID();
                needsRewrite = true;
            }

            if (needsRewrite && !SaveMetaToDisk(metaPath, meta))
            {
                ME_CORE_WARN("Failed to rewrite asset meta: {}", metaPath.string());
            }
        }

        auto guidIt = m_AssetPathByGuid.find(meta.Guid);
        if (guidIt != m_AssetPathByGuid.end() && guidIt->second != normalizedPath)
        {
            ME_CORE_WARN("GUID collision detected between '{}' and '{}'. Regenerating GUID for current asset.", guidIt->second, normalizedPath);
            meta.Guid = GenerateGUID();
            if (!SaveMetaToDisk(metaPath, meta))
            {
                ME_CORE_WARN("Failed to save regenerated GUID to asset meta: {}", metaPath.string());
            }
        }

        CacheMeta(meta);

        ME_CORE_INFO("Asset {}: type='{}', path='{}', guid='{}'",
                     alreadyRegistered ? "updated" : "registered",
                     meta.AssetType,
                     meta.AssetPath,
                     meta.Guid.ToString());
    }

    const AssetMeta* AssetManager::FindAssetMetaByPath(const std::string& path) const
    {
        const std::string normalizedPath = NormalizeAssetPath(path);
        auto iter = m_AssetRegistry.find(normalizedPath);
        if (iter == m_AssetRegistry.end())
        {
            return nullptr;
        }

        return &iter->second;
    }

    const AssetMeta* AssetManager::FindAssetMetaByGuid(const GUID& guid) const
    {
        auto guidIter = m_AssetPathByGuid.find(guid);
        if (guidIter == m_AssetPathByGuid.end())
        {
            return nullptr;
        }

        auto pathIter = m_AssetRegistry.find(guidIter->second);
        if (pathIter == m_AssetRegistry.end())
        {
            return nullptr;
        }

        return &pathIter->second;
    }

    std::shared_ptr<StaticMesh> AssetManager::LoadStaticMeshByMeta(const AssetMeta& meta)
    {
        if (meta.AssetType != "StaticMesh")
        {
            ME_CORE_WARN("Asset type mismatch. Expected StaticMesh but got '{}': {}", meta.AssetType, meta.AssetPath);
            return nullptr;
        }

        return LoadStaticMesh(meta.AssetPath);
    }

    std::shared_ptr<Texture2D> AssetManager::LoadTexture2DByMeta(const AssetMeta& meta, uint32_t unit)
    {
        if (meta.AssetType != "Texture2D")
        {
            ME_CORE_WARN("Asset type mismatch. Expected Texture2D but got '{}': {}", meta.AssetType, meta.AssetPath);
            return nullptr;
        }

        return LoadTexture2D(meta.AssetPath, unit);
    }

    std::shared_ptr<StaticMesh> AssetManager::LoadStaticMeshByGuid(const GUID& guid)
    {
        const AssetMeta* meta = FindAssetMetaByGuid(guid);
        if (meta == nullptr)
        {
            return nullptr;
        }

        return LoadStaticMeshByMeta(*meta);
    }

    std::shared_ptr<Texture2D> AssetManager::LoadTexture2DByGuid(const GUID& guid, uint32_t unit)
    {
        const AssetMeta* meta = FindAssetMetaByGuid(guid);
        if (meta == nullptr)
        {
            return nullptr;
        }

        return LoadTexture2DByMeta(*meta, unit);
    }

    std::string AssetManager::NormalizeAssetPath(const std::string& path) const
    {
        if (path.empty())
        {
            return std::string();
        }

        const std::filesystem::path input(path);
        const std::filesystem::path absolutePath = input.is_absolute()
            ? input
            : std::filesystem::absolute(input);

        return absolutePath.lexically_normal().generic_string();
    }

    std::string AssetManager::InferAssetType(const std::filesystem::path& path) const
    {
        const std::string extension = ToLowerCopy(path.extension().string());
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
        {
            return "Texture2D";
        }

        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf")
        {
            return "StaticMesh";
        }

        if (IsLikelySceneJson(path))
        {
            return "Scene";
        }

        return std::string();
    }

    std::filesystem::path AssetManager::BuildMetaPath(const std::filesystem::path& assetPath) const
    {
        return std::filesystem::path(assetPath.string() + ".meta");
    }

    bool AssetManager::LoadMetaFromDisk(const std::filesystem::path& metaPath, AssetMeta& outMeta) const
    {
        std::ifstream input(metaPath);
        if (!input.is_open())
        {
            return false;
        }

        Json metaJson;
        try
        {
            input >> metaJson;
        }
        catch (const std::exception&)
        {
            return false;
        }

        if (!metaJson.is_object())
        {
            return false;
        }

        if (!metaJson.contains("assetPath") || !metaJson.contains("assetType") || !metaJson.contains("guid"))
        {
            return false;
        }

        const Json& guidJson = metaJson["guid"];
        if (!guidJson.is_object() || !guidJson.contains("high") || !guidJson.contains("low"))
        {
            return false;
        }

        outMeta.AssetPath = metaJson["assetPath"].get<std::string>();
        outMeta.AssetType = metaJson["assetType"].get<std::string>();
        outMeta.Guid.High = guidJson["high"].get<uint64_t>();
        outMeta.Guid.Low = guidJson["low"].get<uint64_t>();

        return !outMeta.AssetPath.empty() && !outMeta.AssetType.empty();
    }

    bool AssetManager::SaveMetaToDisk(const std::filesystem::path& metaPath, const AssetMeta& meta) const
    {
        Json metaJson;
        metaJson["version"] = 1;
        metaJson["assetPath"] = meta.AssetPath;
        metaJson["assetType"] = meta.AssetType;
        metaJson["guid"] = {
            {"high", meta.Guid.High},
            {"low", meta.Guid.Low}
        };

        std::ofstream output(metaPath, std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }

        output << metaJson.dump(4);
        return output.good();
    }

    void AssetManager::CacheMeta(const AssetMeta& meta)
    {
        m_AssetRegistry[meta.AssetPath] = meta;
        m_AssetPathByGuid[meta.Guid] = meta.AssetPath;
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

        RHI* rhi = RenderSystem::Get().GetRHI();
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
        const aiScene* scene = importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals);

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
                if (mesh == nullptr)
                {
                    ME_CORE_WARN("Assimp returned a null mesh pointer while loading '{}'.", path);
                    continue;
                }

                if (!mesh->HasPositions() || mesh->mVertices == nullptr)
                {
                    ME_CORE_WARN("Skip mesh without positions while loading '{}'.", path);
                    continue;
                }

                const bool hasNormals = mesh->HasNormals() && mesh->mNormals != nullptr;
                const bool hasTexCoords = mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr;

                // vertices.resize(vertices.size() + mesh->mNumVertices); // should we resize the vector first?

                StaticMeshSectionInfo sectionInfo;
                sectionInfo.FirstIndex = static_cast<uint32_t>(indices.size());

                uint32_t baseVertex = static_cast<uint32_t>(vertices.size());

                // Fill vertices
                for(unsigned int j = 0; j < mesh->mNumVertices; j++)
                {
                    Vertex vertex;
                    vertex.Position = Vector3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
                    if (hasNormals)
                    {
                        vertex.Normal = Vector3(mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z);
                    }
                    else
                    {
                        vertex.Normal = Vector3(0.0f, 0.0f, 0.0f);
                    }

                    if (hasTexCoords)
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
                    for(unsigned int k = 0; k < face.mNumIndices; k++)
                    {
                        if (face.mIndices[k] >= mesh->mNumVertices)
                        {
                            ME_CORE_WARN("Skip invalid face index while loading '{}'. meshVertexCount={}, index={}",
                                         path,
                                         mesh->mNumVertices,
                                         face.mIndices[k]);
                            continue;
                        }

                        indices.push_back(face.mIndices[k] + baseVertex);   // Since we are merging multiple meshes, need to offset by baseVertex.( Assimp's indices are relative to each mesh )
                        ++NumIndices;
                    }
                }
                sectionInfo.NumIndices = NumIndices;

                if (!hasNormals)
                {
                    // Fallback: accumulate face normals and normalize per vertex.
                    for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
                    {
                        const aiFace& face = mesh->mFaces[j];
                        if (face.mNumIndices < 3)
                        {
                            continue;
                        }

                        const uint32_t i0 = baseVertex + face.mIndices[0];
                        const uint32_t i1 = baseVertex + face.mIndices[1];
                        const uint32_t i2 = baseVertex + face.mIndices[2];

                        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                        {
                            continue;
                        }

                        const Vector3& p0 = vertices[i0].Position;
                        const Vector3& p1 = vertices[i1].Position;
                        const Vector3& p2 = vertices[i2].Position;

                        const Vector3 edge01 = p1 - p0;
                        const Vector3 edge02 = p2 - p0;
                        Vector3 faceNormal = glm::cross(edge01, edge02);
                        const float faceNormalLen2 = glm::dot(faceNormal, faceNormal);
                        if (faceNormalLen2 <= 1e-12f)
                        {
                            continue;
                        }

                        faceNormal = glm::normalize(faceNormal);
                        vertices[i0].Normal += faceNormal;
                        vertices[i1].Normal += faceNormal;
                        vertices[i2].Normal += faceNormal;
                    }

                    for (uint32_t j = 0; j < mesh->mNumVertices; ++j)
                    {
                        Vector3& normal = vertices[baseVertex + j].Normal;
                        const float normalLen2 = glm::dot(normal, normal);
                        if (normalLen2 <= 1e-12f)
                        {
                            normal = Vector3(0.0f, 1.0f, 0.0f);
                        }
                        else
                        {
                            normal = glm::normalize(normal);
                        }
                    }

                    ME_CORE_WARN("Mesh '{}' has no normal data. Fallback normals were generated in AssetManager.", path);
                }

                // TODO: handle material loading later
                if(mesh->mMaterialIndex >= 0)
                {
                    sectionInfo.MaterialIndex = static_cast<int32_t>(mesh->mMaterialIndex);
                }
                
                outMesh->m_Sections.push_back(sectionInfo);
            }


            
            for(unsigned int i = 0; i < node->mNumChildren; i++)
            {
                nodeQueue.push(node->mChildren[i]);
            }
        }

        if (vertices.empty())
        {
            ME_CORE_ERROR("Failed to load mesh '{}': no valid vertices were produced.", path);
            return nullptr;
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

#include "AssetManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "stb_image.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Core/Math/Math.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHITexture.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Resource/AssetResources/ShaderResource.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include "AssetMeta.h"

#include <algorithm>
#include <cctype>


namespace minEngine
{
    AssetManager& AssetManager::Get()
    {
        return *RuntimeGlobalContext::Get().m_AssetManager;
    }

    void AssetManager::Initialize()
    {}

    void AssetManager::Shutdown()
    {
        m_AssetPathByGuid.clear();
        m_AssetRegistry.clear();
        m_LoadedAssetCache.clear();
    }

    // TODO: currently "directory" should be an absolute path, but maybe we should also support relative path and resolve it to absolute path internally 
    void AssetManager::ScanAssets(const std::filesystem::path &directory)
    {
        if (!std::filesystem::exists(directory))
        {
            ME_CORE_WARN("Skip scanning assets because directory does not exist: {}", directory.string());
            return;
        }

        if (!std::filesystem::is_directory(directory))
        {
            ME_CORE_WARN("Skip scanning assets because path is not a directory: {}", directory.string());
            return;
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const std::filesystem::path assetPath = entry.path().lexically_normal();
            const std::string assetType = InferAssetTypeFromExtension(assetPath);
            if (assetType.empty())
            {
                continue;
            }

            AssetMeta meta = RegisterAsset(assetPath.string(), assetType);

            // TODO: maybe later we should register different types of assets to different subsystems (e.g., register scene assets to scene manager
            // If the asset is a scene, also register it to the scene manager
            if (assetType == "Scene")
            { 
                SceneManager::Get().RegisterScene(meta.AssetName, meta.AssetPath);
            }
        }
    }

    AssetMeta AssetManager::RegisterAsset(const std::string &path, const std::string &type)
    {
        const std::string normalizedPath = NormalizeAssetPath(path);
        if (normalizedPath.empty())
        {
            return AssetMeta();
        }

        const bool alreadyRegistered = (m_AssetRegistry.find(normalizedPath) != m_AssetRegistry.end());

        std::filesystem::path assetPath(normalizedPath);
        const std::filesystem::path metaPath = BuildMetaPath(assetPath);
        const std::string inferredAssetName = assetPath.stem().string();

        const Serialization::SerializerOptions metaSerializerOptions{
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = true,
            .allowObjectPtrSerialization = false
        };

        auto loadMetaFromFile = [&](AssetMeta& outMeta) -> bool
        {
            Serialization::JsonReaderArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
                metaPath.string(),
                minEngine::Reflection::GetClassName<AssetMeta>(),
                &outMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_CORE_WARN("Failed to deserialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                             result.message,
                             result.fieldPath,
                             metaPath.string());
                return false;
            }

            return true;
        };

        auto saveMetaToFile = [&](const AssetMeta& inMeta) -> bool
        {
            Serialization::JsonWriterArchive archive;
            const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
                metaPath.string(),
                minEngine::Reflection::GetClassName<AssetMeta>(),
                &inMeta,
                archive,
                metaSerializerOptions);

            if (!result.ok)
            {
                ME_CORE_WARN("Failed to serialize asset meta. Error: {}. Field path: {}. Meta file: {}",
                             result.message,
                             result.fieldPath,
                             metaPath.string());
                return false;
            }

            return true;
        };

        AssetMeta meta;
        bool loadedExistingMeta = false;
        if (std::filesystem::exists(metaPath))
        {
            loadedExistingMeta = loadMetaFromFile(meta);
            if (!loadedExistingMeta)
            {
                ME_CORE_WARN("Failed to parse asset meta, regenerate it: {}", metaPath.string());
            }
        }

        if (!loadedExistingMeta)
        {
            meta.AssetName = inferredAssetName;
            meta.AssetPath = normalizedPath;
            meta.AssetType = type;
            meta.Guid = GenerateGUID();

            if (!saveMetaToFile(meta))
            {
                ME_CORE_WARN("Failed to save new asset meta: {}", metaPath.string());
            }
        }
        else
        {
            bool needsRewrite = false;
            if (meta.AssetName.empty())
            {
                meta.AssetName = inferredAssetName;
                needsRewrite = true;
            }

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

            if (needsRewrite && !saveMetaToFile(meta))
            {
                ME_CORE_WARN("Failed to rewrite asset meta: {}", metaPath.string());
            }
        }

        auto guidIt = m_AssetPathByGuid.find(meta.Guid);
        if (guidIt != m_AssetPathByGuid.end() && guidIt->second != normalizedPath)
        {
            ME_CORE_WARN("GUID collision detected between '{}' and '{}'. Regenerating GUID for current asset.", guidIt->second, normalizedPath);
            meta.Guid = GenerateGUID();
            if (!saveMetaToFile(meta))
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

        return meta;
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByPath(const std::string &path, std::string &outErrorMessage)
    {
        outErrorMessage.clear();

        const AssetMeta* meta = FindAssetMetaByPath(path);
        if (meta == nullptr)        
        {
            outErrorMessage = "asset meta not found for path: " + path;
            return nullptr;
        }
        return LoadAssetByMeta_Internal(*meta, outErrorMessage);
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta(const AssetMeta &meta, std::string &outErrorMessage)
    {
        outErrorMessage.clear();
        return LoadAssetByMeta_Internal(meta, outErrorMessage);
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByGUID(const GUID& guid, std::string& outErrorMessage)
    {
        outErrorMessage.clear();

        const AssetMeta* assetMeta = FindAssetMetaByGuid(guid);
        if (assetMeta == nullptr)
        {
            outErrorMessage = "guid not found in object manager or asset registry";
            return nullptr;
        }

        return LoadAssetByMeta_Internal(*assetMeta, outErrorMessage);
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

    std::vector<AssetMeta *> AssetManager::FindAssetMetasByType(const std::string &type) const
    {
        std::vector<AssetMeta*> result;
        for (const auto& pair : m_AssetRegistry)
        {
            if (pair.second.AssetType == InferAssetTypeFromClassName(type))
            {
                result.push_back(const_cast<AssetMeta*>(&pair.second));
            }
        }
        return result;
    }

    std::shared_ptr<Asset> AssetManager::LoadAssetByMeta_Internal(const AssetMeta &meta, std::string &outErrorMessage)
    {
        if(meta.AssetType == "StaticMesh")
        {
            std::shared_ptr<StaticMesh> asset = LoadAsset<StaticMesh>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load static mesh by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if(meta.AssetType == "Texture2D")
        {
            std::shared_ptr<Texture2D> asset = LoadAsset<Texture2D>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load texture2d by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Scene")
        {
            std::shared_ptr<Scene> asset = LoadAsset<Scene>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load scene by guid";
                return nullptr;
            }

            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Material")
        {
            std::shared_ptr<Material> asset = LoadAsset<Material>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load material by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        if (meta.AssetType == "Shader")
        {
            std::shared_ptr<Shader> asset = LoadAsset<Shader>(meta.AssetPath);
            if (asset == nullptr)
            {
                outErrorMessage = "failed to load shader by guid";
                return nullptr;
            }
            return std::static_pointer_cast<Asset>(asset);
        }

        outErrorMessage = "unsupported asset type '" + meta.AssetType + "'";
        return nullptr;
    }

    std::string AssetManager::NormalizeAssetPath(const std::string &path) const
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

    std::string AssetManager::InferAssetTypeFromExtension(const std::filesystem::path& path) const
    {
        const std::string extension = path.extension().string();
        if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
        {
            return "Texture2D";
        }

        if (extension == ".obj" || extension == ".fbx" || extension == ".gltf")
        {
            return "StaticMesh";
        }

        if (extension == ".memtl")
        {
            return "Material";
        }

        if(extension == ".meshader")
        {
            return "Shader";
        }

        if (extension == ".mescene")
        {
            return "Scene";
        }

        return std::string();
    }

    std::string AssetManager::InferAssetTypeFromClassName(const std::string &className) const
    {
        using namespace minEngine::Reflection;
        if (className == GetClassName<Scene>())
        {
            return "Scene";
        }

        if (className == GetClassName<StaticMesh>())
        {
            return "StaticMesh";
        }

        if (className == GetClassName<Texture2D>())
        {
            return "Texture2D";
        }

        if (className == GetClassName<Material>())
        {
            return "Material";
        }

        if (className == GetClassName<Shader>())
        {
            return "Shader";
        }

        return std::string();
    }

    std::filesystem::path AssetManager::BuildMetaPath(const std::filesystem::path& assetPath) const
    {
        return std::filesystem::path(assetPath.string() + ".meta");
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

    template<>
    std::shared_ptr<Scene> AssetManager::LoadAsset_Impl<Scene>(const AssetMeta& meta)
    {
        std::shared_ptr<Scene> scene = NewObject<Scene>(meta.AssetName, nullptr, meta.Guid);
        scene->Reset();
        scene->m_SceneName = meta.AssetName;

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            meta.AssetPath,
            minEngine::Reflection::GetClassName<Scene>(),
            scene.get(),
            archive,
            Serialization::SerializerOptions {
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
            .allowObjectPtrSerialization = true
        });

        if (!result.ok)
        {
            ObjectManager::Get().UnregisterObject(scene.get());
            ME_CORE_ERROR("Failed to deserialize scene '{}'. Error: {}. Field path: {}",
                          meta.AssetPath,
                          result.message,
                          result.fieldPath);
            return nullptr;
        }

        scene->RebuildRuntimeGameObjectIndex();

        return scene;
    }

    
    template<>
    std::shared_ptr<StaticMesh> AssetManager::LoadAsset_Impl<StaticMesh>(const AssetMeta& meta)
    {
        std::shared_ptr<StaticMesh> outMesh = NewObject<StaticMesh>(meta.AssetName, nullptr, meta.Guid);

        struct Vertex
        {
            Vector3 Position;
            Vector2 TexCoord;
            Vector3 Normal;
        };

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            meta.AssetPath.c_str(),
            aiProcess_Triangulate |
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            ME_CORE_ERROR("Assimp failed to load mesh: {}. Failure reason: {}", meta.AssetPath, importer.GetErrorString());
            return nullptr;
        }
        

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        Geometry::AABB boundingBox;
        boundingBox.Min = Vector3(std::numeric_limits<float>::max());
        boundingBox.Max = Vector3(std::numeric_limits<float>::lowest());


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
                    ME_CORE_WARN("Assimp returned a null mesh pointer while loading '{}'.", meta.AssetPath);
                    continue;
                }

                if (!mesh->HasPositions() || mesh->mVertices == nullptr)
                {
                    ME_CORE_WARN("Skip mesh without positions while loading '{}'.", meta.AssetPath);
                    continue;
                }

                const bool hasNormals = mesh->HasNormals() && mesh->mNormals != nullptr;
                const bool hasTexCoords = mesh->HasTextureCoords(0) && mesh->mTextureCoords[0] != nullptr;

                float minX = 0.0f;
                float maxX = 1.0f;
                float minZ = 0.0f;
                float maxZ = 1.0f;
                if (!hasTexCoords && mesh->mNumVertices > 0)
                {
                    minX = maxX = mesh->mVertices[0].x;
                    minZ = maxZ = mesh->mVertices[0].z;
                    for (unsigned int j = 1; j < mesh->mNumVertices; ++j)
                    {
                        minX = std::min(minX, mesh->mVertices[j].x);
                        maxX = std::max(maxX, mesh->mVertices[j].x);
                        minZ = std::min(minZ, mesh->mVertices[j].z);
                        maxZ = std::max(maxZ, mesh->mVertices[j].z);
                    }
                }

                // Update bounding box
                for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
                {
                    boundingBox.Encapsulate(Vector3(mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z));
                }

                const float uvExtentX = std::max(maxX - minX, 1e-6f);
                const float uvExtentZ = std::max(maxZ - minZ, 1e-6f);

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
                        // Fallback UV projection for meshes without texture coordinates.
                        vertex.TexCoord = Vector2(
                            (vertex.Position.x - minX) / uvExtentX,
                            (vertex.Position.z - minZ) / uvExtentZ);
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
                                         meta.AssetPath,
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

                    ME_CORE_WARN("Mesh '{}' has no normal data. Fallback normals were generated in AssetManager.", meta.AssetPath);
                }

                if (!hasTexCoords)
                {
                    ME_CORE_WARN("Mesh '{}' has no UV data. Fallback planar UVs were generated in AssetManager.", meta.AssetPath);
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
            ME_CORE_ERROR("Failed to load mesh '{}': no valid vertices were produced.", meta.AssetPath);
            return nullptr;
        }

        outMesh->m_BoundingBox = boundingBox;
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
        
        // TODO: Cache the loaded mesh in m_LoadedAssetCache
        return outMesh;
    }

    template<>
    std::shared_ptr<Texture2D> AssetManager::LoadAsset_Impl<Texture2D>(const AssetMeta& meta)
    {
        int width = 0;
        int height = 0;
        int channels = 0;
        unsigned char* data = LoadImage(meta.AssetPath.c_str(), width, height, channels);
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

        std::shared_ptr<Texture2D> texture = NewObject<Texture2D>(meta.AssetName);
        texture->SetGuid(meta.Guid);

        texture->m_Width = static_cast<uint32_t>(width);
        texture->m_Height = static_cast<uint32_t>(height);
        texture->m_Channels = static_cast<uint32_t>(channels);
        texture->m_RHITexture = rhi->CreateRHITexture2D(data, RHITextureDesc{
            .Width = texture->m_Width,
            .Height = texture->m_Height,
            .Format = (channels == 4) ? TextureFormat::RGBA8 : TextureFormat::RGB8,
            .Usage = TextureUsage::TextureBinding
        });

        FreeImage(data);

        return texture;
    }

    template<>
    std::shared_ptr<Material> AssetManager::LoadAsset_Impl<Material>(const AssetMeta& meta)
    {
        std::shared_ptr<Material> material = NewObject<Material>(meta.AssetName, nullptr, meta.Guid);

        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult deserializeResult = Serialization::Serializer::FromFile(
            meta.AssetPath,
            "minEngine::Material",
            material.get(),
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true
            });
        if (!deserializeResult.ok)
        {
            ME_CORE_ERROR("Failed to deserialize material '{}'. Error: {}. Field path: {}",
                          meta.AssetPath,
                          deserializeResult.message,
                          deserializeResult.fieldPath);
            return nullptr;
        }

        std::string graphError;
        if (!material->FinalizeGraphAfterLoad(&graphError))
        {
            ME_CORE_ERROR("Failed to finalize material graph '{}': {}", meta.AssetPath, graphError);
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            ME_CORE_ERROR("Failed to compile material '{}': RHI is not available.", meta.AssetPath);
            return nullptr;
        }

        MaterialCompileContext ctx;
        ctx.RHI = rhi;
        if (!MaterialCompiler::Compile(*material, ctx))
        {
            ME_CORE_ERROR("Failed to compile material '{}'.", meta.AssetPath);
            for (const MaterialCompileDiagnostic& diagnostic : material->m_LastCompileDiagnostics)
            {
                ME_CORE_ERROR("  {}", diagnostic.Message);
            }
            return nullptr;
        }

        return material;
    }

    template<>
    std::shared_ptr<Shader> AssetManager::LoadAsset_Impl<Shader>(const AssetMeta& meta)
    {
        ShaderResource resource;
        Serialization::JsonReaderArchive archive;
        const Serialization::SerializeResult result = Serialization::Serializer::FromFile(
            meta.AssetPath,
            "minEngine::ShaderResource",
            &resource,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true
            });
        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to deserialize shader resource '{}'. Error: {}. Field path: {}",
                          meta.AssetPath,
                          result.message,
                          result.fieldPath);
            return nullptr;
        }
        std::shared_ptr<Shader> shader = NewObject<Shader>(meta.AssetName, nullptr, meta.Guid);
        std::string compileError;
        if (!shader->CompileFromFiles(
                *RenderSystem::Get().GetRHI(),
                resource.m_VertexPath,
                resource.m_FragmentPath,
                &compileError))
        {
            ME_CORE_ERROR(
                "Failed to compile shader asset '{}'. Vertex: '{}', Fragment: '{}'. {}",
                meta.AssetPath,
                resource.m_VertexPath,
                resource.m_FragmentPath,
                compileError);
            return nullptr;
        }
        return shader;
    }


    template<>
    bool AssetManager::SaveAsset_Impl<Material>(const AssetMeta& meta, const Material& asset) const
    {
        Serialization::JsonWriterArchive archive;

        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            meta.AssetPath,
            "minEngine::Material",
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true
            });

        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to serialize material '{}'. Error: {}. Field path: {}",
                          meta.AssetPath,
                          result.message,
                          result.fieldPath);
            return false;
        }

        return true;
    }

    template<>
    bool AssetManager::SaveAsset_Impl<Scene>(const AssetMeta& meta, const Scene& asset) const
    {
        const std::filesystem::path scenePath(meta.AssetPath);

        Serialization::JsonWriterArchive archive;
        
        const Serialization::SerializeResult result = Serialization::Serializer::ToFile(
            meta.AssetPath,
            "minEngine::Scene",
            &asset,
            archive,
            Serialization::SerializerOptions{
                .enumAsString = true,
                .strictTypeCheck = true,
                .skipUnknownField = false,
                .allowObjectPtrSerialization = true
            });

        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to serialize scene '{}'. Error: {}. Field path: {}",
                          meta.AssetPath,
                          result.message,
                          result.fieldPath);
            return false;
        }

        return true;
    }

    
}

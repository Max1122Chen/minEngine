#include "AssetManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Core/Serialization/JsonArchive.h"

#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/StaticMesh.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Shader.h"
#include "Runtime/Resource/AssetResources/ShaderResource.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Core/Object/ObjectManager.h"

#include "AssetMeta.h"

#include <algorithm>
#include <cctype>


namespace minEngine
{
    AssetManager* AssetManager::s_Instance = nullptr;

    void AssetManager::SetInstance(AssetManager* instance)
    {
        s_Instance = instance;
    }

    AssetManager& AssetManager::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "AssetManager is not initialized");
        return *s_Instance;
    }

    bool AssetManager::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void AssetManager::Initialize()
    {}

    void AssetManager::Shutdown()
    {
        m_AssetPathByGuid.clear();
        m_AssetRegistry.clear();
        m_LoadedAssetCache.clear();
    }

    void AssetManager::MarkReachableLoadedAssets(const std::function<void(MEObject*)>& markReachable) const
    {
        for (const auto& [path, weakAsset] : m_LoadedAssetCache)
        {
            (void)path;
            const std::shared_ptr<MEObject> loadedObject = weakAsset.lock();
            if (!loadedObject)
            {
                continue;
            }

            markReachable(loadedObject.get());

            Scene* sceneAsset = dynamic_cast<Scene*>(loadedObject.get());
            if (sceneAsset != nullptr)
            {
                sceneAsset->MarkReachableObjects(markReachable);
            }
        }
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

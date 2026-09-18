#include "Services/Asset/AssetEditorServices.h"

#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "SubEditor/Scene/SceneEditor.h"

#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <algorithm>
#include <string>

namespace minEngine
{
    AssetEditorResult AssetEditorServices::OkResult()
    {
        AssetEditorResult result;
        result.Ok = true;
        return result;
    }

    AssetEditorResult AssetEditorServices::Fail(std::string message)
    {
        AssetEditorResult result;
        result.Ok = false;
        result.Message = std::move(message);
        return result;
    }

    AssetEditorResult AssetEditorServices::List(
        std::string_view directory,
        std::string_view assetType,
        std::vector<const AssetMeta*>& outMetas) const
    {
        outMetas.clear();
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        AssetManager& assets = AssetManager::Get();
        const std::string directoryStorage = directory.empty() ? std::string("Assets") : std::string(directory);
        std::vector<const AssetMeta*> found = assetType.empty()
            ? assets.FindAssetMetasUnderDirectory(directoryStorage)
            : assets.FindAssetMetasByType(std::string(assetType));

        if (!assetType.empty() && !directory.empty())
        {
            const std::string prefix = directoryStorage;
            found.erase(
                std::remove_if(
                    found.begin(),
                    found.end(),
                    [&](const AssetMeta* meta)
                    {
                        if (meta == nullptr)
                        {
                            return true;
                        }
                        return meta->AssetPath.rfind(prefix, 0) != 0;
                    }),
                found.end());
        }

        outMetas = std::move(found);
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Create(
        std::string_view assetType,
        std::string_view directory,
        std::string_view name,
        std::string& outPath,
        std::string& outGuid)
    {
        outPath.clear();
        outGuid.clear();
        if (directory.empty() || name.empty() || assetType.empty())
        {
            return Fail("assetType, directory, and name are required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        const std::string typeStorage(assetType);
        const std::string directoryStorage(directory);
        const std::string nameStorage(name);
        GUID createdGuid;
        bool created = false;

        if (typeStorage == "Scene")
        {
            if (std::shared_ptr<Scene> asset = AssetManager::Get().CreateAsset<Scene>(nameStorage, directoryStorage))
            {
                createdGuid = asset->GetGuid();
                created = true;
            }
        }
        else if (typeStorage == "Material")
        {
            if (std::shared_ptr<Material> asset =
                    AssetManager::Get().CreateAsset<Material>(nameStorage, directoryStorage))
            {
                createdGuid = asset->GetGuid();
                created = true;
            }
        }
        else if (typeStorage == "AnimationGraph")
        {
            if (std::shared_ptr<AnimationGraph> asset =
                    AssetManager::Get().CreateAsset<AnimationGraph>(nameStorage, directoryStorage))
            {
                createdGuid = asset->GetGuid();
                created = true;
            }
        }
        else
        {
            return Fail("assetType '" + typeStorage + "' is not in the create whitelist.");
        }

        if (!created)
        {
            return Fail("CreateAsset failed.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(createdGuid);
        if (meta == nullptr)
        {
            return Fail("Created asset has no registry meta.");
        }
        outPath = meta->AssetPath;
        outGuid = meta->Guid.ToString();
        return OkResult();
    }

    AssetEditorResult AssetEditorServices::Delete(IEditorContext& context, std::string_view assetPath)
    {
        if (assetPath.empty())
        {
            return Fail("path is required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail("AssetManager is not available.");
        }

        const std::string pathStorage(assetPath);
        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(pathStorage);
        if (meta == nullptr)
        {
            return Fail("Asset not found.");
        }

        if (meta->AssetType == "Prefab")
        {
            if (SceneEditor* sceneEditor = GetSceneEditor(&context))
            {
                if (sceneEditor->GetPrefabStages().HasStage(pathStorage))
                {
                    return Fail("Close the Prefab stage before deleting this asset.");
                }
            }
        }

        std::string errorMessage;
        if (!AssetManager::Get().DeleteAsset(pathStorage, errorMessage, false))
        {
            return Fail(errorMessage.empty() ? "DeleteAsset failed." : errorMessage);
        }
        return OkResult();
    }
}

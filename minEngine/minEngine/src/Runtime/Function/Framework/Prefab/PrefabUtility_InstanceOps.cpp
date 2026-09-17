#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"

#include <algorithm>

namespace minEngine
{
    PrefabInstanceRecord* PrefabUtility::FindInstanceRecord(Scene& scene, const GUID& instanceObjectGuid)
    {
        for (PrefabInstanceRecord& record : scene.GetPrefabInstancesMutable())
        {
            if (record.RootInstanceGuid == instanceObjectGuid)
            {
                return &record;
            }

            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                if (mapping.InstanceGuid == instanceObjectGuid)
                {
                    return &record;
                }
            }
        }

        return nullptr;
    }

    const PrefabInstanceRecord* PrefabUtility::FindInstanceRecord(
        const Scene& scene,
        const GUID& instanceObjectGuid)
    {
        return FindInstanceRecord(const_cast<Scene&>(scene), instanceObjectGuid);
    }

    std::vector<PrefabInstanceRef> PrefabUtility::FindInstanceRefsInScene(
        Scene& scene,
        const GUID& prefabAssetGuid)
    {
        std::vector<PrefabInstanceRef> refs;
        if (prefabAssetGuid.IsZero())
        {
            return refs;
        }

        for (const PrefabInstanceRecord& record : scene.GetPrefabInstances())
        {
            if (record.PrefabAssetGuid != prefabAssetGuid)
            {
                continue;
            }

            PrefabInstanceRef ref;
            ref.Scene = &scene;
            ref.RootInstanceGuid = record.RootInstanceGuid;
            ref.SceneName = scene.GetSceneName();

            if (ObjectManager::HasInstance())
            {
                if (std::shared_ptr<MEObject> rootObject =
                        ObjectManager::Get().FindObject(record.RootInstanceGuid))
                {
                    if (GameObject* rootGo = dynamic_cast<GameObject*>(rootObject.get()))
                    {
                        ref.RootName = rootGo->GetName();
                    }
                }
            }
            if (ref.RootName.empty())
            {
                ref.RootName = record.RootInstanceGuid.ToString();
            }

            refs.push_back(std::move(ref));
        }

        return refs;
    }

    std::vector<PrefabInstanceRef> PrefabUtility::FindInstanceRefsInEditorScene(const GUID& prefabAssetGuid)
    {
        std::vector<PrefabInstanceRef> refs;
        if (!SceneManager::HasInstance())
        {
            return refs;
        }

        if (Scene* editorScene = SceneManager::Get().GetEditorScene())
        {
            refs = FindInstanceRefsInScene(*editorScene, prefabAssetGuid);
        }

        return refs;
    }

    bool PrefabUtility::UnpackInstance(Scene& scene, const GUID& rootInstanceGuid, std::string* outError)
    {
        std::vector<PrefabInstanceRecord>& records = scene.GetPrefabInstancesMutable();
        const auto it = std::find_if(
            records.begin(),
            records.end(),
            [&rootInstanceGuid](const PrefabInstanceRecord& record)
            {
                return record.RootInstanceGuid == rootInstanceGuid;
            });

        if (it == records.end())
        {
            if (outError)
            {
                *outError = "PrefabInstanceRecord not found for root Guid.";
            }
            return false;
        }

        records.erase(it);
        return true;
    }

    bool PrefabUtility::UnpackAllInstancesOfPrefab(
        Scene& scene,
        const GUID& prefabAssetGuid,
        size_t* outUnpackedCount,
        std::string* outError)
    {
        if (outUnpackedCount)
        {
            *outUnpackedCount = 0;
        }

        if (prefabAssetGuid.IsZero())
        {
            if (outError)
            {
                *outError = "Invalid Prefab asset Guid.";
            }
            return false;
        }

        std::vector<PrefabInstanceRecord>& records = scene.GetPrefabInstancesMutable();
        const size_t before = records.size();
        records.erase(
            std::remove_if(
                records.begin(),
                records.end(),
                [&prefabAssetGuid](const PrefabInstanceRecord& record)
                {
                    return record.PrefabAssetGuid == prefabAssetGuid;
                }),
            records.end());

        if (outUnpackedCount)
        {
            *outUnpackedCount = before - records.size();
        }

        return true;
    }

    std::string PrefabUtility::FormatPrefabInstanceRefsMessage(
        const std::string& assetPath,
        const std::vector<PrefabInstanceRef>& refs)
    {
        std::string message = "Cannot delete Prefab '" + assetPath + "': "
            + std::to_string(refs.size())
            + " open-scene instance(s). Unpack instances first, or use Unpack and Delete.";
        for (size_t i = 0; i < refs.size() && i < 8; ++i)
        {
            message += " [";
            message += refs[i].SceneName.empty() ? "?" : refs[i].SceneName;
            message += " / ";
            message += refs[i].RootName.empty() ? refs[i].RootInstanceGuid.ToString() : refs[i].RootName;
            message += "]";
        }
        if (refs.size() > 8)
        {
            message += " ...";
        }
        return message;
    }
}

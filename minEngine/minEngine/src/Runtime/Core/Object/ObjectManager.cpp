#include "ObjectManager.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/AssetManager.h"

#include <unordered_set>

namespace minEngine
{
    ObjectManager* ObjectManager::s_Instance = nullptr;

    void ObjectManager::SetInstance(ObjectManager* instance)
    {
        s_Instance = instance;
    }

    ObjectManager& ObjectManager::Get()
    {
        ME_ASSERT(s_Instance != nullptr, "ObjectManager is not initialized");
        return *s_Instance;
    }

    bool ObjectManager::HasInstance()
    {
        return s_Instance != nullptr;
    }

    void ObjectManager::Initialize()
    {
        m_ObjectsByGuid.clear();
    }

    void ObjectManager::Shutdown()
    {
        m_GarbageRootSources.clear();
        m_ObjectsByGuid.clear();
        ME_CORE_INFO("ObjectManager Shutdown.");
    }

    void ObjectManager::RegisterObject(const std::shared_ptr<MEObject>& object)
    {
        if (object == nullptr)
        {
            return;
        }

        GUID guid = object->GetGuid();
        if (guid.IsZero())
        {
            guid = GenerateGUID();
            object->SetGuid(guid);
        }

        m_ObjectsByGuid[guid] = object;
    }

    bool ObjectManager::RemapObjectGuid(const std::shared_ptr<MEObject>& object, const GUID& newGuid)
    {
        if (object == nullptr || newGuid.IsZero())
        {
            return false;
        }

        UnregisterObject(object->GetGuid());
        object->SetGuid(newGuid);
        RegisterObject(object);
        return true;
    }

    bool ObjectManager::UnregisterObject(const GUID& guid)
    {
        if (guid.IsZero())
        {
            return false;
        }

        auto iter = m_ObjectsByGuid.find(guid);
        if (iter == m_ObjectsByGuid.end())
        {
            return false;
        }

        m_ObjectsByGuid.erase(iter);
        return true;
    }

    bool ObjectManager::UnregisterObject(const MEObject* object)
    {
        if (object == nullptr)
        {
            return false;
        }

        return UnregisterObject(object->GetGuid());
    }

    std::shared_ptr<MEObject> ObjectManager::FindObject(const GUID& guid) const
    {
        if (guid.IsZero())
        {
            return nullptr;
        }

        auto iter = m_ObjectsByGuid.find(guid);
        if (iter == m_ObjectsByGuid.end())
        {
            return nullptr;
        }

        return iter->second.lock();
    }

    std::shared_ptr<MEObject> ObjectManager::FindObject(const std::string& name) const
    {
        for (const auto& [guid, weakObj] : m_ObjectsByGuid)
        {
            std::shared_ptr<MEObject> obj = weakObj.lock();
            if (obj && obj->GetName() == name)
            {
                return obj;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<MEObject>> ObjectManager::FindObjectsByClass(const Reflection::MEClass* classInfo) const
    {
        std::vector<std::shared_ptr<MEObject>> result;
        for (const auto& [guid, weakObj] : m_ObjectsByGuid)
        {
            std::shared_ptr<MEObject> obj = weakObj.lock();
            if (obj && obj->GetClass() == classInfo)
            {
                result.push_back(obj);
            }
        }
        return result;
    }

    void ObjectManager::ForEachLiveObject(
        const std::function<void(const std::shared_ptr<MEObject>&)>& visitor) const
    {
        if (!visitor)
        {
            return;
        }

        for (const auto& [guid, weakObj] : m_ObjectsByGuid)
        {
            (void)guid;
            if (std::shared_ptr<MEObject> object = weakObj.lock())
            {
                visitor(object);
            }
        }
    }

    std::shared_ptr<MEObject> ObjectManager::NewObject(
        const Reflection::MEClass* classInfo,
        const std::string& inName,
        MEObject* inOuter,
        const GUID& inGuid)
    {
        if (classInfo == nullptr)
        {
            ME_CORE_ERROR("NewObject: classInfo is null.");
            return nullptr;
        }

        std::shared_ptr<MEObject> newObj = std::static_pointer_cast<MEObject>(classInfo->CreateDefaultInstance());
        if (newObj == nullptr)
        {
            ME_CORE_ERROR("Failed to create instance of class '{}'.", classInfo->GetName());
            return nullptr;
        }

        newObj->SetClass(classInfo);
        newObj->SetName(inName);
        newObj->SetGuid(inGuid);
        newObj->SetOuter(inOuter);
        RegisterObject(newObj);
        return newObj;
    }

    std::shared_ptr<MEObject> ObjectManager::NewObject(
        const std::string& className,
        const std::string& inName,
        MEObject* inOuter,
        const GUID& inGuid)
    {
        const Reflection::MEClass* classInfo = Reflection::ReflectionSystem::Get().FindClass(className);
        if (classInfo == nullptr)
        {
            ME_CORE_ERROR("Failed to find class info for class name '{}'.", className);
            return nullptr;
        }

        return NewObject(classInfo, inName, inOuter, inGuid);
    }

    bool ObjectManager::RemoveObject(const GUID& guid)
    {
        return UnregisterObject(guid);
    }

    bool ObjectManager::RemoveObject(const MEObject* object)
    {
        return UnregisterObject(object);
    }

    void ObjectManager::PruneExpiredEntries()
    {
        for (auto iter = m_ObjectsByGuid.begin(); iter != m_ObjectsByGuid.end();)
        {
            if (iter->second.expired())
            {
                iter = m_ObjectsByGuid.erase(iter);
            }
            else
            {
                ++iter;
            }
        }
    }

    void ObjectManager::RegisterGarbageRootSource(
        ObjectGarbageRootSourceId sourceId,
        ObjectReachabilityRootVisitor visitRoots)
    {
        if (sourceId == nullptr || !visitRoots)
        {
            return;
        }

        m_GarbageRootSources[sourceId] = std::move(visitRoots);
    }

    void ObjectManager::UnregisterGarbageRootSource(ObjectGarbageRootSourceId sourceId)
    {
        if (sourceId == nullptr)
        {
            return;
        }

        m_GarbageRootSources.erase(sourceId);
    }

    void ObjectManager::VisitEngineGarbageRoots(const ObjectReachabilityMarker& markReachable) const
    {
        if (AssetManager::HasInstance())
        {
            AssetManager::Get().MarkReachableLoadedAssets(markReachable);
        }

        if (SceneManager::HasInstance())
        {
            const std::shared_ptr<Scene> activeScene = SceneManager::Get().GetCurrentActiveScene();
            if (activeScene)
            {
                activeScene->MarkReachableObjects(markReachable);
            }
        }

        for (const auto& [sourceId, visitRoots] : m_GarbageRootSources)
        {
            (void)sourceId;
            if (visitRoots)
            {
                visitRoots(markReachable);
            }
        }
    }

    void ObjectManager::CollectGarbageWithEngineRoots()
    {
        CollectGarbage([this](const ObjectReachabilityMarker& markReachable) {
            VisitEngineGarbageRoots(markReachable);
        });
    }

    void ObjectManager::CollectGarbage(const ObjectReachabilityRootVisitor& visitRoots)
    {
        std::unordered_set<GUID, GUID::Hash> reachableGuids;
        if (visitRoots)
        {
            visitRoots([&reachableGuids](MEObject* object) {
                if (object == nullptr || object->GetGuid().IsZero())
                {
                    return;
                }
                reachableGuids.insert(object->GetGuid());
            });
        }

        PruneExpiredEntries();

        if (!visitRoots)
        {
            return;
        }

        for (const auto& [guid, weakObj] : m_ObjectsByGuid)
        {
            const std::shared_ptr<MEObject> liveObject = weakObj.lock();
            if (!liveObject)
            {
                continue;
            }

            if (reachableGuids.find(guid) == reachableGuids.end())
            {
                ME_CORE_WARN(
                    "ObjectManager::CollectGarbage: live object '{}' ({}) is not reachable from supplied roots.",
                    liveObject->GetName(),
                    guid.ToString());
            }
        }
    }
}

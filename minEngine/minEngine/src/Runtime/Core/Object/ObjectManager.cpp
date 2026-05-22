#include "ObjectManager.h"

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

    void ObjectManager::Initialize()
    {
        m_ObjectsByGuid.clear();
    }

    void ObjectManager::Shutdown()
    {
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

        // Copy the shared_ptr out before erasing the map entry. This prevents
        // the object's destructor running inside unordered_map::erase(), which
        // could re-enter ObjectManager and mutate the hashtable while it's in
        // an inconsistent state (leading to crashes). Holding a local copy
        // ensures destruction happens after erase completes.
        std::shared_ptr<MEObject> obj = iter->second;
        m_ObjectsByGuid.erase(iter);
        (void)obj; // keep obj alive until end of scope
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

        return iter->second;
    }

    std::shared_ptr<MEObject> ObjectManager::FindObject(const std::string &name) const
    {
        for (const auto& [guid, obj] : m_ObjectsByGuid)
        {;
            if (obj && obj->GetName() == name)
            {
                return obj;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<MEObject>> ObjectManager::FindObjectsByClass(const Reflection::MEClass *classInfo) const
    {
        std::vector<std::shared_ptr<MEObject>> result;
        for(auto& [guid, obj] : m_ObjectsByGuid)
        {
            if (obj && obj->GetClass() == classInfo)
            {
                result.push_back(obj);
            }
        }
        return result;
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

    bool ObjectManager::RemoveObject(const GUID &guid)
    {
        return UnregisterObject(guid);
    }

    bool ObjectManager::RemoveObject(const MEObject *object)
    {
        return UnregisterObject(object);
    }
}

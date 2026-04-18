#include "ObjectManager.h"
#include "Runtime/Function/RuntimeGlobalContext.h"

namespace minEngine
{
    ObjectManager &ObjectManager::Get()
    {
        return *RuntimeGlobalContext::GetRuntimeGlobalContext().m_ObjectManager;
    }

    void ObjectManager::Initialize()
    {
        m_ObjectsByGuid.clear();
    }

    void ObjectManager::Shutdown()
    {
        m_ObjectsByGuid.clear();
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

    void ObjectManager::UnregisterObject(const GUID& guid)
    {
        if (guid.IsZero())
        {
            return;
        }

        m_ObjectsByGuid.erase(guid);
    }

    void ObjectManager::UnregisterObject(const MEObject* object)
    {
        if (object == nullptr)
        {
            return;
        }

        UnregisterObject(object->GetGuid());
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
}

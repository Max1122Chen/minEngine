#pragma once

#include "Core.h"
#include "MEObject.h"

namespace minEngine
{
    class ObjectManager
    {
    public:
        ObjectManager() = default;
        ~ObjectManager() = default;

        static ObjectManager& Get();

        void Initialize();
        void Shutdown();

        void RegisterObject(const std::shared_ptr<MEObject>& object);
        void UnregisterObject(const GUID& guid);
        void UnregisterObject(const MEObject* object);

        std::shared_ptr<MEObject> FindObject(const GUID& guid) const;
        std::shared_ptr<MEObject> FindObject(const std::string& name) const;

        template<typename T>
        std::shared_ptr<T> FindObjectAs(const GUID& guid) const
        {
            static_assert(std::is_base_of_v<MEObject, T>, "T must be derived from MEObject");
            std::shared_ptr<MEObject> baseObject = FindObject(guid);
            if (baseObject == nullptr)
            {
                return nullptr;
            }
            return std::dynamic_pointer_cast<T>(baseObject);
        }

        template<typename T>
        std::shared_ptr<T> NewObject(const std::string& inName = "", MEObject* inOuter = nullptr, const GUID& inGuid = GenerateGUID())
        {
            static_assert(std::is_base_of_v<MEObject, T>, "T must be derived from MEObject");
            std::shared_ptr<T> newObj = std::make_shared<T>();
            newObj->SetClass(Reflection::ReflectionSystem::Get().FindClass<T>());
            newObj->SetName(inName);
            newObj->SetGuid(inGuid);
            newObj->SetOuter(inOuter);
            RegisterObject(newObj);
            return newObj;
        }
        size_t GetTrackedObjectCount() const { return m_ObjectsByGuid.size(); }

    private:


        std::unordered_map<GUID, std::shared_ptr<MEObject>, GUID::Hash> m_ObjectsByGuid;
    };

    // Convenience static methods for global access
    template<typename T>
    static std::shared_ptr<T> NewObject(const std::string& inName = "", MEObject* inOuter = nullptr, const GUID& inGuid = GenerateGUID())
    {
        return ObjectManager::Get().NewObject<T>(inName, inOuter, inGuid);
    }

    template<typename T>
    static std::shared_ptr<T> FindObjectAs(const GUID& guid)
    {
        return ObjectManager::Get().FindObjectAs<T>(guid);
    }

    static std::shared_ptr<MEObject> FindObject(const GUID& guid)
    {
        return ObjectManager::Get().FindObject(guid);
    }

    static std::shared_ptr<MEObject> FindObject(const std::string& name)
    {
        return ObjectManager::Get().FindObject(name);
    }
    
}

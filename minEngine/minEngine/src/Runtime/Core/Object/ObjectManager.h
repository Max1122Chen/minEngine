#pragma once

#include "Core.h"
#include "MEObject.h"

namespace minEngine
{
    class Engine;

    class ObjectManager
    {
    public:
        ObjectManager() = default;
        ~ObjectManager() = default;

        static ObjectManager& Get();

        void Initialize();
        void Shutdown();

        void RegisterObject(const std::shared_ptr<MEObject>& object);
        bool UnregisterObject(const GUID& guid);
        bool UnregisterObject(const MEObject* object);

        std::shared_ptr<MEObject> FindObject(const GUID& guid) const;
        std::shared_ptr<MEObject> FindObject(const std::string& name) const;

        std::vector<std::shared_ptr<MEObject>> FindObjectsByClass(const Reflection::MEClass* classInfo) const;

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

        std::shared_ptr<MEObject> NewObject(
            const Reflection::MEClass* classInfo,
            const std::string& inName = "",
            MEObject* inOuter = nullptr,
            const GUID& inGuid = GenerateGUID());

        std::shared_ptr<MEObject> NewObject(const std::string& className, const std::string& inName = "", MEObject* inOuter = nullptr, const GUID& inGuid = GenerateGUID());

        // Cancel the reference to the object with the given GUID, allowing it to be destroyed if there are no other references. Returns true if the object was found and unregistered, false otherwise.
        bool RemoveObject(const GUID& guid);
        bool RemoveObject(const MEObject* object);

        size_t GetTrackedObjectCount() const { return m_ObjectsByGuid.size(); }

    private:
        friend class Engine;

        static void SetInstance(ObjectManager* instance);
        static ObjectManager* s_Instance;

        std::unordered_map<GUID, std::shared_ptr<MEObject>, GUID::Hash> m_ObjectsByGuid;
    };

    // Convenience methods for global access
    template<typename T>
    inline std::shared_ptr<T> NewObject(const std::string& inName = "", MEObject* inOuter = nullptr, const GUID& inGuid = GenerateGUID())
    {
        return ObjectManager::Get().NewObject<T>(inName, inOuter, inGuid);
    }

    template<typename T>
    inline std::shared_ptr<T> FindObjectAs(const GUID& guid)
    {
        return ObjectManager::Get().FindObjectAs<T>(guid);
    }

    inline std::shared_ptr<MEObject> FindObject(const GUID& guid)
    {
        return ObjectManager::Get().FindObject(guid);
    }

    inline std::shared_ptr<MEObject> FindObject(const std::string& name)
    {
        return ObjectManager::Get().FindObject(name);
    }

    inline bool RemoveObject(const GUID& guid)
    {
        return ObjectManager::Get().RemoveObject(guid);
    }

    inline bool RemoveObject(const MEObject* object)
    {
        return ObjectManager::Get().RemoveObject(object);
    }

    inline std::shared_ptr<MEObject> NewObject(
        const Reflection::MEClass* classInfo,
        const std::string& inName = "",
        MEObject* inOuter = nullptr,
        const GUID& inGuid = GenerateGUID())
    {
        return ObjectManager::Get().NewObject(classInfo, inName, inOuter, inGuid);
    }
}

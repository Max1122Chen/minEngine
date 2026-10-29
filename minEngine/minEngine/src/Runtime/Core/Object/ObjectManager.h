#pragma once

#include "Core.h"
#include "MEObject.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    using ObjectReachabilityMarker = std::function<void(MEObject*)>;
    using ObjectReachabilityRootVisitor = std::function<void(const ObjectReachabilityMarker& markReachable)>;
    using ObjectGarbageRootSourceId = void*;

    /**
     * Core-service entries whose contract reserves them for the game role.
     *
     * The object registry, the synchronous asset load path and the scene teardown all read or write
     * state the frame model gives to one thread: a registry walked while another thread inserts into
     * it, a load that mutates the asset cache and the object registry, a teardown that releases
     * render state a role thread may still be reading. None of them refused an off-role caller, and
     * none of them said anything about it either, which is what made a wrong-thread call invisible
     * until it surfaced as corruption somewhere else.
     *
     * One entry per guarded call site, so the count says which entry was entered rather than only
     * that something was.
     */
    enum class ECoreServiceEntry : uint8_t
    {
        NewObject = 0,
        FindObjectByGuid,
        FindObjectByName,
        FindObjectsByClass,
        ForEachLiveObject,
        CollectGarbage,
        /** Synchronous load: LoadAssetByPath / LoadAssetByMeta / LoadAssetByGUID. */
        SyncAssetLoad,
        SceneShutdown,
        Count
    };

    /** Process-level object registry (not owned by Engine). Aligns with LogSystem lifetime. */
    class MINENGINE_API ObjectManager
    {
    public:
        ObjectManager() = default;
        ~ObjectManager() = default;

        static ObjectManager& Get();
        static bool HasInstance();

        /** Create process singleton if needed, then reset the registry. */
        static void Initialize();
        /** Clear registry and destroy the process-owned instance (if any). */
        static void Shutdown();

        void RegisterObject(const std::shared_ptr<MEObject>& object);
        bool RemapObjectGuid(const std::shared_ptr<MEObject>& object, const GUID& newGuid);
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
            return Cast<T>(baseObject);
        }

        template<typename T>
        std::shared_ptr<T> NewObject(const std::string& inName = "", MEObject* inOuter = nullptr, const GUID& inGuid = GenerateGUID())
        {
            static_assert(std::is_base_of_v<MEObject, T>, "T must be derived from MEObject");
            // The overload every caller reaches: the registry it inserts into is the game role's.
            ME_ENSURE(
                !NoteIfOffGameRole(ECoreServiceEntry::NewObject),
                "ObjectManager::NewObject was called off the game role: the object registry is read "
                "and written by the game role, so an object inserted here is an entry no other "
                "thread's view of the registry contains. The object is still created.");
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

        // Prunes expired weak entries. When visitRoots is set, warns about live objects not marked reachable.
        void CollectGarbage(const ObjectReachabilityRootVisitor& visitRoots = nullptr);

        /** Marks documented engine roots (AssetManager cache, active scene, registered sources) then audits orphans. */
        void CollectGarbageWithEngineRoots();

        void RegisterGarbageRootSource(ObjectGarbageRootSourceId sourceId, ObjectReachabilityRootVisitor visitRoots);
        void UnregisterGarbageRootSource(ObjectGarbageRootSourceId sourceId);
        void VisitEngineGarbageRoots(const ObjectReachabilityMarker& markReachable) const;

        size_t GetTrackedObjectCount() const
        {
            size_t count = 0;
            for (const auto& [guid, weakObj] : m_ObjectsByGuid)
            {
                if (!weakObj.expired())
                {
                    ++count;
                }
            }
            return count;
        }

        void ForEachLiveObject(const std::function<void(const std::shared_ptr<MEObject>&)>& visitor) const;

        /**
         * Counts one entered core-service entry, counted per entry, process-wide: the count is a fact
         * about the run, not about one registry, so it needs no instance and survives a registry
         * being replaced.
         */
        static void NoteOffRoleCoreServiceCall(ECoreServiceEntry entry);

        /** Calls counted for one entry since process start. Diagnostics, tests, the frame summary. */
        static uint64_t GetOffRoleCoreServiceCallCount(ECoreServiceEntry entry);

        /** Every entry's off-role calls, summed. What the per-frame work-loss summary reports. */
        static uint64_t GetOffRoleCoreServiceCallCountTotal();

        /**
         * Counts the call and answers true when the caller is not the thread that holds the game
         * role, so a call site can hand the answer straight to ME_ENSURE:
         *
         *     ME_ENSURE(!ObjectManager::NoteIfOffGameRole(entry), "why this entry wants the role");
         *
         * A process with no frame-role table installed - headless tools, unit tests, a single-thread
         * launch before the roles are bound - has no role to be off, so it reports nothing. The call
         * that follows is never refused: these entries stay safe to call from any thread for now,
         * and a caller outside the role is a suspicious call site, not a violated premise.
         */
        static bool NoteIfOffGameRole(ECoreServiceEntry entry);

    private:
        friend class Testing::TestAccess<ObjectManager>;

        static void SetInstance(ObjectManager* instance);
        void ResetRegistry();
        void ClearRegistry();
        void PruneExpiredEntries();

        static ObjectManager* s_Instance;
        static std::unique_ptr<ObjectManager> s_Owned;

        std::unordered_map<GUID, std::weak_ptr<MEObject>, GUID::Hash> m_ObjectsByGuid;
        std::unordered_map<ObjectGarbageRootSourceId, ObjectReachabilityRootVisitor> m_GarbageRootSources;
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

    inline void CollectGarbageWithEngineRoots()
    {
        ObjectManager::Get().CollectGarbageWithEngineRoots();
    }
}

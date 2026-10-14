#pragma once

#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Subsystems/Subsystem.h"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace minEngine
{
    /**
     * Owns subsystem instances for one Outer (Engine / GameInstance / Scene).
     * Discovery: Reflection GetDerivedClasses(Base), skip Abstract, ShouldCreateSubsystem.
     */
    class SubsystemCollectionBase
    {
    public:
        SubsystemCollectionBase() = default;
        ~SubsystemCollectionBase();

        SubsystemCollectionBase(const SubsystemCollectionBase&) = delete;
        SubsystemCollectionBase& operator=(const SubsystemCollectionBase&) = delete;

        void Initialize(MEObject* outer, const Reflection::MEClass* baseClass);
        void Deinitialize();
        bool IsInitialized() const { return m_bInitialized; }

        Subsystem* InitializeDependency(const Reflection::MEClass* subsystemClass);
        template<typename T>
        T* InitializeDependency()
        {
            static_assert(std::is_base_of_v<Subsystem, T>, "T must derive from Subsystem");
            return static_cast<T*>(InitializeDependency(T::StaticClass()));
        }

        Subsystem* GetSubsystem(const Reflection::MEClass* subsystemClass) const;
        template<typename T>
        T* GetSubsystem() const
        {
            static_assert(std::is_base_of_v<Subsystem, T>, "T must derive from Subsystem");
            return static_cast<T*>(GetSubsystem(T::StaticClass()));
        }

        void ForEachSubsystem(const std::function<void(Subsystem*)>& fn) const;

        /** Test-only: force a type into discovery without full reflection scan of the module. */
        void RegisterTypeForTest(const Reflection::MEClass* subsystemClass);

    private:
        Subsystem* CreateAndInitialize(const Reflection::MEClass* subsystemClass);
        bool IsTypeAllowed(const Reflection::MEClass* subsystemClass) const;

        MEObject* m_Outer = nullptr;
        const Reflection::MEClass* m_BaseClass = nullptr;
        bool m_bInitialized = false;
        bool m_bPopulating = false;

        std::vector<std::shared_ptr<Subsystem>> m_Instances;
        std::unordered_map<const Reflection::MEClass*, Subsystem*> m_ByClass;
        std::unordered_set<const Reflection::MEClass*> m_TestExtraTypes;
        std::unordered_set<const Reflection::MEClass*> m_InitializingStack;
    };

    template<typename TBase>
    class SubsystemCollection : public SubsystemCollectionBase
    {
    public:
        void Initialize(MEObject* outer)
        {
            static_assert(std::is_base_of_v<Subsystem, TBase>, "TBase must derive from Subsystem");
            SubsystemCollectionBase::Initialize(outer, TBase::StaticClass());
        }

        template<typename T>
        T* GetSubsystem() const
        {
            static_assert(std::is_base_of_v<TBase, T> || std::is_same_v<TBase, T>,
                "T must derive from collection base");
            return SubsystemCollectionBase::GetSubsystem<T>();
        }
    };
}

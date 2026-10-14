#include "SubsystemCollection.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <algorithm>

namespace minEngine
{
    SubsystemCollectionBase::~SubsystemCollectionBase()
    {
        if (m_bInitialized)
        {
            Deinitialize();
        }
    }

    void SubsystemCollectionBase::Initialize(MEObject* outer, const Reflection::MEClass* baseClass)
    {
        ME_ASSERT(outer != nullptr, "SubsystemCollection::Initialize requires outer");
        ME_ASSERT(baseClass != nullptr, "SubsystemCollection::Initialize requires base class");
        ME_ASSERT(!m_bInitialized, "SubsystemCollection already initialized");
        ME_ASSERT(ObjectManager::HasInstance(), "ObjectManager must be initialized before subsystems");

        m_Outer = outer;
        m_BaseClass = baseClass;
        m_bPopulating = true;

        std::vector<const Reflection::MEClass*> types =
            Reflection::ReflectionSystem::Get().GetDerivedClasses(baseClass);

        for (const Reflection::MEClass* extra : m_TestExtraTypes)
        {
            if (extra != nullptr
                && std::find(types.begin(), types.end(), extra) == types.end())
            {
                types.push_back(extra);
            }
        }

        for (const Reflection::MEClass* type : types)
        {
            if (!IsTypeAllowed(type))
            {
                continue;
            }

            if (m_ByClass.find(type) != m_ByClass.end())
            {
                continue;
            }

            CreateAndInitialize(type);
        }

        m_bPopulating = false;
        m_bInitialized = true;
    }

    void SubsystemCollectionBase::Deinitialize()
    {
        if (!m_bInitialized && m_Instances.empty())
        {
            m_Outer = nullptr;
            m_BaseClass = nullptr;
            m_ByClass.clear();
            return;
        }

        // Reverse creation order.
        for (auto it = m_Instances.rbegin(); it != m_Instances.rend(); ++it)
        {
            if (*it)
            {
                (*it)->Deinitialize();
            }
        }

        for (auto it = m_Instances.rbegin(); it != m_Instances.rend(); ++it)
        {
            if (*it)
            {
                ObjectManager::Get().UnregisterObject(it->get());
            }
        }

        m_Instances.clear();
        m_ByClass.clear();
        m_InitializingStack.clear();
        m_bInitialized = false;
        m_bPopulating = false;
        m_Outer = nullptr;
        m_BaseClass = nullptr;
    }

    bool SubsystemCollectionBase::IsTypeAllowed(const Reflection::MEClass* subsystemClass) const
    {
        if (subsystemClass == nullptr || m_BaseClass == nullptr)
        {
            return false;
        }
        if (subsystemClass == m_BaseClass)
        {
            return false;
        }
        if (!subsystemClass->IsA(m_BaseClass))
        {
            return false;
        }
        if (subsystemClass->HasSpecifier(Reflection::ClassSpecifier::Abstract))
        {
            return false;
        }
        return true;
    }

    Subsystem* SubsystemCollectionBase::CreateAndInitialize(const Reflection::MEClass* subsystemClass)
    {
        ME_ASSERT(subsystemClass != nullptr, "CreateAndInitialize null class");
        ME_ASSERT(m_Outer != nullptr, "CreateAndInitialize null outer");

        if (m_InitializingStack.find(subsystemClass) != m_InitializingStack.end())
        {
            ME_LOG(LogCore, Error,
                "SubsystemCollection: cyclic InitializeDependency involving '{}'",
                subsystemClass->GetName());
            ME_ASSERT(false, "Subsystem InitializeDependency cycle");
            return nullptr;
        }

        auto existing = m_ByClass.find(subsystemClass);
        if (existing != m_ByClass.end())
        {
            return existing->second;
        }

        if (!IsTypeAllowed(subsystemClass))
        {
            return nullptr;
        }

        // CDO gate via temporary default instance (factory skips C++ abstracts).
        {
            std::shared_ptr<void> cdoVoid = subsystemClass->CreateDefaultInstance();
            if (!cdoVoid)
            {
                return nullptr;
            }

            Subsystem* cdoSubsystem =
                static_cast<Subsystem*>(subsystemClass->CastObject(cdoVoid.get()));
            if (cdoSubsystem == nullptr || !cdoSubsystem->ShouldCreateSubsystem(m_Outer))
            {
                return nullptr;
            }
        }

        std::shared_ptr<MEObject> created =
            ObjectManager::Get().NewObject(subsystemClass, subsystemClass->GetName(), m_Outer);
        if (!created)
        {
            ME_LOG(LogCore, Error,
                "SubsystemCollection: NewObject failed for '{}'",
                subsystemClass->GetName());
            return nullptr;
        }

        Subsystem* rawSubsystem = static_cast<Subsystem*>(subsystemClass->CastObject(created.get()));
        if (rawSubsystem == nullptr)
        {
            ME_LOG(LogCore, Error,
                "SubsystemCollection: NewObject did not yield Subsystem for '{}'",
                subsystemClass->GetName());
            ObjectManager::Get().UnregisterObject(created.get());
            return nullptr;
        }

        std::shared_ptr<Subsystem> instance(created, rawSubsystem);
        m_Instances.push_back(instance);
        m_ByClass[subsystemClass] = instance.get();

        m_InitializingStack.insert(subsystemClass);
        instance->Initialize(*this);
        m_InitializingStack.erase(subsystemClass);

        return instance.get();
    }

    Subsystem* SubsystemCollectionBase::InitializeDependency(const Reflection::MEClass* subsystemClass)
    {
        ME_ASSERT(m_bPopulating,
            "InitializeDependency is only valid while SubsystemCollection is populating");
        if (subsystemClass == nullptr)
        {
            return nullptr;
        }
        return CreateAndInitialize(subsystemClass);
    }

    Subsystem* SubsystemCollectionBase::GetSubsystem(const Reflection::MEClass* subsystemClass) const
    {
        if (subsystemClass == nullptr)
        {
            return nullptr;
        }

        auto exact = m_ByClass.find(subsystemClass);
        if (exact != m_ByClass.end())
        {
            return exact->second;
        }

        // Allow GetSubsystem<Base>() to find a created derived instance.
        for (const auto& [type, instance] : m_ByClass)
        {
            if (type != nullptr && type->IsA(subsystemClass))
            {
                return instance;
            }
        }
        return nullptr;
    }

    void SubsystemCollectionBase::ForEachSubsystem(const std::function<void(Subsystem*)>& fn) const
    {
        if (!fn)
        {
            return;
        }
        for (const std::shared_ptr<Subsystem>& instance : m_Instances)
        {
            if (instance)
            {
                fn(instance.get());
            }
        }
    }

    void SubsystemCollectionBase::RegisterTypeForTest(const Reflection::MEClass* subsystemClass)
    {
        if (subsystemClass != nullptr)
        {
            m_TestExtraTypes.insert(subsystemClass);
        }
    }
}

#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "MEProperties.h"

namespace minEngine::Reflection
{
    using MEClassFactoryFn = std::shared_ptr<void> (*)();

    // 
    class MEStruct
    {
    public:
        explicit MEStruct(std::string inName)
            : m_Name(std::move(inName))
        {
        }

        virtual ~MEStruct() = default;

        const std::string& GetName() const
        {
            return m_Name;
        }

        void SetName(std::string inName)
        {
            m_Name = std::move(inName);
        }

    protected:
        std::string m_Name;
    };

    // 
    class MEClass final : public MEStruct
    {
    public:
        explicit MEClass(std::string inName)
            : MEStruct(std::move(inName))
        {
        }

        void SetFactory(MEClassFactoryFn inFactory)
        {
            m_Factory = inFactory;
        }

        MEClassFactoryFn GetFactory() const
        {
            return m_Factory;
        }

        bool HasFactory() const
        {
            return m_Factory != nullptr;
        }

        std::shared_ptr<void> CreateInstance() const
        {
            if (m_Factory == nullptr)
            {
                return nullptr;
            }

            return m_Factory();
        }

        template<typename T>
        static std::shared_ptr<void> CreateDefaultInstance()
        {
            if constexpr (std::is_default_constructible_v<T> && !std::is_abstract_v<T>)
            {
                return std::make_shared<T>();
            }
            else
            {
                return nullptr;
            }
        }

        void SetResolvedSuperClass(MEClass* inSuperClass)
        {
            m_SuperClass = inSuperClass;
        }

        MEClass* GetSuperClass()
        {
            return m_SuperClass;
        }

        const MEClass* GetSuperClass() const
        {
            return m_SuperClass;
        }

        void AddProperty(MEProperty* property)
        {
            if (property != nullptr)
            {
                m_Properties.push_back(property);
            }
        }

        std::vector<MEProperty*>& GetProperties()
        {
            return m_Properties;
        }

        const std::vector<MEProperty*>& GetProperties() const
        {
            return m_Properties;
        }

        void ClearDirectDerivedClasses()
        {
            m_DirectDerivedClasses.clear();
        }

        void AddDirectDerivedClass(MEClass* derivedClass)
        {
            if (derivedClass != nullptr)
            {
                m_DirectDerivedClasses.push_back(derivedClass);
            }
        }

        const std::vector<MEClass*>& GetDirectDerivedClasses() const
        {
            return m_DirectDerivedClasses;
        }

    private:
        MEClassFactoryFn m_Factory = nullptr;
        MEClass* m_SuperClass = nullptr;
        std::vector<MEProperty*> m_Properties;
        std::vector<MEClass*> m_DirectDerivedClasses;
    };

    // 
    struct MEEnumEntry
    {
        std::string name;
        int64_t value = 0;
    };

    class MEEnum
    {
    public:
        explicit MEEnum(std::string inName)
            : m_Name(std::move(inName))
        {
        }

        const std::string& GetName() const
        {
            return m_Name;
        }

        void AddEntry(std::string name, int64_t value)
        {
            m_Entries.push_back(MEEnumEntry{std::move(name), value});
        }

        const MEEnumEntry* FindByName(const std::string& enumName) const
        {
            for (const MEEnumEntry& entry : m_Entries)
            {
                if (entry.name == enumName)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const MEEnumEntry* FindByValue(int64_t enumValue) const
        {
            for (const MEEnumEntry& entry : m_Entries)
            {
                if (entry.value == enumValue)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const std::vector<MEEnumEntry>& GetEntries() const
        {
            return m_Entries;
        }

    private:
        std::string m_Name;
        std::vector<MEEnumEntry> m_Entries;
    };
}
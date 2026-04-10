#pragma once

#include <string>
#include <utility>
#include <vector>

#include "MEProperties.h"

namespace minEngine::MEReflection
{
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

    private:
        std::string m_Name;
    };

    class MEClass final : public MEStruct
    {
    public:
        explicit MEClass(std::string inName)
            : MEStruct(std::move(inName))
        {
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
        MEClass* m_SuperClass = nullptr;
        std::vector<MEProperty*> m_Properties;
        std::vector<MEClass*> m_DirectDerivedClasses;
    };
}
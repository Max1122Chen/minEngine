#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "MEProperties.h"

namespace minEngine::Reflection
{
    using MEClassFactoryFn = std::shared_ptr<void> (*)();
    using MEClassCasterFn = void* (*)(void* objectPtr);

    enum class ClassSpecifier : uint32_t
    {
        None = 0u,
        Abstract = 1u << 0,
        Transient = 1u << 1,
        EditorOnly = 1u << 2,
    };

    using ClassSpecifierMask = uint32_t;
    using ClassMetadata = std::unordered_map<std::string, std::string>;

    // Take in a type-erased shared_ptr<void> and set it to the dst, which actually points to a shared_ptr of the correct type. This is used for handling shared pointer properties in a generic way without knowing the actual type at compile time.
    using MEClassSetSharedPtrFn = bool (*)(const std::shared_ptr<void>& src, void* dst);

    // 
    class MINENGINE_API MEStruct
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
    class MINENGINE_API MEClass final : public MEStruct
    {
    public:
        explicit MEClass(std::string inName)
            : MEStruct(std::move(inName))
        {
        }

        MEClassFactoryFn GetFactory() const { return m_Factory; }
        void SetFactory(MEClassFactoryFn inFactory) { m_Factory = inFactory; }
        bool HasFactory() const { return m_Factory != nullptr; }

        MEClassCasterFn GetCaster() const { return m_Caster; }
        void SetCaster(MEClassCasterFn inCaster) { m_Caster = inCaster; }
        bool HasCaster() const { return m_Caster != nullptr; }

        void SetSharedPtrSetter(MEClassSetSharedPtrFn inSetSharedPtr) { m_SetSharedPtr = inSetSharedPtr; }
        bool HasSharedPtrSetter() const { return m_SetSharedPtr != nullptr; }

        std::shared_ptr<void> CreateInstance() const { return (m_Factory != nullptr) ? m_Factory() : nullptr; }

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

        void* CastObject(void* objectPtr) const { return (m_Caster != nullptr) ? m_Caster(objectPtr) : nullptr; }
        template<typename T>
        void* CastObjectImpl(void* objectPtr) const
        {
            if (objectPtr == nullptr)
            {
                return nullptr;
            }

            T* typedPtr = static_cast<T*>(objectPtr);
            return static_cast<void*>(typedPtr);
        }

        bool SetSharedPtr(const std::shared_ptr<void>& src, void* dst) const { return (m_SetSharedPtr != nullptr) ? m_SetSharedPtr(src, dst) : false; }
         
        template<typename T>
        static bool SetSharedPtrImpl(const std::shared_ptr<void>& src, void* dst)
        {
            if (dst == nullptr)
            {
                return false;
            }

            std::shared_ptr<T>* typedDst = static_cast<std::shared_ptr<T>*>(dst);

            if (src == nullptr)
            {
                typedDst->reset();
                return true;
            }

            *typedDst = std::static_pointer_cast<T>(src);
            return true;
        }

        void SetResolvedSuperClass(MEClass* inSuperClass) { m_SuperClass = inSuperClass; }

        MEClass* GetSuperClass() { return m_SuperClass; }
        const MEClass* GetSuperClass() const { return m_SuperClass; }

        bool IsA(const MEClass* baseClass) const
        {
            if (baseClass == nullptr)
            {
                return false;
            }

            const MEClass* current = this;
            while (current != nullptr)
            {
                if (current == baseClass)
                {
                    return true;
                }

                current = current->GetSuperClass();
            }

            return false;
        }

        ClassSpecifierMask GetSpecifierMask() const { return m_SpecifierMask; }
        void SetAnnotations(ClassSpecifierMask inSpecifierMask, ClassMetadata inMetadata)
        {
            m_SpecifierMask = inSpecifierMask;
            m_Metadata = std::move(inMetadata);
        }

        bool HasSpecifier(ClassSpecifier specifier) const
        {
            return (m_SpecifierMask & static_cast<ClassSpecifierMask>(specifier)) != 0;
        }

        const ClassMetadata& GetMetadata() const { return m_Metadata; }

        const std::string* FindMetadata(const std::string& key) const
        {
            auto iter = m_Metadata.find(key);
            if (iter == m_Metadata.end())
            {
                return nullptr;
            }
            return &iter->second;
        }

        void AddProperty(MEProperty* property)
        {
            if (property != nullptr)
            {
                m_Properties.push_back(property);
            }
        }

        std::vector<MEProperty*>& GetProperties() { return m_Properties; }
        const std::vector<MEProperty*>& GetProperties() const { return m_Properties; }

        void ClearDirectDerivedClasses() { m_DirectDerivedClasses.clear(); }
        void AddDirectDerivedClass(MEClass* derivedClass)
        {
            if (derivedClass != nullptr)
            {
                m_DirectDerivedClasses.push_back(derivedClass);
            }
        }
        const std::vector<MEClass*>& GetDirectDerivedClasses() const { return m_DirectDerivedClasses; }
        

    private:
        MEClassFactoryFn m_Factory = nullptr;
        MEClassCasterFn m_Caster = nullptr;
        MEClassSetSharedPtrFn m_SetSharedPtr = nullptr;
        ClassSpecifierMask m_SpecifierMask = static_cast<ClassSpecifierMask>(ClassSpecifier::None);
        ClassMetadata m_Metadata;
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

    class MINENGINE_API MEEnum
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
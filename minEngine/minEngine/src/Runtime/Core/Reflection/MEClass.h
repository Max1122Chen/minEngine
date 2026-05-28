#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EngineAPI.h"
#include "MEProperties.h"

namespace minEngine
{
    class MEObject;
}

namespace minEngine::Reflection
{
    class MEFunction;

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

    class MINENGINE_API MEClass final : public MEStruct
    {
        friend class ReflectionSystem;
    public:
        explicit MEClass(std::string inName)
            : MEStruct(std::move(inName))
        {
        }

        std::shared_ptr<void> CreateDefaultInstance() const { return (m_Factory != nullptr) ? m_Factory() : nullptr; }
        void* CastObject(void* objectPtr) const { return (m_Caster != nullptr) ? m_Caster(objectPtr) : nullptr; }   // TODO: the caster is not correct now, the static cast in CastObject_Impl may case UB if the cast is not valid.
        bool SetSharedPtr(const std::shared_ptr<void>& src, void* dst) const { return (m_SetSharedPtr != nullptr) ? m_SetSharedPtr(src, dst) : false; }

        const MEClass* GetSuperClass() const { return m_SuperClass; }

        bool IsA(const MEClass* other) const
        {
            if (other == nullptr)
            {
                return false;
            }

            const MEClass* current = this;
            while (current != nullptr)
            {
                if (current == other)
                {
                    return true;
                }

                current = current->GetSuperClass();
            }

            return false;
        }

        template<typename T>
        bool IsA() const
        {
            ME_ASSERT(T::StaticClass() != nullptr, "IsA<T> requires T to be a reflected class with StaticClass() method.");
            return IsA(T::StaticClass());
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

        const std::vector<MEProperty*>& GetProperties() const { return m_Properties; }
        const std::vector<MEClass*>& GetDirectDerivedClasses() const { return m_DirectDerivedClasses; }

        const std::vector<MEFunction*>& GetFunctions() const { return m_Functions; }
        MEFunction* FindFunction(const std::string& functionName) const;
        bool InvokeStaticFunction(MEFunction* function, void* parmsBuffer) const;

    private:
        MEClass(const MEClass&) = delete;
        MEClass(MEClass&&) = delete;
        MEClass& operator=(const MEClass&) = delete;
        MEClass& operator=(MEClass&&) = delete;

        MEClass* GetSuperClass() { return m_SuperClass; }

        void AddProperty(MEProperty* property)
        {
            if (property != nullptr)
            {
                m_Properties.push_back(property);
            }
        }

        bool AddFunction(MEFunction* function);
        void SetResolvedSuperClass(MEClass* inSuperClass) { m_SuperClass = inSuperClass; }
        void ClearDirectDerivedClasses() { m_DirectDerivedClasses.clear(); }
        void AddDirectDerivedClass(MEClass* derivedClass)
        {
            if (derivedClass != nullptr)
            {
                m_DirectDerivedClasses.push_back(derivedClass);
            }
        }

        // Setters for internal use by the ReflectionSystem during registration. These should not be exposed publicly as they can break the integrity of the reflection data if used incorrectly.
        void SetFactory(MEClassFactoryFn inFactory) { m_Factory = inFactory; }
        bool HasFactory() const { return m_Factory != nullptr; }

        void SetCaster(MEClassCasterFn inCaster) { m_Caster = inCaster; }
        bool HasCaster() const { return m_Caster != nullptr; }

        void SetSharedPtrSetter(MEClassSetSharedPtrFn inSetSharedPtr) { m_SetSharedPtr = inSetSharedPtr; }
        bool HasSharedPtrSetter() const { return m_SetSharedPtr != nullptr; }


        template<typename T>
        static std::shared_ptr<void> CreateDefaultInstance_Impl()
        {
            if constexpr (std::is_default_constructible_v<T> && !std::is_abstract_v<T>)
            {
                std::shared_ptr<T> instance = std::make_shared<T>();
                if constexpr (std::is_base_of_v<minEngine::MEObject, T>)
                {
                    instance->SetClass(T::StaticClass());
                }
                return instance;
            }
            else
            {
                return nullptr;
            }
        }
        template<typename T>
        static void* CastObject_Impl(void* objectPtr)
        {
            if (objectPtr == nullptr)
            {
                return nullptr;
            }

            T* typedPtr = static_cast<T*>(objectPtr);
            return static_cast<void*>(typedPtr);
        }
        template<typename T>
        static bool SetSharedPtr_Impl(const std::shared_ptr<void>& src, void* dst)
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
        

    private:
        MEClassFactoryFn m_Factory = nullptr;
        MEClassCasterFn m_Caster = nullptr;
        MEClassSetSharedPtrFn m_SetSharedPtr = nullptr;
        ClassSpecifierMask m_SpecifierMask = static_cast<ClassSpecifierMask>(ClassSpecifier::None);
        ClassMetadata m_Metadata;
        MEClass* m_SuperClass = nullptr;
        std::vector<MEProperty*> m_Properties;
        std::vector<MEFunction*> m_Functions;
        std::unordered_map<std::string, MEFunction*> m_FunctionsByName;
        std::vector<MEClass*> m_DirectDerivedClasses;
    };

}
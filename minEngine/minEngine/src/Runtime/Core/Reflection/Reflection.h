#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <initializer_list>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <typeinfo>

#include "Math/Math.h"

namespace minEngine::Reflection
{
    using SharedFactoryFn = std::shared_ptr<void> (*)();
    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);

    using MetadataMap = std::unordered_map<std::string, std::string>;

    inline std::pair<std::string, std::string> MetaKV(const char* key, const char* value)
    {
        return { key, value };
    }

    inline MetadataMap BuildMetadata(std::initializer_list<std::pair<std::string, std::string>> entries)
    {
        MetadataMap metadata;
        for (const auto& entry : entries)
        {
            metadata[entry.first] = entry.second;
        }
        return metadata;
    }

    struct FieldInfo
    {
        std::string name;
        std::string typeName;
        FieldConstAccessorFn constAccessor = nullptr;
        FieldMutableAccessorFn mutableAccessor = nullptr;
        MetadataMap metadata;

        const std::string* FindMetadata(const std::string& key) const
        {
            const auto iter = metadata.find(key);
            if (iter == metadata.end())
            {
                return nullptr;
            }
            return &iter->second;
        }
    };

    struct TypeInfo
    {
        std::string name;
        size_t size = 0;
        std::vector<FieldInfo> fields;
        SharedFactoryFn createInstance = nullptr;

        const FieldInfo* FindField(const std::string& fieldName) const
        {
            for (const auto& field : fields)
            {
                if (field.name == fieldName)
                {
                    return &field;
                }
            }
            return nullptr;
        }

        bool CanCreateInstance() const
        {
            return createInstance != nullptr;
        }

        std::shared_ptr<void> CreateInstance() const
        {
            return createInstance ? createInstance() : std::shared_ptr<void>();
        }
    };

    struct EnumValueInfo
    {
        std::string name;
        int64_t value = 0;
    };

    struct EnumInfo
    {
        std::string name;
        std::vector<EnumValueInfo> entries;

        const EnumValueInfo* FindByName(const std::string& enumName) const
        {
            for (const auto& entry : entries)
            {
                if (entry.name == enumName)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        const EnumValueInfo* FindByValue(int64_t enumValue) const
        {
            for (const auto& entry : entries)
            {
                if (entry.value == enumValue)
                {
                    return &entry;
                }
            }
            return nullptr;
        }
    };

    template<typename T>
    inline std::string GetTypeName()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        return typeid(RawType).name();
    }

    template<>
    inline std::string GetTypeName<bool>()
    {
        return "bool";
    }

    template<>
    inline std::string GetTypeName<int>()
    {
        return "int";
    }

    template<>
    inline std::string GetTypeName<float>()
    {
        return "float";
    }

    template<>
    inline std::string GetTypeName<double>()
    {
        return "double";
    }

    template<>
    inline std::string GetTypeName<std::string>()
    {
        return "std::string";
    }

    template<>
    inline std::string GetTypeName<Vector2>()
    {
        return "Vector2";
    }

    template<>
    inline std::string GetTypeName<Vector3>()
    {
        return "Vector3";
    }

    template<>
    inline std::string GetTypeName<Vector4>()
    {
        return "Vector4";
    }

    template<typename T>
    inline std::shared_ptr<void> CreateDefaultInstance()
    {
        if constexpr (std::is_default_constructible_v<T> && !std::is_abstract_v<T>)
        {
            return std::make_shared<T>();
        }
        else
        {
            return std::shared_ptr<void>();
        }
    }

    class ReflectionSystem
    {
    public:
        static ReflectionSystem& Get()
        {
            static ReflectionSystem system;
            return system;
        }

        template<typename T>
        void RegisterType(TypeInfo info, SharedFactoryFn createFn = nullptr)
        {
            const std::string declaredName = info.name;
            const std::string typeIdName = typeid(T).name();

            info.createInstance = createFn;

            m_TypeInfoByDeclaredName[declaredName] = std::move(info);
            m_DeclaredNameByTypeId[typeIdName] = declaredName;
        }

        template<typename T>
        static const TypeInfo* StaticClass()
        {
            return Get().GetTypeInfo<T>();
        }

        static const TypeInfo* StaticClass(const std::string& declaredTypeName)
        {
            return Get().GetTypeInfo(declaredTypeName);
        }

        const TypeInfo* GetTypeInfo(const std::string& declaredName) const
        {
            const auto iter = m_TypeInfoByDeclaredName.find(declaredName);
            if (iter == m_TypeInfoByDeclaredName.end())
            {
                return nullptr;
            }

            return &iter->second;
        }

        template<typename T>
        const TypeInfo* GetTypeInfo() const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeid(T).name());
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return nullptr;
            }

            return GetTypeInfo(typeIdIter->second);
        }

        const TypeInfo* GetTypeInfoByTypeId(const std::string& typeId) const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeId);
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return nullptr;
            }

            return GetTypeInfo(typeIdIter->second);
        }

        const std::unordered_map<std::string, TypeInfo>& GetAllTypeInfo() const
        {
            return m_TypeInfoByDeclaredName;
        }

        std::shared_ptr<void> CreateInstance(const std::string& declaredTypeName) const
        {
            const TypeInfo* typeInfo = GetTypeInfo(declaredTypeName);
            if (typeInfo == nullptr)
            {
                return std::shared_ptr<void>();
            }
            return typeInfo->CreateInstance();
        }

        template<typename TBase>
        std::shared_ptr<TBase> CreateInstanceAs(const std::string& declaredTypeName) const
        {
            std::shared_ptr<void> instance = CreateInstance(declaredTypeName);
            if (!instance)
            {
                return nullptr;
            }
            return std::shared_ptr<TBase>(instance, static_cast<TBase*>(instance.get()));
        }

        template<typename T>
        void RegisterEnum(EnumInfo info)
        {
            const std::string declaredName = info.name;
            const std::string typeIdName = typeid(T).name();

            m_EnumInfoByDeclaredName[declaredName] = std::move(info);
            m_DeclaredEnumNameByTypeId[typeIdName] = declaredName;
        }

        const EnumInfo* GetEnumInfo(const std::string& declaredName) const
        {
            const auto iter = m_EnumInfoByDeclaredName.find(declaredName);
            if (iter == m_EnumInfoByDeclaredName.end())
            {
                return nullptr;
            }
            return &iter->second;
        }

        template<typename T>
        const EnumInfo* GetEnumInfo() const
        {
            const auto typeIdIter = m_DeclaredEnumNameByTypeId.find(typeid(T).name());
            if (typeIdIter == m_DeclaredEnumNameByTypeId.end())
            {
                return nullptr;
            }
            return GetEnumInfo(typeIdIter->second);
        }

        const std::unordered_map<std::string, EnumInfo>& GetAllEnumInfo() const
        {
            return m_EnumInfoByDeclaredName;
        }

        const FieldInfo* FindField(const TypeInfo& typeInfo, const std::string& fieldName) const
        {
            return typeInfo.FindField(fieldName);
        }

        const FieldInfo* FindField(const std::string& declaredTypeName, const std::string& fieldName) const
        {
            const TypeInfo* typeInfo = GetTypeInfo(declaredTypeName);
            if (typeInfo == nullptr)
            {
                return nullptr;
            }
            return typeInfo->FindField(fieldName);
        }

        template<typename TObject>
        const FieldInfo* FindField(const std::string& fieldName) const
        {
            const TypeInfo* typeInfo = GetTypeInfo<TObject>();
            if (typeInfo == nullptr)
            {
                return nullptr;
            }
            return typeInfo->FindField(fieldName);
        }

        static void* GetFieldPtr(void* object, const FieldInfo& field)
        {
            if (object == nullptr || field.mutableAccessor == nullptr)
            {
                return nullptr;
            }
            return field.mutableAccessor(object);
        }

        static const void* GetFieldPtr(const void* object, const FieldInfo& field)
        {
            if (object == nullptr || field.constAccessor == nullptr)
            {
                return nullptr;
            }
            return field.constAccessor(object);
        }

        template<typename TObject, typename TField>
        bool TryGetFieldValue(const TObject& object, const std::string& fieldName, TField& outValue) const
        {
            const FieldInfo* field = FindField<TObject>(fieldName);
            if (field == nullptr)
            {
                return false;
            }

            const void* fieldPtr = GetFieldPtr(&object, *field);
            if (fieldPtr == nullptr)
            {
                return false;
            }

            outValue = *reinterpret_cast<const TField*>(fieldPtr);
            return true;
        }

        template<typename TObject, typename TField>
        bool TrySetFieldValue(TObject& object, const std::string& fieldName, const TField& value) const
        {
            const FieldInfo* field = FindField<TObject>(fieldName);
            if (field == nullptr)
            {
                return false;
            }

            void* fieldPtr = GetFieldPtr(&object, *field);
            if (fieldPtr == nullptr)
            {
                return false;
            }

            *reinterpret_cast<TField*>(fieldPtr) = value;
            return true;
        }

    private:
        std::unordered_map<std::string, TypeInfo> m_TypeInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredNameByTypeId;
        std::unordered_map<std::string, EnumInfo> m_EnumInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredEnumNameByTypeId;
    };
}

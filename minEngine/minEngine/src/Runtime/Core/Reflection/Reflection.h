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
#include <functional>
#include <algorithm>
#include <unordered_set>

#include "Math/Math.h"

namespace minEngine::Reflection
{

    template<typename T>
    struct TypeAccessor;

    using SharedFactoryFn = std::shared_ptr<void> (*)();
    using FieldConstAccessorFn = const void* (*)(const void*);
    using FieldMutableAccessorFn = void* (*)(void*);
    using UpcastConstFn = const void* (*)(const void*);
    using UpcastMutableFn = void* (*)(void*);

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
        struct BaseTypeInfo
        {
            std::string typeName;
            UpcastConstFn constUpcast = nullptr;
            UpcastMutableFn mutableUpcast = nullptr;
        };

        std::string name;
        size_t size = 0;
        std::vector<BaseTypeInfo> directBases;
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
        using FieldVisitorFn = std::function<bool(const TypeInfo&, const FieldInfo&)>;

        struct ResolvedFieldInfo
        {
            const TypeInfo* declaringType = nullptr;
            const FieldInfo* field = nullptr;
        };

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

            RemoveInheritanceEdges(declaredName);

            info.createInstance = createFn;

            m_TypeInfoByDeclaredName[declaredName] = std::move(info);
            m_DeclaredNameByTypeId[typeIdName] = declaredName;
            BuildInheritanceEdges(declaredName);
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

        std::string GetDeclaredTypeNameByTypeId(const std::string& typeId) const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeId);
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return {};
            }

            return typeIdIter->second;
        }

        const std::unordered_map<std::string, TypeInfo>& GetAllTypeInfo() const
        {
            return m_TypeInfoByDeclaredName;
        }

        const std::vector<std::string>& GetDirectBaseTypes(const std::string& declaredTypeName) const
        {
            const auto iter = m_DirectBaseNamesByType.find(declaredTypeName);
            if (iter == m_DirectBaseNamesByType.end())
            {
                static const std::vector<std::string> kEmpty;
                return kEmpty;
            }

            return iter->second;
        }

        const std::vector<std::string>& GetDirectDerivedTypes(const std::string& declaredTypeName) const
        {
            const auto iter = m_DirectDerivedNamesByType.find(declaredTypeName);
            if (iter == m_DirectDerivedNamesByType.end())
            {
                static const std::vector<std::string> kEmpty;
                return kEmpty;
            }

            return iter->second;
        }

        bool IsDerivedFrom(const std::string& declaredTypeName, const std::string& potentialBaseTypeName) const
        {
            if (declaredTypeName.empty() || potentialBaseTypeName.empty())
            {
                return false;
            }

            if (declaredTypeName == potentialBaseTypeName)
            {
                return true;
            }

            std::unordered_set<std::string> visited;
            return IsDerivedFromRecursive(declaredTypeName, potentialBaseTypeName, visited);
        }

        template<typename TDerived, typename TBase>
        bool IsDerivedFrom() const
        {
            const TypeInfo* derivedTypeInfo = GetTypeInfo<TDerived>();
            const TypeInfo* baseTypeInfo = GetTypeInfo<TBase>();
            if (derivedTypeInfo == nullptr || baseTypeInfo == nullptr)
            {
                return false;
            }

            return IsDerivedFrom(derivedTypeInfo->name, baseTypeInfo->name);
        }

        std::vector<std::string> GetAllBaseTypes(const std::string& declaredTypeName) const
        {
            std::vector<std::string> result;
            std::unordered_set<std::string> visited;
            CollectAllBaseTypesRecursive(declaredTypeName, result, visited);
            return result;
        }

        std::vector<std::string> GetAllDerivedTypes(const std::string& declaredTypeName) const
        {
            std::vector<std::string> result;
            std::unordered_set<std::string> visited;
            CollectAllDerivedTypesRecursive(declaredTypeName, result, visited);
            return result;
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

            const TypeInfo* baseTypeInfo = GetTypeInfo<TBase>();
            if (baseTypeInfo == nullptr)
            {
                return std::shared_ptr<TBase>(instance, static_cast<TBase*>(instance.get()));
            }

            void* basePtr = CastObjectToType(instance.get(), declaredTypeName, baseTypeInfo->name);
            if (basePtr == nullptr)
            {
                return nullptr;
            }

            return std::shared_ptr<TBase>(instance, static_cast<TBase*>(basePtr));
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

        std::string GetDeclaredEnumNameByTypeId(const std::string& typeId) const
        {
            const auto typeIdIter = m_DeclaredEnumNameByTypeId.find(typeId);
            if (typeIdIter == m_DeclaredEnumNameByTypeId.end())
            {
                return {};
            }

            return typeIdIter->second;
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

        bool ForEachFieldInHierarchy(const std::string& declaredTypeName, const FieldVisitorFn& visitor, bool baseFirst = true) const
        {
            if (!visitor)
            {
                return false;
            }

            const TypeInfo* typeInfo = GetTypeInfo(declaredTypeName);
            if (typeInfo == nullptr)
            {
                return false;
            }

            std::vector<const TypeInfo*> typeOrder;
            std::unordered_set<std::string> visited;
            BuildTypeTraversalOrder(*typeInfo, baseFirst, typeOrder, visited);

            for (const TypeInfo* orderedType : typeOrder)
            {
                for (const FieldInfo& field : orderedType->fields)
                {
                    if (!visitor(*orderedType, field))
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        bool FindFieldInHierarchy(const std::string& declaredTypeName, const std::string& fieldName, ResolvedFieldInfo& outField, bool requireUnique = true) const
        {
            outField = {};

            bool found = false;
            bool duplicate = false;
            ForEachFieldInHierarchy(declaredTypeName,
                [&](const TypeInfo& typeInfo, const FieldInfo& field)
                {
                    if (field.name != fieldName)
                    {
                        return true;
                    }

                    if (!found)
                    {
                        outField.declaringType = &typeInfo;
                        outField.field = &field;
                        found = true;
                        return true;
                    }

                    duplicate = true;
                    return false;
                });

            if (!found)
            {
                return false;
            }

            if (requireUnique && duplicate)
            {
                outField = {};
                return false;
            }

            return true;
        }

        bool FindFieldQualified(const std::string& rootTypeName,
                                const std::string& declaringTypeName,
                                const std::string& fieldName,
                                ResolvedFieldInfo& outField) const
        {
            outField = {};

            if (!IsDerivedFrom(rootTypeName, declaringTypeName))
            {
                return false;
            }

            const TypeInfo* declaringType = GetTypeInfo(declaringTypeName);
            if (declaringType == nullptr)
            {
                return false;
            }

            const FieldInfo* field = declaringType->FindField(fieldName);
            if (field == nullptr)
            {
                return false;
            }

            outField.declaringType = declaringType;
            outField.field = field;
            return true;
        }

        const void* CastObjectToType(const void* object,
                                     const std::string& sourceTypeName,
                                     const std::string& targetTypeName) const
        {
            if (object == nullptr)
            {
                return nullptr;
            }

            if (sourceTypeName == targetTypeName)
            {
                return object;
            }

            const TypeInfo* sourceType = GetTypeInfo(sourceTypeName);
            const TypeInfo* targetType = GetTypeInfo(targetTypeName);
            if (sourceType == nullptr || targetType == nullptr)
            {
                return nullptr;
            }

            std::unordered_set<std::string> visited;
            return CastObjectToTypeRecursive(object, *sourceType, *targetType, visited);
        }

        void* CastObjectToType(void* object,
                               const std::string& sourceTypeName,
                               const std::string& targetTypeName) const
        {
            return const_cast<void*>(
                CastObjectToType(static_cast<const void*>(object), sourceTypeName, targetTypeName));
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
        void RemoveInheritanceEdges(const std::string& declaredName)
        {
            const auto baseIter = m_DirectBaseNamesByType.find(declaredName);
            if (baseIter != m_DirectBaseNamesByType.end())
            {
                for (const std::string& baseName : baseIter->second)
                {
                    auto derivedIter = m_DirectDerivedNamesByType.find(baseName);
                    if (derivedIter == m_DirectDerivedNamesByType.end())
                    {
                        continue;
                    }

                    std::vector<std::string>& derivedList = derivedIter->second;
                    derivedList.erase(std::remove(derivedList.begin(), derivedList.end(), declaredName), derivedList.end());
                    if (derivedList.empty())
                    {
                        m_DirectDerivedNamesByType.erase(derivedIter);
                    }
                }
            }

            m_DirectBaseNamesByType.erase(declaredName);
        }

        void BuildInheritanceEdges(const std::string& declaredName)
        {
            const TypeInfo* typeInfo = GetTypeInfo(declaredName);
            if (typeInfo == nullptr)
            {
                return;
            }

            std::vector<std::string> directBases;
            directBases.reserve(typeInfo->directBases.size());

            for (const TypeInfo::BaseTypeInfo& baseInfo : typeInfo->directBases)
            {
                if (baseInfo.typeName.empty())
                {
                    continue;
                }

                directBases.push_back(baseInfo.typeName);
                m_DirectDerivedNamesByType[baseInfo.typeName].push_back(declaredName);
            }

            if (!directBases.empty())
            {
                m_DirectBaseNamesByType[declaredName] = std::move(directBases);
            }
        }

        bool IsDerivedFromRecursive(const std::string& declaredTypeName,
                                    const std::string& potentialBaseTypeName,
                                    std::unordered_set<std::string>& visited) const
        {
            if (!visited.insert(declaredTypeName).second)
            {
                return false;
            }

            const auto baseIter = m_DirectBaseNamesByType.find(declaredTypeName);
            if (baseIter == m_DirectBaseNamesByType.end())
            {
                return false;
            }

            for (const std::string& baseName : baseIter->second)
            {
                if (baseName == potentialBaseTypeName)
                {
                    return true;
                }

                if (IsDerivedFromRecursive(baseName, potentialBaseTypeName, visited))
                {
                    return true;
                }
            }

            return false;
        }

        void CollectAllBaseTypesRecursive(const std::string& declaredTypeName,
                                          std::vector<std::string>& outBaseTypes,
                                          std::unordered_set<std::string>& visited) const
        {
            const auto baseIter = m_DirectBaseNamesByType.find(declaredTypeName);
            if (baseIter == m_DirectBaseNamesByType.end())
            {
                return;
            }

            for (const std::string& baseName : baseIter->second)
            {
                if (!visited.insert(baseName).second)
                {
                    continue;
                }

                outBaseTypes.push_back(baseName);
                CollectAllBaseTypesRecursive(baseName, outBaseTypes, visited);
            }
        }

        void CollectAllDerivedTypesRecursive(const std::string& declaredTypeName,
                                             std::vector<std::string>& outDerivedTypes,
                                             std::unordered_set<std::string>& visited) const
        {
            const auto derivedIter = m_DirectDerivedNamesByType.find(declaredTypeName);
            if (derivedIter == m_DirectDerivedNamesByType.end())
            {
                return;
            }

            for (const std::string& derivedName : derivedIter->second)
            {
                if (!visited.insert(derivedName).second)
                {
                    continue;
                }

                outDerivedTypes.push_back(derivedName);
                CollectAllDerivedTypesRecursive(derivedName, outDerivedTypes, visited);
            }
        }

        void BuildTypeTraversalOrder(const TypeInfo& typeInfo,
                                     bool baseFirst,
                                     std::vector<const TypeInfo*>& outOrder,
                                     std::unordered_set<std::string>& visited) const
        {
            if (!visited.insert(typeInfo.name).second)
            {
                return;
            }

            if (!baseFirst)
            {
                outOrder.push_back(&typeInfo);
            }

            for (const TypeInfo::BaseTypeInfo& baseInfo : typeInfo.directBases)
            {
                const TypeInfo* baseType = GetTypeInfo(baseInfo.typeName);
                if (baseType == nullptr)
                {
                    continue;
                }

                BuildTypeTraversalOrder(*baseType, baseFirst, outOrder, visited);
            }

            if (baseFirst)
            {
                outOrder.push_back(&typeInfo);
            }
        }

        const void* CastObjectToTypeRecursive(const void* object,
                                              const TypeInfo& sourceType,
                                              const TypeInfo& targetType,
                                              std::unordered_set<std::string>& visited) const
        {
            if (!visited.insert(sourceType.name).second)
            {
                return nullptr;
            }

            if (sourceType.name == targetType.name)
            {
                return object;
            }

            for (const TypeInfo::BaseTypeInfo& baseInfo : sourceType.directBases)
            {
                if (baseInfo.constUpcast == nullptr)
                {
                    continue;
                }

                const TypeInfo* baseType = GetTypeInfo(baseInfo.typeName);
                if (baseType == nullptr)
                {
                    continue;
                }

                const void* baseObject = baseInfo.constUpcast(object);
                if (baseObject == nullptr)
                {
                    continue;
                }

                const void* casted = CastObjectToTypeRecursive(baseObject, *baseType, targetType, visited);
                if (casted != nullptr)
                {
                    return casted;
                }
            }

            return nullptr;
        }

        std::unordered_map<std::string, TypeInfo> m_TypeInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredNameByTypeId;
        std::unordered_map<std::string, std::vector<std::string>> m_DirectBaseNamesByType;
        std::unordered_map<std::string, std::vector<std::string>> m_DirectDerivedNamesByType;
        std::unordered_map<std::string, EnumInfo> m_EnumInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredEnumNameByTypeId;
    };

    template<typename T>
    inline std::string GetTypeName()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        ReflectionSystem& system = ReflectionSystem::Get();
        const std::string typeIdName = typeid(RawType).name();
        const std::string declaredTypeName = system.GetDeclaredTypeNameByTypeId(typeIdName);
        if (!declaredTypeName.empty())
        {
            return declaredTypeName;
        }

        return typeIdName;
    }

    template<typename T>
    inline std::string GetEnumName()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;
        static_assert(std::is_enum_v<RawType>, "GetEnumName<T>() requires an enum type.");

        ReflectionSystem& system = ReflectionSystem::Get();
        const std::string typeIdName = typeid(RawType).name();
        const std::string declaredEnumName = system.GetDeclaredEnumNameByTypeId(typeIdName);
        if (!declaredEnumName.empty())
        {
            return declaredEnumName;
        }

        return typeIdName;
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
}

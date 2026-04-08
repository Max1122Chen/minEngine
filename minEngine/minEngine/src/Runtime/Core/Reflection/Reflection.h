#pragma once

#include "ReflectionTypes.h"

namespace minEngine::Reflection
{
    class ReflectionSystem
    {
    public:

        static ReflectionSystem& Get()
        {
            static ReflectionSystem system;
            return system;
        }

        // Type registration and querying
        template<typename T>
        void RegisterType(TypeInfo info)
        {
            const std::string declaredName = info.typeName;
            const std::string typeIdName = typeid(T).name();

            m_TypeInfoByDeclaredName[declaredName] = std::move(info);
            m_DeclaredNameByTypeId[typeIdName] = declaredName;
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

        // Helper function to get field pointer
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

        // Type Inheritance related
        const std::vector<BaseTypeInfo>* GetDirectBaseTypes(const std::string& declaredName) const
        {
            const auto it = m_TypeInfoByDeclaredName.find(declaredName);
            if (it == m_TypeInfoByDeclaredName.end())
            {
                return nullptr;
            }
            return &it->second.directBases;
        }

        template<typename T>
        const std::vector<BaseTypeInfo>* GetDirectBaseTypes() const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeid(T).name());
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return nullptr;
            }
            return GetDirectBaseTypes(typeIdIter->second);
        }

        bool ForEachFieldInHierarchy(const std::string& rootTypeName, const FieldVisitorFn& visitor) const
        {
            if (!visitor)
            {
                return false;
            }

            const TypeInfo* rootTypeInfo = GetTypeInfo(rootTypeName);
            if (rootTypeInfo == nullptr)
            {
                return false;
            }

            std::unordered_set<std::string> visitedTypeNames;
            return ForEachFieldInHierarchy_Recursive(*rootTypeInfo, visitor, visitedTypeNames);
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

            std::unordered_set<std::string> visitedTypeNames;
            return CastObjectToType_Recursive(object, *sourceType, *targetType, visitedTypeNames);
        }

        void* CastObjectToType(void* object,
                               const std::string& sourceTypeName,
                               const std::string& targetTypeName) const
        {
            return const_cast<void*>(
                CastObjectToType(static_cast<const void*>(object), sourceTypeName, targetTypeName));
        }

        

        // Enum registration and querying
        template<typename T>
        void RegisterEnum(EnumInfo info)
        {
            const std::string declaredName = info.enumName;
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

    private:

        bool ForEachFieldInHierarchy_Recursive(const TypeInfo& typeInfo,
                                               const FieldVisitorFn& visitor,
                                               std::unordered_set<std::string>& visitedTypeNames) const
        {
            if (!visitedTypeNames.insert(typeInfo.typeName).second)
            {
                return true;
            }

            for (const BaseTypeInfo& baseInfo : typeInfo.directBases)
            {
                const TypeInfo* baseType = GetTypeInfo(baseInfo.typeName);
                if (baseType == nullptr)
                {
                    continue;
                }

                if (!ForEachFieldInHierarchy_Recursive(*baseType, visitor, visitedTypeNames))
                {
                    return false;
                }
            }

            for (const FieldInfo& field : typeInfo.fields)
            {
                if (!visitor(field))
                {
                    return false;
                }
            }

            return true;
        }

        const void* CastObjectToType_Recursive(const void* object,
                                               const TypeInfo& sourceType,
                                               const TypeInfo& targetType,
                                               std::unordered_set<std::string>& visitedTypeNames) const
        {
            if (!visitedTypeNames.insert(sourceType.typeName).second)
            {
                return nullptr;
            }

            if (sourceType.typeName == targetType.typeName)
            {
                return object;
            }

            for (const BaseTypeInfo& baseInfo : sourceType.directBases)
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

                const void* castedObject = CastObjectToType_Recursive(baseObject, *baseType, targetType, visitedTypeNames);
                if (castedObject != nullptr)
                {
                    return castedObject;
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

    inline const TypeInfo* GetTypeInfo(const std::string& declaredName)
    {
        return ReflectionSystem::Get().GetTypeInfo(declaredName);
    }

    template<typename T>
    inline const TypeInfo* GetTypeInfo()
    {
        return ReflectionSystem::Get().GetTypeInfo<T>();
    }

    template<typename T>
    inline static std::string GetTypeName()
    {
        static_assert(!std::is_same_v<T, T>, "GetTypeName is not implemented for this type.");
        return "";
    }

    // Explicit specialization of GetTypeName for some common types.
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

    bool ForEachFieldInHierarchy(const std::string& rootTypeName, const FieldVisitorFn& visitor);

    const void* CastObjectToType(const void* object, const std::string& sourceTypeName, const std::string& targetTypeName);

    void* CastObjectToType(void* object, const std::string& sourceTypeName, const std::string& targetTypeName);

    template<typename T>
    inline static const EnumInfo* GetEnumInfo()
    {
        return ReflectionSystem::Get().GetEnumInfo<T>();
    }

    inline static const EnumInfo* GetEnumInfo(const std::string& declaredName)
    {
        return ReflectionSystem::Get().GetEnumInfo(declaredName);
    }
}

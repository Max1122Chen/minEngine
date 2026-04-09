#pragma once

#include "ReflectionTypes.h"
#include "TypeTraits.h"

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

            if(info.createInstance == nullptr)
            {
                info.createInstance = &CreateDefaultInstance<T>;
            }

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

        const TypeInfo* GetTypeInfoByTypeId(const std::string& typeIdName) const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeIdName);
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return nullptr;
            }

            return GetTypeInfo(typeIdIter->second);
        }

        std::string GetDeclaredTypeNameByTypeId(const std::string& typeIdName) const
        {
            const auto typeIdIter = m_DeclaredNameByTypeId.find(typeIdName);
            if (typeIdIter == m_DeclaredNameByTypeId.end())
            {
                return {};
            }

            return typeIdIter->second;
        }

        std::shared_ptr<void> CreateInstance(const std::string& declaredName) const
        {
            const TypeInfo* typeInfo = GetTypeInfo(declaredName);
            if (typeInfo == nullptr || typeInfo->createInstance == nullptr)
            {
                return nullptr;
            }

            return typeInfo->createInstance();
        }

        template<typename TBase>
        std::shared_ptr<TBase> CreateInstanceAs(const std::string& declaredName) const
        {
            std::shared_ptr<void> instance = CreateInstance(declaredName);
            if (!instance)
            {
                return nullptr;
            }

            const TypeInfo* baseTypeInfo = GetTypeInfo<TBase>();
            if (baseTypeInfo == nullptr)
            {
                return std::shared_ptr<TBase>(instance, static_cast<TBase*>(instance.get()));
            }

            void* basePtr = CastObjectToType(instance.get(), declaredName, baseTypeInfo->typeName);
            if (basePtr == nullptr)
            {
                return nullptr;
            }

            return std::shared_ptr<TBase>(instance, static_cast<TBase*>(basePtr));
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

        // Array type registration and querying
        void RegisterArrayType(ArrayTypeInfo info)
        {
            const std::string declaredName = info.typeName;
            if (declaredName.empty())
            {
                return;
            }

            m_ArrayTypeInfoByDeclaredName[declaredName] = std::move(info);
        }

        const ArrayTypeInfo* GetArrayTypeInfo(const std::string& declaredName) const
        {
            const auto iter = m_ArrayTypeInfoByDeclaredName.find(declaredName);
            if (iter == m_ArrayTypeInfoByDeclaredName.end())
            {
                return nullptr;
            }
            return &iter->second;
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

        std::unordered_map<std::string, ArrayTypeInfo> m_ArrayTypeInfoByDeclaredName;

        std::unordered_map<std::string, std::vector<std::string>> m_DirectBaseNamesByType;
        std::unordered_map<std::string, std::vector<std::string>> m_DirectDerivedNamesByType;

        std::unordered_map<std::string, EnumInfo> m_EnumInfoByDeclaredName;
        std::unordered_map<std::string, std::string> m_DeclaredEnumNameByTypeId;
    };

    template<typename T>
    inline const TypeCategory GetTypeCategory()
    {
        if constexpr (std::is_arithmetic_v<T>           ||
                      std::is_same_v<T, std::string>    ||
                      std::is_same_v<T, Vector2>        || // TODO: currently treating glm::vec2/3/4 as primitive types for simplicity, but we might want to have special handling for them in the future (e.g. to display them nicely in editor UI)
                      std::is_same_v<T, Vector3>        ||
                      std::is_same_v<T, Vector4>
                    )
        {
            return TypeCategory::Primitive;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            return TypeCategory::Enum;
        }
        else if constexpr (std::is_pointer_v<T>)
        {
            return TypeCategory::Pointer;
        }
        else if constexpr (minEngine::is_vector<T>::value)
        {
            return TypeCategory::Array;
        }
        else if constexpr (std::is_class_v<T>)
        {
            return TypeCategory::Object;
        }
        else 
        {
            static_assert(!sizeof(T), "Unsupported field type");
            return TypeCategory::Unknown;
        }
    }

    inline const TypeInfo* GetTypeInfo(const std::string& declaredName)
    {
        return ReflectionSystem::Get().GetTypeInfo(declaredName);
    }

    template<typename T>
    inline const TypeInfo* GetTypeInfo()
    {
        return ReflectionSystem::Get().GetTypeInfo<T>();
    }

    inline const TypeInfo* GetTypeInfoByTypeId(const std::string& typeIdName)
    {
        return ReflectionSystem::Get().GetTypeInfoByTypeId(typeIdName);
    }

    inline std::string GetDeclaredTypeNameByTypeId(const std::string& typeIdName)
    {
        return ReflectionSystem::Get().GetDeclaredTypeNameByTypeId(typeIdName);
    }

    inline std::shared_ptr<void> CreateInstance(const std::string& declaredName)
    {
        return ReflectionSystem::Get().CreateInstance(declaredName);
    }

    template<typename TBase>
    inline std::shared_ptr<TBase> CreateInstanceAs(const std::string& declaredName)
    {
        return ReflectionSystem::Get().CreateInstanceAs<TBase>(declaredName);
    }

    template<typename T>
    inline static std::string GetTypeName()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;

        if constexpr (is_vector<RawType>::value)
        {
            using ElementType = typename is_vector<RawType>::ElementType;
            return std::string("std::vector<") + GetTypeName<ElementType>() + ">";
        }
        else
        {
            if (const TypeInfo* typeInfo = ReflectionSystem::Get().GetTypeInfo<RawType>())
            {
                return typeInfo->typeName;
            }

            if (const EnumInfo* enumInfo = ReflectionSystem::Get().GetEnumInfo<RawType>())
            {
                return enumInfo->enumName;
            }

            // Return the raw type name as a fallback. This can lead to different strings for the same type across different compilers or even different runs, but it ensures that we always get some kind of name for any type.
            return typeid(RawType).name();
        }
    }

    template<typename T>
    inline void TryRegisterArrayType()
    {
        using RawType = std::remove_cv_t<std::remove_reference_t<T>>;

        if constexpr (is_vector<RawType>::value)
        {
            using ElementType = typename is_vector<RawType>::ElementType;

            ArrayTypeInfo arrayInfo;
            arrayInfo.typeName = GetTypeName<RawType>();
            arrayInfo.elementTypeName = GetTypeName<ElementType>();
            arrayInfo.elementCategory = GetTypeCategory<ElementType>();

            arrayInfo.getSize = [](const void* arrayObject) -> size_t
            {
                if (arrayObject == nullptr)
                {
                    return 0;
                }
                const RawType* typedArray = static_cast<const RawType*>(arrayObject);
                return typedArray->size();
            };
            arrayInfo.getConstElement = [](const void* arrayObject, size_t index) -> const void*
            {
                if (arrayObject == nullptr)
                {
                    return nullptr;
                }

                const RawType* typedArray = static_cast<const RawType*>(arrayObject);
                if (index >= typedArray->size())
                {
                    return nullptr;
                }

                return static_cast<const void*>(&((*typedArray)[index]));
            };
            arrayInfo.resize = [](void* arrayObject, size_t newSize)
            {
                if (arrayObject == nullptr)
                {
                    return;
                }
                RawType* typedArray = static_cast<RawType*>(arrayObject);
                typedArray->resize(newSize);
            };
            arrayInfo.getMutableElement = [](void* arrayObject, size_t index) -> void*
            {
                if (arrayObject == nullptr)
                {
                    return nullptr;
                }

                RawType* typedArray = static_cast<RawType*>(arrayObject);
                if (index >= typedArray->size())
                {
                    return nullptr;
                }

                return static_cast<void*>(&((*typedArray)[index]));
            };

            ReflectionSystem::Get().RegisterArrayType(std::move(arrayInfo));
        }
    }

    inline const ArrayTypeInfo* GetArrayTypeInfo(const std::string& declaredName)
    {
        return ReflectionSystem::Get().GetArrayTypeInfo(declaredName);
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







    // ------------------------------------------------------------
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
}

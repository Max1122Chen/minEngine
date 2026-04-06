#pragma once

#include "Reflection.h"

#define ME_REFLECT_CONCAT_INNER(a, b) a##b
#define ME_REFLECT_CONCAT(a, b) ME_REFLECT_CONCAT_INNER(a, b)

#define ME_REFLECT_TYPE_BEGIN(TYPE) \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECT_CONCAT(_me_reflect_registered_line_, __COUNTER__) = []() \
    { \
        minEngine::Reflection::TypeInfo typeInfo; \
        typeInfo.name = #TYPE; \
        typeInfo.size = sizeof(TYPE);

#define ME_REFLECT_BASE(TYPE, BASE) \
        { \
            minEngine::Reflection::TypeInfo::BaseTypeInfo baseInfo; \
            baseInfo.typeName = #BASE; \
            baseInfo.constUpcast = [](const void* object) -> const void* { \
                const TYPE* typedObject = static_cast<const TYPE*>(object); \
                const BASE* baseObject = static_cast<const BASE*>(typedObject); \
                return static_cast<const void*>(baseObject); \
            }; \
            baseInfo.mutableUpcast = [](void* object) -> void* { \
                TYPE* typedObject = static_cast<TYPE*>(object); \
                BASE* baseObject = static_cast<BASE*>(typedObject); \
                return static_cast<void*>(baseObject); \
            }; \
            typeInfo.directBases.push_back(std::move(baseInfo)); \
        }

#define ME_REFLECT_ACCESSOR_BEGIN(TYPE) \
namespace minEngine::Reflection \
{ \
    template<> \
    struct TypeAccessor<TYPE> \
    {

#define ME_REFLECT_ACCESSOR_FIELD(TYPE, FIELD) \
        static const void* ME_REFLECT_CONCAT(GetConst_, FIELD)(const void* object) \
        { \
            const TYPE* typedObject = static_cast<const TYPE*>(object); \
            return static_cast<const void*>(&(typedObject->FIELD)); \
        } \
\
        static void* ME_REFLECT_CONCAT(GetMutable_, FIELD)(void* object) \
        { \
            TYPE* typedObject = static_cast<TYPE*>(object); \
            return static_cast<void*>(&(typedObject->FIELD)); \
        }

#define ME_REFLECT_ACCESSOR_END() \
    }; \
}

#define ME_REFLECT_FIELD_T(TYPE, FIELD, FIELD_TYPE) \
        { \
            minEngine::Reflection::FieldInfo fieldInfo; \
            fieldInfo.name = #FIELD; \
            fieldInfo.typeName = minEngine::Reflection::GetTypeName<FIELD_TYPE>(); \
            fieldInfo.constAccessor = &minEngine::Reflection::TypeAccessor<TYPE>::ME_REFLECT_CONCAT(GetConst_, FIELD); \
            fieldInfo.mutableAccessor = &minEngine::Reflection::TypeAccessor<TYPE>::ME_REFLECT_CONCAT(GetMutable_, FIELD); \
            typeInfo.fields.push_back(std::move(fieldInfo)); \
        }

#define ME_REFLECT_FIELD_META_T(TYPE, FIELD, FIELD_TYPE, ...) \
        { \
            minEngine::Reflection::FieldInfo fieldInfo; \
            fieldInfo.name = #FIELD; \
            fieldInfo.typeName = minEngine::Reflection::GetTypeName<FIELD_TYPE>(); \
            fieldInfo.constAccessor = &minEngine::Reflection::TypeAccessor<TYPE>::ME_REFLECT_CONCAT(GetConst_, FIELD); \
            fieldInfo.mutableAccessor = &minEngine::Reflection::TypeAccessor<TYPE>::ME_REFLECT_CONCAT(GetMutable_, FIELD); \
            fieldInfo.metadata = minEngine::Reflection::BuildMetadata({ __VA_ARGS__ }); \
            typeInfo.fields.push_back(std::move(fieldInfo)); \
        }

#define ME_REFLECT_FIELD(TYPE, FIELD) \
        ME_REFLECT_FIELD_T(TYPE, FIELD, decltype(std::declval<TYPE>().FIELD))

#define ME_REFLECT_FIELD_META(TYPE, FIELD, ...) \
    ME_REFLECT_FIELD_META_T(TYPE, FIELD, decltype(std::declval<TYPE>().FIELD), __VA_ARGS__)

#define ME_REFLECT_TYPE_END(TYPE) \
        minEngine::Reflection::ReflectionSystem::Get().RegisterType<TYPE>( \
            std::move(typeInfo), \
            &minEngine::Reflection::CreateDefaultInstance<TYPE> \
        ); \
        return true; \
    }(); \
}

#define ME_REFLECT_ENUM_BEGIN(ENUM_TYPE) \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECT_CONCAT(_me_reflect_enum_registered_line_, __COUNTER__) = []() \
    { \
        minEngine::Reflection::EnumInfo enumInfo; \
        enumInfo.name = #ENUM_TYPE;

#define ME_REFLECT_ENUM_VALUE(VALUE_NAME, VALUE_EXPR) \
        enumInfo.entries.push_back(minEngine::Reflection::EnumValueInfo { \
            #VALUE_NAME, \
            static_cast<int64_t>(VALUE_EXPR) \
        });

#define ME_REFLECT_ENUM_END(ENUM_TYPE) \
        minEngine::Reflection::ReflectionSystem::Get().RegisterEnum<ENUM_TYPE>(std::move(enumInfo)); \
        return true; \
    }(); \
}



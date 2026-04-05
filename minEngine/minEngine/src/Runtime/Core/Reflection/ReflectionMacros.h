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

#define ME_REFLECT_FIELD(TYPE, FIELD) \
        typeInfo.fields.push_back(minEngine::Reflection::FieldInfo { \
            #FIELD, \
            minEngine::Reflection::GetTypeName<decltype(TYPE::FIELD)>(), \
            offsetof(TYPE, FIELD), \
            {} \
        });

#define ME_REFLECT_FIELD_META(TYPE, FIELD, ...) \
        { \
            minEngine::Reflection::FieldInfo fieldInfo; \
            fieldInfo.name = #FIELD; \
            fieldInfo.typeName = minEngine::Reflection::GetTypeName<decltype(TYPE::FIELD)>(); \
            fieldInfo.offset = offsetof(TYPE, FIELD); \
            fieldInfo.metadata = minEngine::Reflection::BuildMetadata({ __VA_ARGS__ }); \
            typeInfo.fields.push_back(std::move(fieldInfo)); \
        }

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



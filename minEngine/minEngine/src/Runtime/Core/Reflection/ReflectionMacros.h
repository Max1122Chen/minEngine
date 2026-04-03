#pragma once

#include "Reflection.h"

#define ME_REFLECT_CONCAT_INNER(a, b) a##b
#define ME_REFLECT_CONCAT(a, b) ME_REFLECT_CONCAT_INNER(a, b)

#define ME_REFLECT_TYPE_BEGIN(TYPE) \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECT_CONCAT(_me_reflect_registered_line_, __LINE__) = []() \
    { \
        minEngine::Reflection::TypeInfo typeInfo; \
        typeInfo.name = #TYPE; \
        typeInfo.size = sizeof(TYPE);

#define ME_REFLECT_FIELD(TYPE, FIELD) \
        typeInfo.fields.push_back(minEngine::Reflection::FieldInfo { \
            #FIELD, \
            minEngine::Reflection::GetTypeName<decltype(TYPE::FIELD)>(), \
            offsetof(TYPE, FIELD) \
        });

#define ME_REFLECT_TYPE_END(TYPE) \
        minEngine::Reflection::ReflectionSystem::Get().RegisterType<TYPE>(std::move(typeInfo)); \
        return true; \
    }(); \
}

/*
Usage Example:

struct TransformData
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool visible = true;
};

ME_REFLECT_TYPE_BEGIN(TransformData)
    ME_REFLECT_FIELD(TransformData, x)
    ME_REFLECT_FIELD(TransformData, y)
    ME_REFLECT_FIELD(TransformData, z)
    ME_REFLECT_FIELD(TransformData, visible)
ME_REFLECT_TYPE_END(TransformData)

// Query type info:
// const auto* info = minEngine::Reflection::ReflectionSystem::Get().GetTypeInfo<TransformData>();
// if (info) { for (const auto& field : info->fields) { ... } }
*/

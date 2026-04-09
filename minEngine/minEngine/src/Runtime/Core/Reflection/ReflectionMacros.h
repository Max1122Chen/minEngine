#pragma once

#include "Reflection.h"
#include "ReflectionSerializerBridge.h"

#define ME_REFLECT_CONCAT_INNER(a, b) a##b
#define ME_REFLECT_CONCAT(a, b) ME_REFLECT_CONCAT_INNER(a, b)

#define ME_REFLECT_ACCESSOR_BEGIN(TYPE) \
namespace minEngine::Reflection \
{ \
    template<> \
    struct FieldAccessor<TYPE> \
    {

#define ME_REFLECT_ACCESSOR_FIELD(TYPE, FIELD) \
    using ME_REFLECT_CONCAT(FieldType_, FIELD) = decltype(std::declval<TYPE>().FIELD); \
\
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

#define ME_REFLECT_TYPE_BEGIN(TYPE) \
    template<> \
    inline std::string minEngine::Reflection::GetTypeName<TYPE>() \
    { \
        return #TYPE; \
    } \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECT_CONCAT(_me_reflect_registered_line_, __COUNTER__) = []() \
    { \
        minEngine::Reflection::TypeInfo typeInfo; \
        typeInfo.typeName = #TYPE; \
        typeInfo.size = sizeof(TYPE); \
        typeInfo.category = minEngine::Reflection::GetTypeCategory<TYPE>(); \
        typeInfo.writeToJson = &minEngine::Reflection::WriteToJsonErased<TYPE>; \
        // typeInfo.writePointerToJson = &minEngine::Reflection::WritePointerToJsonErased<TYPE>;

#define ME_REFLECT_TYPE_META(...) \
        { \
            typeInfo.BuildMetadata({ __VA_ARGS__ }); \
        }

#define ME_REFLECT_BASE(TYPE, BASE) \
        { \
            minEngine::Reflection::BaseTypeInfo baseInfo; \
            baseInfo.typeName = #BASE; \
            baseInfo.constUpcast = [](const void* object) -> const void* { \
                const TYPE* typedObject = static_cast<const TYPE*>(object); \
                const BASE* baseObject = static_cast<const BASE*>(typedObject); \
                return static_cast<const void*>(baseObject); \
            }; \
\
            baseInfo.mutableUpcast = [](void* object) -> void* { \
                TYPE* typedObject = static_cast<TYPE*>(object); \
                BASE* baseObject = static_cast<BASE*>(typedObject); \
                return static_cast<void*>(baseObject); \
            }; \
            typeInfo.directBases.push_back(std::move(baseInfo)); \
        }

// Auto-deduced field reflection macro. Metadata entries are optional.
#define ME_REFLECT_FIELD(TYPE, FIELD, ...) \
    { \
            using FIELD_TYPE = typename minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECT_CONCAT(FieldType_, FIELD); \
        minEngine::Reflection::TryRegisterArrayType<FIELD_TYPE>(); \
        minEngine::Reflection::FieldInfo fieldInfo; \
        fieldInfo.fieldName = #FIELD; \
        fieldInfo.fieldTypeName = minEngine::Reflection::GetTypeName<FIELD_TYPE>(); \
        fieldInfo.category = minEngine::Reflection::GetTypeCategory<FIELD_TYPE>(); \
        fieldInfo.constAccessor = &minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECT_CONCAT(GetConst_, FIELD); \
        fieldInfo.mutableAccessor = &minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECT_CONCAT(GetMutable_, FIELD); \
        fieldInfo.BuildMetadata({ __VA_ARGS__ }); \
        typeInfo.fields.push_back(std::move(fieldInfo)); \
    }

#define ME_REFLECT_TYPE_END(TYPE) \
        minEngine::Reflection::ReflectionSystem::Get().RegisterType<TYPE>( \
            std::move(typeInfo) \
        ); \
        return true; \
    }(); \
} \

#define ME_REFLECT_ENUM_BEGIN(ENUM_TYPE) \
    template<> \
    inline std::string minEngine::Reflection::GetTypeName<ENUM_TYPE>() \
    { \
        return #ENUM_TYPE; \
    } \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECT_CONCAT(_me_reflect_enum_registered_line_, __COUNTER__) = []() \
    { \
        minEngine::Reflection::EnumInfo enumInfo; \
        enumInfo.enumName = #ENUM_TYPE;

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



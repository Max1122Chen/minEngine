#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "MEReflection.h"

#define ME_REFLECTION_CONCAT_INNER(a, b) a##b
#define ME_REFLECTION_CONCAT(a, b) ME_REFLECTION_CONCAT_INNER(a, b)

#define ME_REFLECTION_ACCESSOR_BEGIN(TYPE) \
namespace minEngine::MEReflection \
{ \
    template<> \
    struct FieldAccessor<TYPE> \
    {

#define ME_REFLECTION_ACCESSOR_FIELD(TYPE, FIELD) \
    using ME_REFLECTION_CONCAT(FieldType_, FIELD) = decltype(std::declval<TYPE>().FIELD); \
    static const void* ME_REFLECTION_CONCAT(GetConst_, FIELD)(const void* object) \
    { \
        const TYPE* typedObject = static_cast<const TYPE*>(object); \
        return static_cast<const void*>(&(typedObject->FIELD)); \
    } \
    static void* ME_REFLECTION_CONCAT(GetMutable_, FIELD)(void* object) \
    { \
        TYPE* typedObject = static_cast<TYPE*>(object); \
        return static_cast<void*>(&(typedObject->FIELD)); \
    }

#define ME_REFLECTION_ACCESSOR_END() \
    }; \
}

#define ME_REFLECTION_CLASS_BEGIN(TYPE) \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECTION_CONCAT(_me_reflection_registered_line_, __COUNTER__) = []() \
    { \
        auto& meSystem = minEngine::MEReflection::MEReflectionSystem::Get(); \
        minEngine::MEReflection::MEClass* meClass = meSystem.CreateClass(#TYPE);

#define ME_REFLECTION_CLASS_SUPER(SUPER_TYPE) \
        meSystem.AddPendingSuperClass<SUPER_TYPE>(meClass);

#define ME_REFLECTION_CLASS_ADD_FIELD(TYPE, FIELD) \
        { \
            using FIELD_TYPE = typename minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(FieldType_, FIELD); \
            meSystem.AddFieldByType<TYPE, FIELD_TYPE>( \
                meClass, \
                #FIELD, \
                &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD), \
                &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD)); \
        }

#define ME_REFLECTION_CLASS_ADD_PRIMITIVE(TYPE, FIELD, PRIMITIVE_TYPE_NAME) \
        { \
            minEngine::MEReflection::MEPrimitiveProperty* property = \
                meSystem.CreateProperty<minEngine::MEReflection::MEPrimitiveProperty>(#FIELD, PRIMITIVE_TYPE_NAME); \
            property->constAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD); \
            property->mutableAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD); \
            meClass->AddProperty(property); \
        }

#define ME_REFLECTION_CLASS_ADD_OBJECT(TYPE, FIELD, CLASS_TYPE) \
        { \
            minEngine::MEReflection::MEObjectProperty* property = \
                meSystem.CreateProperty<minEngine::MEReflection::MEObjectProperty>(#FIELD); \
            property->constAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD); \
            property->mutableAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD); \
            meClass->AddProperty(property); \
            meSystem.AddPendingPropertyClass<CLASS_TYPE>(meClass, property); \
        }

#define ME_REFLECTION_CLASS_ADD_OBJECT_PTR(TYPE, FIELD, CLASS_TYPE) \
        { \
            minEngine::MEReflection::MEObjectPtrProperty* property = \
                meSystem.CreateProperty<minEngine::MEReflection::MEObjectPtrProperty>(#FIELD); \
            property->constAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD); \
            property->mutableAccessor = &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD); \
            meClass->AddProperty(property); \
            meSystem.AddPendingPropertyClass<CLASS_TYPE>(meClass, property); \
        }

#define ME_REFLECTION_CLASS_ADD_ARRAY(TYPE, FIELD, INNER_TYPE) \
        { \
            using ARRAY_TYPE = std::vector<INNER_TYPE>; \
            meSystem.AddFieldByType<TYPE, ARRAY_TYPE>( \
                meClass, \
                #FIELD, \
                &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD), \
                &minEngine::MEReflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD)); \
        }

#define ME_REFLECTION_CLASS_END(TYPE) \
        return meSystem.RegisterClass<TYPE>(meClass); \
    }(); \
}

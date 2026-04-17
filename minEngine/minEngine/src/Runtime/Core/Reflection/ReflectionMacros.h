#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "Reflection.h"

#define ME_REFLECTION_CONCAT_INNER(a, b) a##b
#define ME_REFLECTION_CONCAT(a, b) ME_REFLECTION_CONCAT_INNER(a, b)

#define ME_REFLECTION_ACCESSOR_BEGIN(TYPE) \
namespace minEngine::Reflection \
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
        auto& meSystem = minEngine::Reflection::ReflectionSystem::Get(); \
        if (meSystem.FindClass(#TYPE) != nullptr) \
        { \
            return true; \
        } \
        minEngine::Reflection::MEClass* meClass = meSystem.CreateClass(#TYPE);

#define ME_REFLECTION_CLASS_SUPER(SUPER_TYPE) \
        meSystem.AddPendingSuperClass<SUPER_TYPE>(meClass);

#define ME_REFLECTION_CLASS_SET_FACTORY(FACTORY_FN) \
    meClass->SetFactory(FACTORY_FN);

#define ME_REFLECTION_CLASS_SET_ANNOTATIONS(SPECIFIER_MASK, ...) \
    meClass->SetAnnotations(SPECIFIER_MASK, __VA_ARGS__);

#define ME_REFLECTION_CLASS_ADD_FIELD(TYPE, FIELD, SPECIFIER_MASK, ...) \
        { \
            using FIELD_TYPE = typename minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(FieldType_, FIELD); \
            meSystem.AddFieldByType<TYPE, FIELD_TYPE>( \
                meClass, \
                #FIELD, \
                &minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetConst_, FIELD), \
                &minEngine::Reflection::FieldAccessor<TYPE>::ME_REFLECTION_CONCAT(GetMutable_, FIELD), \
                SPECIFIER_MASK, \
                __VA_ARGS__); \
        }

#define ME_REFLECTION_CLASS_END(TYPE) \
        return meSystem.RegisterClass<TYPE>(meClass); \
    }(); \
}

#define ME_REFLECTION_ENUM_BEGIN(ENUM_TYPE) \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECTION_CONCAT(_me_reflection_enum_registered_line_, __COUNTER__) = []() \
    { \
        auto& meSystem = minEngine::Reflection::ReflectionSystem::Get(); \
        if (meSystem.FindEnum(#ENUM_TYPE) != nullptr) \
        { \
            return true; \
        } \
        minEngine::Reflection::MEEnum* meEnum = meSystem.CreateEnum(#ENUM_TYPE);

#define ME_REFLECTION_ENUM_VALUE(VALUE_NAME, VALUE_EXPR) \
        meEnum->AddEntry(#VALUE_NAME, static_cast<int64_t>(VALUE_EXPR));

#define ME_REFLECTION_ENUM_END(ENUM_TYPE) \
        return meSystem.RegisterEnum<ENUM_TYPE>(meEnum); \
    }(); \
}

#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "Reflection.h"
#include "ReflectionUtils.h"

#ifdef GetClassName
#undef GetClassName
#endif

#define ME_REFLECTION_CONCAT_INNER(a, b) a##b
#define ME_REFLECTION_CONCAT(a, b) ME_REFLECTION_CONCAT_INNER(a, b)

// Class reflection macros
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

#define ME_REFLECTION_CLASS_DECLARE(TYPE, REGISTER_SYMBOL) \
namespace minEngine::Reflection \
{ \
    template<> \
    std::string GetClassName<TYPE>(); \
    extern const bool REGISTER_SYMBOL; \
}

#define ME_REFLECTION_CLASS_DEFINE_BEGIN(TYPE, REGISTER_SYMBOL) \
template<> \
std::string minEngine::Reflection::GetClassName<TYPE>() \
{ \
    return #TYPE; \
} \
const bool minEngine::Reflection::REGISTER_SYMBOL = []() \
{ \
    auto& meSystem = minEngine::Reflection::ReflectionSystem::Get(); \
    if (meSystem.FindClass<TYPE>() != nullptr) \
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

#define ME_REFLECTION_FUNCTION_BEGIN(FUNC_VAR, FUNC_NAME, FLAGS, SPECIFIER_MASK, METADATA) \
    minEngine::Reflection::MEFunction* FUNC_VAR = meSystem.CreateFunction(FUNC_NAME); \
    FUNC_VAR->SetFlags(FLAGS); \
    FUNC_VAR->SetAnnotations(SPECIFIER_MASK, METADATA);

#define ME_REFLECTION_FUNCTION_PARAM(FUNC_VAR, NAME, ROLE, PASS_KIND, ...) \
    { \
        minEngine::Reflection::MEProperty* paramProperty = meSystem.CreateFunctionParamProperty<__VA_ARGS__>(NAME); \
        FUNC_VAR->AddParameter(paramProperty, minEngine::Reflection::MEParamRole::ROLE, \
                               minEngine::Reflection::MEParamPassKind::PASS_KIND); \
    }

#define ME_REFLECTION_FUNCTION_RETURN(FUNC_VAR, ...) \
    { \
        minEngine::Reflection::MEProperty* returnValue = meSystem.CreateFunctionParamProperty<__VA_ARGS__>("ReturnValue"); \
        FUNC_VAR->AddParameter(returnValue, minEngine::Reflection::MEParamRole::Return, minEngine::Reflection::MEParamPassKind::Value); \
    }

#define ME_REFLECTION_FUNCTION_BIND_NATIVE(FUNC_VAR, OWNER_TYPE, METHOD) \
    FUNC_VAR->SetNativeThunk(&minEngine::Reflection::InvokeNativeThunk<OWNER_TYPE, &OWNER_TYPE::METHOD>);

#define ME_REFLECTION_FUNCTION_BIND_NATIVE_STATIC(FUNC_VAR, OWNER_TYPE, METHOD) \
    FUNC_VAR->SetNativeThunk( \
        &minEngine::Reflection::StaticFunctionNativeThunk<OWNER_TYPE, decltype(+OWNER_TYPE::METHOD), \
                                                            +OWNER_TYPE::METHOD>::Invoke);

#define ME_REFLECTION_FUNCTION_END(FUNC_VAR) \
    meSystem.RegisterFunction(meClass, FUNC_VAR);

#define ME_REFLECTION_CLASS_DEFINE_END(TYPE) \
    return meSystem.RegisterClass<TYPE>(meClass); \
}(); \
const minEngine::Reflection::MEClass* TYPE::StaticClass() \
{ \
    static const minEngine::Reflection::MEClass* cachedClass = nullptr; \
    if (cachedClass == nullptr) \
    { \
        cachedClass = minEngine::Reflection::ReflectionSystem::Get().FindClass<TYPE>(); \
    } \
    return cachedClass; \
}

// Enum reflection macros
#define ME_REFLECTION_ENUM_DECLARE(ENUM_TYPE, REGISTER_SYMBOL) \
namespace minEngine::Reflection \
{ \
    template<> \
    std::string GetEnumName<ENUM_TYPE>(); \
    extern const bool REGISTER_SYMBOL; \
}

#define ME_REFLECTION_ENUM_DEFINE_BEGIN(ENUM_TYPE, REGISTER_SYMBOL) \
template<> \
std::string minEngine::Reflection::GetEnumName<ENUM_TYPE>() \
{ \
    return #ENUM_TYPE; \
} \
const bool minEngine::Reflection::REGISTER_SYMBOL = []() \
{ \
    auto& meSystem = minEngine::Reflection::ReflectionSystem::Get(); \
    if (meSystem.FindEnum<ENUM_TYPE>() != nullptr) \
    { \
        return true; \
    } \
    minEngine::Reflection::MEEnum* meEnum = meSystem.CreateEnum(#ENUM_TYPE);

#define ME_REFLECTION_ENUM_VALUE(VALUE_NAME, VALUE_EXPR) \
    meEnum->AddEntry(#VALUE_NAME, static_cast<int64_t>(VALUE_EXPR));

#define ME_REFLECTION_ENUM_DEFINE_END(ENUM_TYPE) \
    return meSystem.RegisterEnum<ENUM_TYPE>(meEnum); \
}();


/* Legacy macros: keep old behavior temporarily for compatibility. */
#define ME_REFLECTION_ENUM_BEGIN(ENUM_TYPE) \
template<> \
inline std::string minEngine::Reflection::GetEnumName<ENUM_TYPE>() \
{ \
    return #ENUM_TYPE; \
} \
namespace \
{ \
    [[maybe_unused]] const bool ME_REFLECTION_CONCAT(_me_reflection_enum_registered_line_, __COUNTER__) = []() \
    { \
        auto& meSystem = minEngine::Reflection::ReflectionSystem::Get(); \
        if (meSystem.FindEnum<ENUM_TYPE>() != nullptr) \
        { \
            return true; \
        } \
        minEngine::Reflection::MEEnum* meEnum = meSystem.CreateEnum(#ENUM_TYPE);

#define ME_REFLECTION_ENUM_END(ENUM_TYPE) \
    return meSystem.RegisterEnum<ENUM_TYPE>(meEnum); \
}(); \
}

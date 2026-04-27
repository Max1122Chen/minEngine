#pragma once
#include <string>
#include "Core/TypeTraits.h"
#include "Math/Math.h"

namespace minEngine::Reflection
{
    template<typename T>
    inline std::string GetClassName()
    {
        static_assert(minEngine::AlwaysFalse<T>::value, "GetClassName<T> is not specialized for this type T. Please provide a specialization that returns the class name for this type.");
        return "";
    }

    template<typename T>
    inline std::string GetEnumName()
    {
        static_assert(minEngine::AlwaysFalse<T>::value, "GetEnumName<T> is not specialized for this type T. Please provide a specialization that returns the enum name for this type.");
        return "";
    }

    template<typename T>
    inline std::string GetPrimitiveName()
    {
        static_assert(minEngine::AlwaysFalse<T>::value, "GetPrimitiveName<T> is not specialized for this type T. Please provide a specialization that returns the primitive type name for this type.");
        return "";
    }

    template<typename T>
    inline std::string GetTypeName()
    {
        if constexpr (std::is_enum_v<T>)
        {
            return GetEnumName<T>();
        }
        else if constexpr (kIsPrimitiveLike<T>)
        {
            return GetPrimitiveName<T>();
        }
        else
        {
            return GetClassName<T>();
        }
    }

    // Specializations for primitive types
    // Boolean type
    template<>
    inline std::string GetPrimitiveName<bool>() { return "bool"; }

    // Integral types
    template<>
    inline std::string GetPrimitiveName<char>() { return "char"; }
    template<>
    inline std::string GetPrimitiveName<int16_t>() { return "int16"; }    // aka short
    template<>
    inline std::string GetPrimitiveName<int32_t>() { return "int32"; }    // aka int
    template<>
    inline std::string GetPrimitiveName<long>() { return "long"; }      
    template<>
    inline std::string GetPrimitiveName<int64_t>() { return "int64"; }    // aka long long
    template<>
    inline std::string GetPrimitiveName<uint32_t>() { return "uint32"; }  // aka unsigned int
    template<>
    inline std::string GetPrimitiveName<unsigned long>() { return "unsigned long"; }
    template<>
    inline std::string GetPrimitiveName<uint64_t>() { return "uint64"; }  //  aka unsigned long long

    // Floating-point types
    template<>
    inline std::string GetPrimitiveName<float>() { return "float"; }
    template<>
    inline std::string GetPrimitiveName<double>() { return "double"; }
    template<>
    inline std::string GetPrimitiveName<long double>() { return "long double"; }

    // String type
    template<>
    inline std::string GetPrimitiveName<std::string>() { return "std::string"; }

    // Vector types
    template<>
    inline std::string GetPrimitiveName<minEngine::Vector2>() { return "Vector2"; }
    template<>
    inline std::string GetPrimitiveName<minEngine::Vector3>() { return "Vector3"; }
    template<>
    inline std::string GetPrimitiveName<minEngine::Vector4>() { return "Vector4"; }

}
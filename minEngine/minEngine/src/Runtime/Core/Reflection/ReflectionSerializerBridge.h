#pragma once

#include "Runtime/Core/Serialization/Serializer.h"
namespace minEngine::Reflection
{
    template<typename T>
    static Json WriteToJsonErased(const void* value)
    {
        return minEngine::Serializer::Write(*static_cast<const T*>(value));
    }

    template<typename T>
    static Json WritePointerToJsonErased(const void* value)
    {
        // return minEngine::Serializer::WritePointer(*static_cast<const T*>(value));
        return Json(); // TODO: implement this later
    }
}
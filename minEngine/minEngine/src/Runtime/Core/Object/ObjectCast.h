#pragma once

#include "Runtime/Core/Object/MEObject.h"

#include <memory>
#include <type_traits>

namespace minEngine
{
    /**
     * Reflection-backed downcast (UE-style Cast).
     * Checks MEObject::IsA then static_cast - no C++ RTTI.
     * Prefer this over std::dynamic_pointer_cast / dynamic_cast for MEObject graphs.
     * Not an AssetManager load API: Load stays untyped (ByPath/ByGUID/ByMeta).
     */
    template <typename T>
    T* Cast(MEObject* object)
    {
        static_assert(std::is_base_of_v<MEObject, T>, "Cast<T> requires T : MEObject");
        if (object == nullptr || !object->IsA(T::StaticClass()))
        {
            return nullptr;
        }
        return static_cast<T*>(object);
    }

    template <typename T>
    const T* Cast(const MEObject* object)
    {
        static_assert(std::is_base_of_v<MEObject, T>, "Cast<T> requires T : MEObject");
        if (object == nullptr || !object->IsA(T::StaticClass()))
        {
            return nullptr;
        }
        return static_cast<const T*>(object);
    }

    template <typename T, typename U>
    std::shared_ptr<T> Cast(const std::shared_ptr<U>& object)
    {
        static_assert(std::is_base_of_v<MEObject, T>, "Cast<T> requires T : MEObject");
        static_assert(std::is_base_of_v<MEObject, U>, "Cast source must be shared_ptr of MEObject");
        if (!object || !object->IsA(T::StaticClass()))
        {
            return nullptr;
        }
        return std::static_pointer_cast<T>(object);
    }
}

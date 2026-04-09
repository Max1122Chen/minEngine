#pragma once
#include <type_traits>
#include <vector>
#include <memory>

namespace minEngine
{
    // Container traits
    // Vector trait
    template<typename T>
    struct is_vector : std::false_type
    {};

    template<typename T, typename Allocator>
    struct is_vector<std::vector<T, Allocator>> : std::true_type
    {
        using ElementType = T;
    };

    // Smart Pointer traits
    template<typename T>
    struct is_shared_ptr : std::false_type
    {};

    template<typename T>
    struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
    {
        using pointee = T;
    };

    template<typename T>
    struct is_unique_ptr : std::false_type
    {};

    template<typename T>
    struct is_unique_ptr<std::unique_ptr<T>> : std::true_type
    {
        using pointee = T;
    };

    template<typename T>
    struct is_weak_ptr : std::false_type
    {};

    template<typename T>
    struct is_weak_ptr<std::weak_ptr<T>> : std::true_type
    {
        using pointee = T;
    };

    template<typename T>
    struct is_smart_ptr : std::integral_constant<bool, is_shared_ptr<T>::value || is_unique_ptr<T>::value || is_weak_ptr<T>::value>
    {};

}
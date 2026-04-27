#pragma once
#include <type_traits>
#include <vector>
#include <memory>
#include "Math/Math.h"

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
    {
        using pointee = T;
    };

    template<typename T>
    using RemoveCvRefT = std::remove_cv_t<std::remove_reference_t<T>>;

    template<typename T>
    struct PointerLike
    {
        static constexpr bool value = false;
    };

    template<typename T>
    struct PointerLike<T*>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T>
    struct PointerLike<std::shared_ptr<T>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T, typename TDeleter>
    struct PointerLike<std::unique_ptr<T, TDeleter>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    template<typename T>
    struct PointerLike<std::weak_ptr<T>>
    {
        static constexpr bool value = true;
        using Type = T;
    };

    // Some type traits helpers for reflection system
    template<typename T>
    inline constexpr bool kIsPointerLike = PointerLike<RemoveCvRefT<T>>::value;

    template<typename T>
    using PointeeT = typename PointerLike<RemoveCvRefT<T>>::Type;

    template<typename T>
    inline constexpr bool kIsPrimitiveLike = std::is_arithmetic_v<T>
                                           || std::is_same_v<T, std::string>
                                           || std::is_same_v<T, Vector2>
                                           || std::is_same_v<T, Vector3>
                                           || std::is_same_v<T, Vector4>
                                           || std::is_enum_v<T>;

    // Helper for static_assert false in templates
    template<typename T>
    struct AlwaysFalse
    {
        static constexpr bool value = false;
    };
}
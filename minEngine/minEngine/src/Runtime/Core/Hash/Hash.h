#pragma once

#include <cstdint>
#include <functional>

namespace minEngine
{
    template<typename T>
    size_t HashCombine(size_t seed, const T& value)
    {
        std::hash<T> hasher;
        return seed ^ (hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
}
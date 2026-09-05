#pragma once

#include <cstdint>

namespace minEngine
{
    enum class ParameterValueType : uint8_t
    {
        Bool = 0,
        Int32 = 1,
        Float = 2,
    };

    using ParameterKeyId = uint16_t;
    static constexpr ParameterKeyId kInvalidParameterKeyId = 0xFFFF;

    // Size / align helpers for closed MVP types (Bool=1B, Int32/Float=4B).
    class ParameterValueTypeUtil
    {
    public:
        static bool IsKnown(ParameterValueType type);
        static uint16_t SizeOf(ParameterValueType type);
        static uint16_t AlignOf(ParameterValueType type);
    };
}

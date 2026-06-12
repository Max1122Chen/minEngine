#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine
{
    /** Placeholder for future RDG buffer resources (RND-F01). */
    struct RDGBufferDesc
    {
        uint32_t ByteSize = 0;
    };

    class RDGBufferRef
    {
    public:
        RDGBufferRef() = default;

        bool IsValid() const { return m_Index != kInvalidIndex; }

        uint32_t GetIndex() const { return m_Index; }

        static RDGBufferRef FromIndex(uint32_t index) { return RDGBufferRef(index); }

    private:
        static constexpr uint32_t kInvalidIndex = UINT32_MAX;

        explicit RDGBufferRef(uint32_t index)
            : m_Index(index)
        {
        }

        uint32_t m_Index = kInvalidIndex;
    };
}

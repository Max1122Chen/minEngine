#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>

namespace minEngine
{
    struct GUID
    {
        uint64_t High;
        uint64_t Low;

        GUID() : High(0), Low(0) {}
        GUID(uint64_t high, uint64_t low) : High(high), Low(low) {}

        std::string ToString() const
        {
            char buffer[37];
            snprintf(buffer, sizeof(buffer), "%08x-%04x-%04x-%04x-%012llx",
                     (unsigned int)(High >> 32),
                     (unsigned int)((High >> 16) & 0xFFFF),
                     (unsigned int)(High & 0xFFFF),
                     (unsigned int)(Low >> 48),
                     (unsigned long long)(Low & 0xFFFFFFFFFFFFULL));
            return std::string(buffer);
        }

        bool operator==(const GUID& other) const
        {
            return High == other.High && Low == other.Low;
        }

        bool operator!=(const GUID& other) const
        {
            return !(*this == other);
        }
    };

    struct GUIDHasher
    {
        std::size_t operator()(const GUID& guid) const
        {
            return std::hash<uint64_t>()(guid.High) ^ (std::hash<uint64_t>()(guid.Low) << 1);
        }
    };

    // Using UUID v4 (random) for GUID generation
    GUID GenerateGUID();
}
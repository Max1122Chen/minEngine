#pragma once

#include "EngineAPI.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace minEngine
{
    // Product / build identity (not the disk schema algebra).
    struct EngineVersion
    {
        uint16_t Major = 0;
        uint16_t Minor = 0;
        uint16_t Patch = 0;

        std::string ToString() const
        {
            char buffer[32] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "%u.%u.%u",
                static_cast<unsigned>(Major),
                static_cast<unsigned>(Minor),
                static_cast<unsigned>(Patch));
            return std::string(buffer);
        }
    };

    // Single source of truth for this engine build.
    inline constexpr EngineVersion kEngineVersion{0, 0, 9};

    // Disk JSON schema algebra (CORE-F10/F18). Bump only for breaking on-disk shape.
    inline constexpr uint32_t kDiskSchemaVersion = 1u;

    inline EngineVersion GetEngineVersion()
    {
        return kEngineVersion;
    }
}

#pragma once

#include "EngineVersion.h"
#include "SerializationTypes.h"

#include <cstdint>
#include <string>

namespace minEngine::Serialization
{
    struct DiskVersionInfo
    {
        uint32_t schemaVersion = 0;
        std::string engineVersion;
    };

    // Fail when schemaVersion > supported. schemaVersion==0 => Warn + Success (legacy).
    MINENGINE_API SerializeResult ValidateDiskSchema(
        const DiskVersionInfo& info,
        uint32_t supportedSchemaVersion = minEngine::kDiskSchemaVersion);
}

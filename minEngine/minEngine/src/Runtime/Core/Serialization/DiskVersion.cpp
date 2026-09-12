#include "DiskVersion.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <string>

namespace minEngine::Serialization
{
    SerializeResult ValidateDiskSchema(const DiskVersionInfo& info, uint32_t supportedSchemaVersion)
    {
        if (info.schemaVersion == 0)
        {
            ME_LOG(
                LogSerialization,
                Warn,
                "Disk JSON missing or invalid $schemaVersion (treated as 0 / legacy); "
                "supported schema is {}. Consider re-saving the asset.",
                supportedSchemaVersion);
            return SerializeResult::Success();
        }

        if (info.schemaVersion > supportedSchemaVersion)
        {
            std::string message = "Disk JSON $schemaVersion "
                + std::to_string(info.schemaVersion)
                + " is newer than supported schema "
                + std::to_string(supportedSchemaVersion)
                + ".";
            if (!info.engineVersion.empty())
            {
                message += " File was saved by engine version '" + info.engineVersion + "'.";
            }
            return SerializeResult::Failure(std::move(message));
        }

        return SerializeResult::Success();
    }
}

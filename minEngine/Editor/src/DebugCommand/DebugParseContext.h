#pragma once

#include "Core.h"

#include <cstdint>
#include <string>

namespace minEngine::DebugCommand
{
    // Context for DebugCommand completion and parse adaptation only.
    // Not the same as EditorCommand execution payload (see ED-F12 §0.5).
    struct DebugParseContext
    {
        uint64_t ActiveSessionId = 0;
        std::string ActiveSessionTypeId;
        std::string ActiveAssetKey;
    };
}

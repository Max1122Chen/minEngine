#pragma once

#include "Core.h"

#include <string>
#include <string_view>

namespace minEngine::DebugCommand
{
    // Minimal JSON string helpers for command PayloadJson (ED-F12). Not a full JSON library.
    class DebugCommandPayloadJson
    {
    public:
        static std::string EscapeString(std::string_view text);
        static std::string Quote(std::string_view text);
    };
}

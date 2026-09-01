#pragma once

#include "EngineAPI.h"

#include <string>
#include <string_view>

namespace minEngine::Reflection
{
    class MEProperty;

    /** Strips m_/x_/b_ member prefixes, then inserts spaces at camelCase boundaries. */
    MINENGINE_API std::string FormatMemberDisplayName(std::string_view memberName);

    /**
     * Display label for UI and console inspect output.
     * Uses DisplayName metadata when set; otherwise FormatMemberDisplayName(GetName()).
     * Returned pointer is valid until the next call on this thread (thread_local buffer).
     */
    MINENGINE_API const char* GetPropertyDisplayName(const MEProperty& property);
}

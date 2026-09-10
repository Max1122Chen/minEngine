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
     * Type label for UI (Add Component / Inspector headers).
     * Shortens namespace, strips trailing "Component", then camelCase word breaks.
     * Example: "minEngine::PointLightComponent" -> "Point Light".
     */
    MINENGINE_API std::string FormatTypeDisplayName(std::string_view reflectedOrShortTypeName);

    /**
     * Default instance name: short class name without "Component" suffix, no word breaks.
     * Example: "PointLightComponent" -> "PointLight".
     */
    MINENGINE_API std::string FormatDefaultComponentInstanceName(std::string_view reflectedOrShortTypeName);

    /**
     * Display label for UI and console inspect output.
     * Uses DisplayName metadata when set; otherwise FormatMemberDisplayName(GetName()).
     * Returned pointer is valid until the next call on this thread (thread_local buffer).
     */
    MINENGINE_API const char* GetPropertyDisplayName(const MEProperty& property);
}

#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine::DebugCommand
{
    enum class DebugCommandScope : uint8_t
    {
        Editor,
        Runtime,
        Both,
    };

    enum class DebugCommandStatus : uint8_t
    {
        Ok,
        Error,
        Cancelled,
        Warning,
    };

    enum class DebugCommandOutputKind : uint8_t
    {
        InputEcho,
        SuccessStatus,
        Error,
        Warning,
        Hint,
        Plain,
        ListItemName,
        ListItemMeta,
        InspectHeader,
        InspectSection,
        InspectKey,
        InspectType,
        InspectValue,
        ValueLiteral,
        Path,
        Muted,
    };

    enum class DebugCommandFlags : uint32_t
    {
        None = 0,
        Undoable = 1u << 0,
        Hidden = 1u << 1,
        DebugOnly = 1u << 2,
    };

    inline DebugCommandFlags operator|(DebugCommandFlags lhs, DebugCommandFlags rhs)
    {
        return static_cast<DebugCommandFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline bool HasDebugCommandFlag(DebugCommandFlags flags, DebugCommandFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }
}

#pragma once

#include "Core.h"

#include <cstdint>

namespace minEngine::Command
{
    enum class CommandScope : uint8_t
    {
        Editor,
        Runtime,
        Both,
    };

    enum class CommandStatus : uint8_t
    {
        Ok,
        Error,
        Cancelled,
        Warning,
    };

    enum class CommandOutputKind : uint8_t
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

    enum class CommandFlags : uint32_t
    {
        None = 0,
        Undoable = 1u << 0,
        Hidden = 1u << 1,
        DebugOnly = 1u << 2,
    };

    inline CommandFlags operator|(CommandFlags lhs, CommandFlags rhs)
    {
        return static_cast<CommandFlags>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
    }

    inline bool HasCommandFlag(CommandFlags flags, CommandFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }
}

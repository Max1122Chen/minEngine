#pragma once

#include "DebugCommand/DebugCommandResult.h"
#include "DebugCommand/DebugCommandTypes.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::DebugCommand
{
    struct DebugCommandContext;

    enum class DebugCommandArgType : uint8_t
    {
        Bool,
        Int,
        Float,
        String,
        Enum,
        Guid,
        AssetPath,
        ObjectRef,
    };

    struct DebugCommandArgDescriptor
    {
        std::string_view Name;
        DebugCommandArgType Type = DebugCommandArgType::String;
        bool Required = true;
        std::string_view Description;
    };

    using DebugCommandExecuteFn =
        std::function<DebugCommandResult(const DebugCommandContext&, const std::vector<std::string>& args)>;

    struct DebugCommandDescriptor
    {
        std::string_view Id;
        std::string_view DisplayName;
        std::string_view Description;
        DebugCommandScope Scope = DebugCommandScope::Both;
        DebugCommandFlags Flags = DebugCommandFlags::None;
        std::vector<DebugCommandArgDescriptor> Args;
        DebugCommandExecuteFn Execute;
    };
}

#pragma once

#include "Runtime/Core/Command/CommandResult.h"
#include "Runtime/Core/Command/CommandTypes.h"

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Command
{
    struct CommandContext;

    enum class CommandArgType : uint8_t
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

    struct CommandArgDescriptor
    {
        std::string_view Name;
        CommandArgType Type = CommandArgType::String;
        bool Required = true;
        std::string_view Description;
    };

    using CommandExecuteFn =
        std::function<CommandResult(const CommandContext&, const std::vector<std::string>& args)>;

    struct CommandDescriptor
    {
        std::string_view Id;
        std::string_view DisplayName;
        std::string_view Description;
        CommandScope Scope = CommandScope::Both;
        CommandFlags Flags = CommandFlags::None;
        std::vector<CommandArgDescriptor> Args;
        CommandExecuteFn Execute;
    };
}

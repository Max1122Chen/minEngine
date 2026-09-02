#pragma once

#include <string_view>

namespace minEngine::Command
{
    struct CommandContext;
    struct CommandResult;
}

namespace minEngine
{
    void RegisterEditorConsoleCommands();

    Command::CommandResult ExecuteEditorConsoleSetValue(
        const Command::CommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral);
}

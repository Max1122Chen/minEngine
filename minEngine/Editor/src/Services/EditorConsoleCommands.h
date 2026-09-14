#pragma once

#include <string_view>

namespace minEngine::DebugCommand
{
    struct DebugCommandContext;
    struct DebugCommandResult;
}

namespace minEngine
{
    void RegisterEditorConsoleCommands();

    DebugCommand::DebugCommandResult ExecuteEditorConsoleSetValue(
        const DebugCommand::DebugCommandContext& context,
        std::string_view propertyPathText,
        std::string_view valueLiteral);
}

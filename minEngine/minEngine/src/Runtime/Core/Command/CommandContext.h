#pragma once

#include "Core.h"

#include <functional>
#include <string_view>

namespace minEngine
{
    class Scene;
}

namespace minEngine::Command
{
    struct CommandResult;

    struct CommandContext
    {
        Scene* ActiveScene = nullptr;

        // Editor-only: set by CommandConsolePresenter; nullptr in headless tests.
        void* EditorContextOpaque = nullptr;

        // When set, `set` delegates to editor undoable path instead of direct PropertyPath::SetValue.
        std::function<CommandResult(std::string_view propertyPathText, std::string_view valueLiteral)> EditorSetValue;
    };
}

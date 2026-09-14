#pragma once

#include "Core.h"

#include "DebugCommand/DebugParseContext.h"
#include "DebugCommand/DebugCommandResult.h"

#include <functional>
#include <string_view>

namespace minEngine
{
    class Scene;
}

namespace minEngine::DebugCommand
{
    struct DebugCommandContext
    {
        // Transitional execution convenience for scene-scoped Debug handlers.
        // Prefer explicit EditorCommand payload fields going forward (ED-F12).
        Scene* ActiveScene = nullptr;

        // Debug REPL only: Active Edit Session snapshot for parse/completion.
        DebugParseContext ParseContext;

        // Editor-only: set by CommandConsolePresenter; nullptr in headless tests.
        void* EditorContextOpaque = nullptr;

        // When set, `set` delegates to editor undoable path instead of direct PropertyPath::SetValue.
        std::function<DebugCommandResult(std::string_view propertyPathText, std::string_view valueLiteral)> EditorSetValue;
    };
}

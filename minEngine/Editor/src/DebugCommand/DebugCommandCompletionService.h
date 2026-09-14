#pragma once

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandCompletionTypes.h"

#include <string_view>
#include <vector>

namespace minEngine::DebugCommand
{
    class DebugCommandCompletionService
    {
    public:
        static std::vector<CompletionItem> Complete(
            std::string_view line,
            size_t cursorOffset,
            const DebugCommandContext& context);
    };
}

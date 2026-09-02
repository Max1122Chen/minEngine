#pragma once

#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CompletionTypes.h"

#include <string_view>
#include <vector>

namespace minEngine::Command
{
    class CompletionService
    {
    public:
        static std::vector<CompletionItem> Complete(
            std::string_view line,
            size_t cursorOffset,
            const CommandContext& context);
    };
}

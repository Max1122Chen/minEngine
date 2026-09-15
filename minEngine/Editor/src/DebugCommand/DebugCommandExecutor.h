#pragma once

#include "DebugCommand/DebugCommandContext.h"
#include "DebugCommand/DebugCommandResult.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine::DebugCommand
{
    class DebugCommandExecutor
    {
    public:
        DebugCommandResult Execute(std::string_view commandId,
                              const std::vector<std::string>& args,
                              const DebugCommandContext& context) const;

        DebugCommandResult ExecuteLine(std::string_view line, const DebugCommandContext& context) const;

    private:
        static std::vector<std::string> TokenizeLine(std::string_view line);
    };
}

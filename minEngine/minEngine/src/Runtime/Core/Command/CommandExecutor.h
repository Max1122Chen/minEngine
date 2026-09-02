#pragma once

#include "Runtime/Core/Command/CommandContext.h"
#include "Runtime/Core/Command/CommandResult.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine::Command
{
    class CommandExecutor
    {
    public:
        CommandResult Execute(std::string_view commandId,
                              const std::vector<std::string>& args,
                              const CommandContext& context) const;

        CommandResult ExecuteLine(std::string_view line, const CommandContext& context) const;

    private:
        static std::vector<std::string> TokenizeLine(std::string_view line);
    };
}

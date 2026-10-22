#pragma once

#include "CommandLineExitCode.h"
#include "CommandLineResult.h"
#include "EngineAPI.h"

#include <optional>

namespace minEngine
{
    class MINENGINE_API ApplicationCommandLine
    {
    public:
        static std::optional<CommandLineResult> TryParse(int argc, char** argv);
        static CommandLineExitCode GetLastExitCode();

    private:
        static CommandLineExitCode s_LastExitCode;
    };
}

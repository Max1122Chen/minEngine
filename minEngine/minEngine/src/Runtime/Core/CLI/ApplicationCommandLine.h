#pragma once

#include "CommandLineExitCode.h"
#include "CommandLineResult.h"

#include <optional>

namespace minEngine
{
    class ApplicationCommandLine
    {
    public:
        static std::optional<CommandLineResult> TryParse(int argc, char** argv);
        static CommandLineExitCode GetLastExitCode();

    private:
        static CommandLineExitCode s_LastExitCode;
    };
}

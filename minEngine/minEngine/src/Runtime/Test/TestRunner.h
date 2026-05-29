#pragma once

#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Core/CLI/CommandLineResult.h"

namespace minEngine
{
    class TestRunner
    {
    public:
        static CommandLineExitCode Run(const CommandLineResult& commandLine, int argc, char** argv);
    };
}

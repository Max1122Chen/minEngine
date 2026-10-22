#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Core/CLI/CommandLineResult.h"

namespace minEngine
{
    class MINENGINE_API TestRunner
    {
    public:
        static CommandLineExitCode Run(const CommandLineResult& commandLine, int argc, char** argv);
    };
}

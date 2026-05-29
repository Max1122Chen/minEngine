#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"

#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/CLI/CommandLineExitCode.h"
#include "Runtime/Test/TestRunner.h"
#include "TestSuites.h"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace
{
    int ExitCodeFrom(minEngine::CommandLineExitCode exitCode)
    {
        return static_cast<int>(exitCode);
    }

    std::vector<std::string> BuildSyntheticArgv(int argc, char** argv)
    {
        std::vector<std::string> arguments;
        arguments.emplace_back(argc > 0 && argv[0] != nullptr ? argv[0] : "minEngineTests");

        bool insertTestVerb = true;
        if (argc > 1 && argv[1] != nullptr)
        {
            const std::string_view first(argv[1]);
            if (first == "test" || first == "--help" || first == "-h" || first == "--version")
            {
                insertTestVerb = false;
            }
        }

        if (insertTestVerb)
        {
            arguments.emplace_back("test");
        }

        for (int argIndex = 1; argIndex < argc; ++argIndex)
        {
            if (argv[argIndex] != nullptr)
            {
                arguments.push_back(argv[argIndex]);
            }
        }

        return arguments;
    }

    std::vector<char*> BuildArgvPointerTable(const std::vector<std::string>& arguments)
    {
        std::vector<char*> pointers;
        pointers.reserve(arguments.size());
        for (const std::string& argument : arguments)
        {
            pointers.push_back(const_cast<char*>(argument.c_str()));
        }
        return pointers;
    }
}

int main(int argc, char** argv)
{
    const std::vector<std::string> syntheticArguments = BuildSyntheticArgv(argc, argv);
    std::vector<char*> syntheticArgv = BuildArgvPointerTable(syntheticArguments);
    const int syntheticArgc = static_cast<int>(syntheticArgv.size());

    if (minEngine::TestRunner::ContainsLegacyTestFlag(syntheticArgc, syntheticArgv.data()))
    {
        minEngine::CommandLineResult legacyPlaceholder;
        return ExitCodeFrom(
            minEngine::TestRunner::Run(legacyPlaceholder, syntheticArgc, syntheticArgv.data()));
    }

    const std::optional<minEngine::CommandLineResult> commandLine =
        minEngine::ApplicationCommandLine::TryParse(syntheticArgc, syntheticArgv.data());
    if (!commandLine.has_value())
    {
        return ExitCodeFrom(minEngine::ApplicationCommandLine::GetLastExitCode());
    }

    if (commandLine->RequestedHelp)
    {
        return 0;
    }

    if (commandLine->RequestedVersion)
    {
        return 0;
    }

    if (commandLine->Mode != minEngine::ApplicationMode::Test)
    {
        std::fprintf(
            stderr,
            "Usage: minEngineTests test <smoke|full|suite-id> [options]\n"
            "       minEngineTests <smoke|full|suite-id> [options]\n");
        return ExitCodeFrom(minEngine::CommandLineExitCode::UsageError);
    }

    minEngine::EnsureTestSuitesRegistered();
    return ExitCodeFrom(minEngine::TestRunner::Run(*commandLine, syntheticArgc, syntheticArgv.data()));
}

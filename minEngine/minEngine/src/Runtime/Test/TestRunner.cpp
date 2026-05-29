#include "TestRunner.h"

#include "TestContext.h"
#include "TestSuiteRegistry.h"
#include "TestSuites.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    namespace
    {
        struct LegacyTestFlagMapping
        {
            std::string_view FlagPrefix;
            std::string_view SuiteId;
            bool IsPrefixOnly = false;
        };

        constexpr LegacyTestFlagMapping kLegacyTestFlags[] = {
            {"--object-manager-test", "object-manager", false},
            {"--serialization-archive-test", "serialization-archive", false},
            {"--asset-manager-test", "asset-manager", false},
            {"--reflection-function-test", "reflection-function", true},
            {"--material-ir-test", "material-ir", false},
        };

        bool ArgEquals(char* argument, const std::string_view literal)
        {
            return argument != nullptr && std::string_view(argument) == literal;
        }

        bool ArgStartsWith(char* argument, const std::string_view prefix)
        {
            if (argument == nullptr)
            {
                return false;
            }

            const std::string_view value(argument);
            return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
        }

        std::optional<CommandLineResult> DetectLegacyTestRequest(int argc, char** argv, bool emitDeprecationWarning)
        {
            std::optional<CommandLineResult> result;
            for (int argIndex = 1; argIndex < argc; ++argIndex)
            {
                if (argv[argIndex] == nullptr)
                {
                    continue;
                }

                for (const LegacyTestFlagMapping& mapping : kLegacyTestFlags)
                {
                    const bool matches = mapping.IsPrefixOnly
                                             ? ArgStartsWith(argv[argIndex], mapping.FlagPrefix)
                                             : ArgEquals(argv[argIndex], mapping.FlagPrefix);
                    if (!matches)
                    {
                        continue;
                    }

                    if (!result.has_value())
                    {
                        result = CommandLineResult{};
                        result->Mode = ApplicationMode::Test;
                        result->TestKind = TestRunKind::SingleSuite;
                    }

                    if (result->SuiteId.empty())
                    {
                        result->SuiteId = std::string(mapping.SuiteId);
                    }

                    if (emitDeprecationWarning)
                    {
                        std::fprintf(
                            stderr,
                            "Warning: '%.*s' is deprecated; use 'test %.*s' instead.\n",
                            static_cast<int>(mapping.FlagPrefix.size()),
                            mapping.FlagPrefix.data(),
                            static_cast<int>(mapping.SuiteId.size()),
                            mapping.SuiteId.data());
                    }
                }
            }

            return result;
        }

        CommandLineResult ResolveEffectiveCommandLine(
            const CommandLineResult& commandLine,
            int argc,
            char** argv)
        {
            if (commandLine.Mode == ApplicationMode::Test)
            {
                return commandLine;
            }

            if (const std::optional<CommandLineResult> legacy =
                    DetectLegacyTestRequest(argc, argv, true))
            {
                return *legacy;
            }

            return commandLine;
        }

        std::vector<ITestSuite*> ResolveSuites(const CommandLineResult& commandLine)
        {
            TestSuiteRegistry& registry = TestSuiteRegistry::Get();

            if (commandLine.TestKind == TestRunKind::Smoke)
            {
                return registry.GetSmokeSuites();
            }

            if (commandLine.TestKind == TestRunKind::Full)
            {
                return registry.GetFullSuites();
            }

            if (commandLine.TestKind == TestRunKind::SingleSuite)
            {
                ITestSuite* suite = registry.FindById(commandLine.SuiteId);
                if (suite == nullptr)
                {
                    return {};
                }
                return {suite};
            }

            return {};
        }

        void ConfigureReflectionProfile(TestContext& context, const CommandLineResult& commandLine)
        {
            if (commandLine.SuiteId != "reflection-function")
            {
                return;
            }

            if (commandLine.ReflectionSuiteFilter.has_value())
            {
                context.SetReflectionProfile(ReflectionTestProfile::FilterFromCli);
                return;
            }

            if (commandLine.TestKind == TestRunKind::Smoke)
            {
                context.SetReflectionProfile(ReflectionTestProfile::SmokeSubset);
                return;
            }

            if (commandLine.TestKind == TestRunKind::Full)
            {
                context.SetReflectionProfile(ReflectionTestProfile::FullAllPhases);
                return;
            }

            context.SetReflectionProfile(ReflectionTestProfile::DefaultFromArgv);
        }
    }

    bool TestRunner::ContainsLegacyTestFlag(int argc, char** argv)
    {
        return DetectLegacyTestRequest(argc, argv, false).has_value();
    }

    CommandLineExitCode TestRunner::Run(const CommandLineResult& commandLine, int argc, char** argv)
    {
        EnsureTestSuitesRegistered();

        const CommandLineResult effective = ResolveEffectiveCommandLine(commandLine, argc, argv);
        if (effective.Mode != ApplicationMode::Test)
        {
            return CommandLineExitCode::UsageError;
        }

        std::vector<ITestSuite*> suites = ResolveSuites(effective);
        if (suites.empty())
        {
            std::fprintf(
                stderr,
                "TestRunner: unknown or empty test target '%s'.\n",
                effective.SuiteId.c_str());
            return CommandLineExitCode::UsageError;
        }

        TestContext context(effective, argc, argv);
        context.InitializeEnginePaths();

        ConfigureReflectionProfile(context, effective);

        bool allPassed = true;
        for (ITestSuite* suite : suites)
        {
            if (suite == nullptr)
            {
                continue;
            }

            const TestSuiteMetadata metadata = suite->GetMetadata();
            ME_CORE_INFO(
                "TestRunner: === suite '{}' ({}) ===",
                metadata.Id,
                metadata.DisplayName);

            if (!suite->Run(context))
            {
                ME_CORE_ERROR("TestRunner: suite '{}' FAILED.", metadata.Id);
                allPassed = false;
            }
            else
            {
                ME_CORE_INFO("TestRunner: suite '{}' PASSED.", metadata.Id);
            }
        }

        context.Shutdown();
        return allPassed ? CommandLineExitCode::Success : CommandLineExitCode::Failure;
    }
}

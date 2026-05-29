#include "TestRunner.h"

#include "TestContext.h"
#include "TestSuiteRegistry.h"

#include "Runtime/Core/Log/LogSystem.h"

#include <cstdio>
#include <string>
#include <vector>

namespace minEngine
{
    namespace
    {
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

    CommandLineExitCode TestRunner::Run(const CommandLineResult& commandLine, int argc, char** argv)
    {
        if (commandLine.Mode != ApplicationMode::Test)
        {
            return CommandLineExitCode::UsageError;
        }

        std::vector<ITestSuite*> suites = ResolveSuites(commandLine);
        if (suites.empty())
        {
            std::fprintf(
                stderr,
                "TestRunner: unknown or empty test target '%s'.\n",
                commandLine.SuiteId.c_str());
            return CommandLineExitCode::UsageError;
        }

        TestContext context(commandLine, argc, argv);
        context.InitializeEnginePaths();

        ConfigureReflectionProfile(context, commandLine);

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

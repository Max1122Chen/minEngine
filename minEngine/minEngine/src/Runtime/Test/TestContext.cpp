#include "TestContext.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"

namespace minEngine
{
    TestContext::TestContext(const CommandLineResult& commandLine, int argc, char** argv)
        : m_CommandLine(commandLine)
        , m_Argc(argc)
        , m_Argv(argv)
    {
    }

    bool TestContext::InitializeEnginePaths()
    {
        if (!m_LogInitialized)
        {
            LogSystem::Initialize();
            m_LogInitialized = true;
        }

        if (!m_PathsLoaded)
        {
            m_PathsLoaded = PathRegistry::Get().LoadEngineConfiguration(m_CommandLine, m_EngineConfig);
            if (!m_PathsLoaded)
            {
                ME_CORE_WARN(
                    "TestRunner: EngineConfig not loaded; path-dependent suites may warn or skip checks.");
            }
        }

        return true;
    }

    void TestContext::Shutdown()
    {
        if (m_LogInitialized)
        {
            LogSystem::Shutdown();
            m_LogInitialized = false;
        }
    }

    void TestContext::SetReflectionProfile(ReflectionTestProfile profile)
    {
        m_ReflectionProfile = profile;
    }

    bool TestContext::BuildReflectionArgv(
        std::vector<std::string>& outStorage,
        std::vector<char*>& outArgv) const
    {
        outStorage.clear();
        outArgv.clear();

        if (m_Argc > 0 && m_Argv != nullptr && m_Argv[0] != nullptr)
        {
            outStorage.emplace_back(m_Argv[0]);
        }
        else
        {
            outStorage.emplace_back("minEngine");
        }

        switch (m_ReflectionProfile)
        {
        case ReflectionTestProfile::FilterFromCli:
            if (m_CommandLine.ReflectionSuiteFilter.has_value())
            {
                outStorage.push_back(
                    "--reflection-function-test=" + *m_CommandLine.ReflectionSuiteFilter);
            }
            else
            {
                outStorage.emplace_back("--reflection-function-test");
            }
            break;
        case ReflectionTestProfile::SmokeSubset:
            outStorage.emplace_back("--reflection-function-test=meta,invoke");
            break;
        case ReflectionTestProfile::FullAllPhases:
            outStorage.emplace_back("--reflection-function-test");
            break;
        case ReflectionTestProfile::DefaultFromArgv:
            for (int argIndex = 1; argIndex < m_Argc; ++argIndex)
            {
                if (m_Argv[argIndex] == nullptr)
                {
                    continue;
                }

                const std::string_view argument(m_Argv[argIndex]);
                if (argument == "--reflection-function-test"
                    || argument.rfind("--reflection-function-test=", 0) == 0)
                {
                    outStorage.emplace_back(m_Argv[argIndex]);
                }
            }
            if (outStorage.size() == 1)
            {
                outStorage.emplace_back("--reflection-function-test");
            }
            break;
        }

        outArgv.reserve(outStorage.size());
        for (std::string& argument : outStorage)
        {
            outArgv.push_back(argument.data());
        }

        return !outArgv.empty();
    }
}

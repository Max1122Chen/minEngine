#pragma once

#include "Runtime/Core/CLI/CommandLineResult.h"
#include "Runtime/EngineConfig.h"

#include <string>
#include <vector>

namespace minEngine
{
    enum class ReflectionTestProfile
    {
        DefaultFromArgv,
        SmokeSubset,
        FullAllPhases,
        FilterFromCli,
    };

    class TestContext
    {
    public:
        TestContext(const CommandLineResult& commandLine, int argc, char** argv);

        const CommandLineResult& GetCommandLine() const { return m_CommandLine; }
        int GetArgc() const { return m_Argc; }
        char** GetArgv() const { return m_Argv; }

        bool InitializeEnginePaths();
        void Shutdown();

        void SetReflectionProfile(ReflectionTestProfile profile);
        ReflectionTestProfile GetReflectionProfile() const { return m_ReflectionProfile; }

        bool BuildReflectionArgv(std::vector<std::string>& outStorage, std::vector<char*>& outArgv) const;

    private:
        CommandLineResult m_CommandLine;
        int m_Argc = 0;
        char** m_Argv = nullptr;
        EngineConfig m_EngineConfig;
        bool m_LogInitialized = false;
        bool m_PathsLoaded = false;
        ReflectionTestProfile m_ReflectionProfile = ReflectionTestProfile::DefaultFromArgv;
    };
}

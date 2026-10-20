#pragma once
#include "Core.h"

#include "Runtime/Core/CLI/CommandLineResult.h"

namespace minEngine
{
    class Application
    {
    public:
        Application()  = default;
        virtual ~Application() = default;

        virtual void Initialize(int argc, char** argv) {}
        virtual void Initialize(int argc, char** argv, const CommandLineResult& commandLine)
        {
            (void)commandLine;
            Initialize(argc, argv);
        }
        virtual void Shutdown() {}
        virtual void Run() {}

        // BUILD-F04 S05: the process exit code a host main must return. An application that can
        // refuse to continue (the editor's game-module gate) has to say so to the shell and to CI,
        // and it only crosses the module boundary as Application*, so a virtual is the one seam
        // that needs no extra exported symbol. Declared LAST on purpose: appending keeps every
        // existing vtable slot index intact for consumers built against an older contract.
        // Default 0 = unchanged behaviour for every non-editor host.
        virtual int GetExitCode() const { return 0; }
    };

    // to be defined in CLIENT
    Application* CreateApplication();
}
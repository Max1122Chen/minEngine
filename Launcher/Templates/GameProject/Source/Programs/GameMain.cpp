// {{PROJECT_NAME}} standalone game host (generated template).
#include "Application.h"
#include "main.h"
#include "Runtime/Engine.h"

namespace minEngine
{
    class {{PROJECT_NAME}}GameApplication : public Application
    {
    public:
        void Initialize(int argc, char** argv, const CommandLineResult& commandLine) override
        {
            (void)argc;
            (void)argv;
            m_Engine = new Engine();
            m_Engine->Initialize(commandLine);
        }

        void Shutdown() override
        {
            if (m_Engine != nullptr)
            {
                m_Engine->Shutdown();
                delete m_Engine;
                m_Engine = nullptr;
            }
        }

        void Run() override
        {
            if (m_Engine != nullptr)
            {
                m_Engine->Run();
            }
        }

    private:
        Engine* m_Engine = nullptr;
    };

    Application* CreateApplication()
    {
        return new {{PROJECT_NAME}}GameApplication();
    }
}

#include "Core.h"
#include "Application.h"
#include "Runtime/Engine.h"

extern minEngine::Application* minEngine::CreateApplication();

int main(int argc, char** argv)
{
    minEngine::Application* app = minEngine::CreateApplication();
    app->Initialize(argc, argv);
    app->Run();
    app->Shutdown();
    delete app;

    return 0;
}
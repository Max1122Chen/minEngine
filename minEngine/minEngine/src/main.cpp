#include "Core.h"

#include <iostream>

#include "Runtime/Engine.h"



int main(int argc, char** argv)
{

    minEngine::Engine* engine = new minEngine::Engine();
    engine->Initialize();

    engine->Run();

    engine->Shutdown();
    delete engine;


    return 0;
}
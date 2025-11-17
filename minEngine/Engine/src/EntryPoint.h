#pragma once
#include <stdio.h>
extern minEngine::Application* minEngine::CreateApplication();


int main(int argc, char** argv)
{
   
   printf("Hello MinEngine!\n");
   getchar();
 
   auto app = minEngine::CreateApplication();
    app->Run();
    delete app;
    return 0;
}
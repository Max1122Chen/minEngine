#include "Core.h"

#include <iostream>

#include "Runtime/Engine.h"

#include "Runtime/Function/Render/GLFWWindowSystem.h"
#include "Runtime/Function/Render/OpenGLBuffer.h"
#include "Runtime/Function/Render/OpenGLShader.h"




int main(int argc, char** argv)
{

    minEngine::Engine* engine = new minEngine::Engine();
    engine->Initialize();

    engine->Run();

    engine->Shutdown();
    delete engine;

    return 0;





    // Initialize Window
    minEngine::GLFWWindowSystem window(1024, 768);
    window.Initialize();


    minEngine::OpenGLShader shader("D:/Dev/GitRepo/minEngine/minEngine/Shaders/first.vert", "D:/Dev/GitRepo/minEngine/minEngine/Shaders/first.frag");
    shader.Use();


    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // left  
         0.5f, -0.5f, 0.0f, // right 
         0.0f,  0.5f, 0.0f  // top   
    }; 

    

    unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    minEngine::OpenGLVertexBuffer vertexBuffer(vertices, sizeof(vertices));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    

    glBindVertexArray(VAO);

    minEngine::BufferLayout layout = {
        minEngine::VertexElement("aPos", minEngine::VertexElementType::Float3)
    };



    // Main loop
    while (!window.ShouldClose())
    {
        // Process input
        window.ProcessInput();

        // Clear the window
        window.Clear();

        // Rendering commands here
        shader.Use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        
        // Swap buffers and poll events
        window.SwapBuffers();
        window.PollEvents();
    }

    return 0;
}
#include "minEngine.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

namespace minEngine
{
    class Editor : public Application
    {
    public:
        Editor() = default;
        virtual ~Editor() = default;

        virtual void Initialize() override
        {
            engine = new Engine();
            engine->Initialize();

            // Set up IMGUI
            ImGui::CreateContext();
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
            ImGui_ImplGlfw_InitForOpenGL(static_cast<GLFWwindow*>(RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->GetWindowHandle()), true);
            ImGui_ImplOpenGL3_Init();
        }

        virtual void Shutdown() override
        {
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();

            engine->Shutdown();
            delete engine;
            engine = nullptr;
        }

        virtual void Run() override
        {
            WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
            while (!windowSystem->ShouldClose())
            {
                // Start the Dear ImGui frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                float deltaTime = engine->CalculateDeltaTime();
                engine->TickOneFrame(deltaTime);

                // Render ImGui
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
        }

    private: 
        Engine* engine = nullptr;
    };

    Application* CreateApplication()
    {
        return new Editor();
    }
}
#include "Editor.h"

#include "main.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

namespace minEngine
{
    void Editor::Initialize()
    {
        m_Engine = new Engine();
        m_Engine->Initialize();

        RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->SetPresentPassEnabled(false);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.FontGlobalScale = 1.50f;
        ImGui::StyleColorsLight();

        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->GetWindowHandle());
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init();

        RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->SetCursorVisible(true);
        m_EditorGUIManager.Initialize(*this);

        const Reflection::TypeInfo* transformTypeInfo = Reflection::ReflectionSystem::Get().GetTypeInfo<Transform>();
        if (transformTypeInfo == nullptr)
        {
            ME_CORE_WARN("[Reflection] Transform type info not found at editor startup.");
        }
        else
        {
            ME_CORE_INFO("[Reflection] Type: {} (size: {}, properties: {})",
                         transformTypeInfo->name,
                         transformTypeInfo->size,
                         transformTypeInfo->fields.size());
            for (const auto& field : transformTypeInfo->fields)
            {
                ME_CORE_INFO("[Reflection] Transform property: {} | type: {} | offset: {}",
                             field.name,
                             field.typeName,
                             field.offset);
            }
        }
    }

    void Editor::Shutdown()
    {
        m_EditorGUIManager.Shutdown();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_Engine->Shutdown();
        delete m_Engine;
        m_Engine = nullptr;
    }

    void Editor::Run()
    {
        WindowSystem* windowSystem = RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem.get();
        while (!windowSystem->ShouldClose() && !m_ExitRequested)
        {
            const float deltaTime = m_Engine->CalculateDeltaTime();
            m_Engine->TickOneFrame(deltaTime);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            m_EditorGUIManager.Tick(deltaTime);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            windowSystem->SwapBuffers();
        }
    }

    Application* CreateApplication()
    {
        return new Editor();
    }
}



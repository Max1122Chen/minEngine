#include "Editor.h"

#include "Material/MaterialEditor.h"
#include "Material/MaterialEditorSession.h"

#include "main.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Resource/AssetManager.h"

#include "Scene/SceneEditor.h"
#include "Shell/EditorSettingsDefaults.h"

#include <filesystem>

namespace minEngine
{
    namespace
    {
        void ApplyEditorTheme()
        {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4* colors = style.Colors;

            style.WindowRounding = 7.0f;
            style.ChildRounding = 6.0f;
            style.FrameRounding = 5.0f;
            style.PopupRounding = 6.0f;
            style.GrabRounding = 4.0f;
            style.TabRounding = 6.0f;
            style.ScrollbarRounding = 8.0f;

            style.WindowPadding = ImVec2(10.0f, 10.0f);
            style.FramePadding = ImVec2(9.0f, 6.0f);
            style.ItemSpacing = ImVec2(8.0f, 7.0f);
            style.ItemInnerSpacing = ImVec2(6.0f, 5.0f);

            colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.15f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);
            colors[ImGuiCol_Border] = ImVec4(0.23f, 0.29f, 0.35f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(0.24f, 0.31f, 0.39f, 1.00f);

            colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.19f, 0.25f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.24f, 0.31f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.23f, 0.30f, 0.39f, 1.00f);

            colors[ImGuiCol_Header] = ImVec4(0.18f, 0.24f, 0.33f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.32f, 0.43f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.19f, 0.28f, 0.39f, 1.00f);

            colors[ImGuiCol_Button] = ImVec4(0.23f, 0.35f, 0.53f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.27f, 0.43f, 0.64f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.19f, 0.29f, 0.46f, 1.00f);

            colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.22f, 1.00f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.38f, 0.59f, 1.00f);
            colors[ImGuiCol_TabActive] = ImVec4(0.27f, 0.43f, 0.65f, 1.00f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.12f, 0.18f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.31f, 0.47f, 1.00f);

            colors[ImGuiCol_Text] = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.58f, 0.64f, 0.71f, 1.00f);
        }
    }

    Editor::Editor() = default;

    Editor::~Editor() = default;

    void Editor::RegisterModules()
    {
        m_SubModules.clear();
        m_SubModules.push_back(&m_SceneEditor);
        m_SubModules.push_back(m_MaterialEditor.get());

        m_SceneEditor.InitializeComponentTypeNames();

        m_MainMenuModule.Register(*this);
        m_InspectorModule.Register(*this);
        m_ConsoleModule.Register(*this);
        m_AssetWorkflow.Register(*this);

        m_SceneEditor.Register(*this);
        m_MaterialEditor->Register(*this);

        ActivateSubModule(SceneEditor::kModuleId);
    }

    EditorSubModule* Editor::FindSubModule(std::string_view moduleId)
    {
        for (EditorSubModule* subModule : m_SubModules)
        {
            if (subModule && subModule->GetModuleId() == moduleId)
            {
                return subModule;
            }
        }
        return nullptr;
    }

    const EditorSubModule* Editor::FindSubModule(std::string_view moduleId) const
    {
        for (EditorSubModule* subModule : m_SubModules)
        {
            if (subModule && subModule->GetModuleId() == moduleId)
            {
                return subModule;
            }
        }
        return nullptr;
    }

    bool Editor::ActivateSubModule(std::string_view moduleId)
    {
        EditorSubModule* target = FindSubModule(moduleId);
        if (!target || !target->CanActivate())
        {
            return false;
        }

        if (m_ActiveSubModule == target)
        {
            return true;
        }

        if (m_ActiveSubModule)
        {
            m_ActiveSubModule->UnregisterCommands(*this);
            m_ActiveSubModule->OnDeactivate(*this);
        }

        m_InputHub.ClearActiveSubModuleCommands();
        m_ActiveSubModule = target;
        m_ActiveSubModule->OnActivate(*this);
        m_ActiveSubModule->RegisterCommands(*this);
        m_EditorGUIManager.OnActiveSubModuleChanged();
        return true;
    }

    bool Editor::OpenProject(const std::string& projectPath)
    {
        ProjectManager& projectManager = ProjectManager::Get();
        ProjectOpenResult result = projectManager.OpenProject(projectPath);
        if (result.IsSuccess())
        {
            ME_CORE_INFO(result.Message);

            ApplyCommandStackSettingsFromProject();
            ResetCommandStackForNewDocument();

            const ProjectContext& projectCtx = projectManager.GetCurrentProjectCtx();
            if (!projectCtx.Settings.EditorDefaultSceneName.empty())
            {
                if (!m_SceneEditor.LoadScene(*this, projectCtx.Settings.EditorDefaultSceneName))
                {
                    ME_CORE_WARN(
                        "Failed to load editor default scene '{}'.",
                        projectCtx.Settings.EditorDefaultSceneName);
                }
                else
                {
                    ME_CORE_INFO(
                        "Editor default scene '{}' loaded successfully.",
                        projectCtx.Settings.EditorDefaultSceneName);
                }
            }

            m_MaterialEditor->RefreshMaterialList();
            m_SceneEditor.OnProjectOpened();

            return true;
        }

        ME_CORE_ERROR(result.Message);
        return false;
    }

    void Editor::CloseProject()
    {
        ResetCommandStackForNewDocument();
    }

    void Editor::ApplyCommandStackSettingsFromProject()
    {
        const ProjectContext& projectCtx = ProjectManager::Get().GetCurrentProjectCtx();
        m_CommandStack.SetMaxDepth(ResolveMaxUndoStackDepth(projectCtx.Settings.Editor.MaxUndoStackDepth));
    }

    void Editor::ResetCommandStackForNewDocument()
    {
        m_CommandStack.Clear();
    }

    void Editor::Initialize(int argc, char** argv)
    {
        m_Engine = new Engine();
        m_Engine->Initialize(argc, argv);

        RenderSystem::Get().SetPresentPassEnabled(false);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.FontGlobalScale = 1.50f;
        ImGui::StyleColorsDark();
        ApplyEditorTheme();

        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(WindowSystem::Get().GetWindowHandle());
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init();

        WindowSystem::Get().SetCursorVisible(true);

        m_ViewportRegistry.SetContext(this);
        m_MaterialEditor = std::make_unique<MaterialEditor>();

        if (m_Engine->IsEnginePathConfigLoaded())
        {
            AssetManager::Get().ScanAssets(PathRegistry::Get().GetEngineDefaultAssetsRoot());
        }

        m_EditorGUIManager.Initialize(*this);
        RegisterModules();

        std::string projectPath;
        if (argc > 1)
        {
            projectPath = argv[1];
        }
        else
        {
            projectPath = "D:/Dev/GitRepo/minEngine/minEngine/MyMEProject";
        }

        OpenProject(projectPath);
        PostInitialize();
    }

    void Editor::PostInitialize()
    {
    }

    void Editor::UpdateWindowTitle()
    {
        std::string windowTitle = "minEngine Editor";
        if (m_ActiveSubModule && m_ActiveSubModule->GetModuleId() == MaterialEditor::kModuleId)
        {
            if (MaterialEditor* materialEditor = dynamic_cast<MaterialEditor*>(m_ActiveSubModule))
            {
                const MaterialEditorSession& session = materialEditor->GetSession();
                std::string materialLabel = "Material Editor";
                if (session.HasOpenMaterial())
                {
                    const std::filesystem::path materialPath(session.AssetPath);
                    materialLabel = materialPath.filename().string();
                    if (materialLabel.empty())
                    {
                        materialLabel = session.AssetPath;
                    }
                }
                const char* dirtySuffix = session.Dirty ? " *" : "";
                windowTitle = "minEngine Editor - " + materialLabel + dirtySuffix;
            }
        }
        else if (SceneEditor* sceneEditor = dynamic_cast<SceneEditor*>(m_ActiveSubModule))
        {
            std::string sceneDisplayName = "Untitled";
            if (const Scene* activeScene = sceneEditor->GetActiveScene())
            {
                const std::filesystem::path scenePath(activeScene->GetSceneName());
                if (!scenePath.empty())
                {
                    sceneDisplayName = scenePath.filename().string();
                    if (sceneDisplayName.empty())
                    {
                        sceneDisplayName = activeScene->GetSceneName();
                    }
                }
            }

            const char* dirtySuffix = sceneEditor->IsSceneDirty() ? " *" : "";
            windowTitle = "minEngine Editor - " + sceneDisplayName + dirtySuffix;
        }

        WindowSystem::Get().SetTitle(windowTitle.c_str());
    }

    void Editor::Shutdown()
    {
        m_EditorGUIManager.Shutdown();

        if (m_MaterialEditor)
        {
            m_MaterialEditor->Shutdown();
        }
        m_MaterialEditor.reset();

        m_InputHub.Shutdown();
        m_SceneEditor.Shutdown();
        m_MainMenuModule.Shutdown();
        m_InspectorModule.Shutdown();
        m_ConsoleModule.Shutdown();
        m_AssetWorkflow.Shutdown();
        m_ViewportRegistry.Clear();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        m_Engine->Shutdown();
        delete m_Engine;
        m_Engine = nullptr;
    }

    void Editor::Run()
    {
        WindowSystem& windowSystem = WindowSystem::Get();
        while (!windowSystem.ShouldClose() && !m_ExitRequested)
        {
            const float deltaTime = m_Engine->CalculateDeltaTime();
            m_Engine->PollEvents();
            m_Engine->TickLogicalFrame(deltaTime);
            m_SceneEditor.SyncSelectionWithScene();

            if (m_ActiveSubModule)
            {
                m_ActiveSubModule->Tick(deltaTime);
            }

            UpdateWindowTitle();

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            m_EditorGUIManager.Tick(deltaTime);
            m_InputHub.ProcessInput(*this);

            m_Engine->TickRendererFrame(deltaTime);

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            windowSystem.SwapBuffers();
        }
    }

    Application* CreateApplication()
    {
        return new Editor();
    }
}

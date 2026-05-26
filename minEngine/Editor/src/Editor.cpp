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
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Resource/AssetManager.h"

#include "Scene/SceneEditor.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/EditorSettingsDefaults.h"
#include "UI/Appearance/EditorAppearance.h"

#include <filesystem>

namespace minEngine
{
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
        m_ContentBrowser.Register(*this);
        m_ProjectAssetWatcher.Register(*this);

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
        m_ProjectAssetWatcher.StopWatching();

        ProjectManager& projectManager = ProjectManager::Get();
        ProjectOpenResult result = projectManager.OpenProject(projectPath);
        if (result.IsSuccess())
        {
            ME_CORE_INFO(result.Message);

            ApplyCommandStackSettingsFromProject();
            ApplyAppearanceSettingsFromProject();
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

            const std::filesystem::path projectContentRoot = PathRegistry::Get().GetProjectContentRoot();
            m_ProjectAssetWatcher.StartWatching(projectContentRoot);
            m_ContentBrowser.GetModel().ResetForProject(projectContentRoot);

            return true;
        }

        ME_CORE_ERROR(result.Message);
        return false;
    }

    void Editor::CloseProject()
    {
        m_ProjectAssetWatcher.StopWatching();
        m_ContentBrowser.GetModel().Clear();
        m_AssetWorkflow.SetSelectedAsset(nullptr);
        m_AssetWorkflow.SetContentBrowserInspectorActive(false);
        ProjectManager::Get().CloseCurrentProject();
        ResetCommandStackForNewDocument();
    }

    void Editor::ApplyCommandStackSettingsFromProject()
    {
        const ProjectContext& projectCtx = ProjectManager::Get().GetCurrentProjectCtx();
        m_CommandStack.SetMaxDepth(ResolveMaxUndoStackDepth(projectCtx.Settings.Editor.MaxUndoStackDepth));
    }

    void Editor::ApplyAppearanceSettingsFromProject()
    {
        const ProjectContext& projectCtx = ProjectManager::Get().GetCurrentProjectCtx();
        m_Appearance.LoadFromAppearanceSettings(projectCtx.Settings.Appearance);
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
        io.FontGlobalScale = 1.0f;
        m_Appearance.ApplyDefaultTheme();

        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(WindowSystem::Get().GetWindowHandle());
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init();

        WindowSystem::Get().SetCursorVisible(true);

        m_ViewportRegistry.SetContext(this);
        m_MaterialEditor = std::make_unique<MaterialEditor>();

        m_EditorGUIManager.Initialize(*this);
        RegisterModules();

        std::string projectPath;
        if (argc > 1)
        {
            projectPath = argv[1];
        }
        else
        {
            // feat/editor-asset-workflow worktree: scan/register Assets under this project (not main minEngine repo).
            projectPath = "D:/Dev/GitRepo/minEngine/minEngine/MyMEProject";
        }

        OpenProject(projectPath);
        m_Appearance.RebuildUiFontAtlas();
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

    IFileDialogService& Editor::GetFileDialogService()
    {
        return FileDialogService::Get().GetImplementation();
    }

    const IFileDialogService& Editor::GetFileDialogService() const
    {
        return FileDialogService::Get().GetImplementation();
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
        m_ContentBrowser.Shutdown();
        m_ProjectAssetWatcher.Shutdown();
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
            m_ProjectAssetWatcher.Tick(deltaTime);
            m_InputHub.ProcessInput(*this);

            // Inspector / UI can mark components dirty after LogicalTick already ran
            // SendAllEndOfFrameUpdates. Flush again so mesh/material ref changes update
            // scene proxies before render (avoids dangling VB/IB pointers when old assets drop).
            SceneManager::Get().SendAllEndOfFrameUpdates();

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

#include "Editor.h"

#include "Platform/EditorCrashDiagnostics.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Material/MaterialEditorSession.h"

#include "main.h"

#include "imgui.h"

#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHI.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Function/Debug/DebugDrawService.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"

#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/EditorSettingsDefaults.h"
#include "UI/Appearance/EditorAppearance.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <optional>

namespace minEngine
{
    std::optional<std::filesystem::path> Editor::ResolveProjectDescriptorPath(
        const CommandLineResult& commandLine)
    {
        if (!commandLine.ProjectDescriptorPath.has_value())
        {
            ME_CORE_ERROR("Editor requires a project descriptor path.");
            ME_CORE_ERROR("Usage: Editor.exe --project <path-to-project.meproject> (see --help).");
            return std::nullopt;
        }

        const std::filesystem::path descriptorPath = *commandLine.ProjectDescriptorPath;
        if (descriptorPath.extension() != ".meproject")
        {
            ME_CORE_ERROR(
                "Project path '{}' is not a .meproject descriptor.",
                descriptorPath.string());
            return std::nullopt;
        }

        if (!std::filesystem::exists(descriptorPath))
        {
            ME_CORE_ERROR("Project descriptor '{}' does not exist.", descriptorPath.string());
            return std::nullopt;
        }

        return descriptorPath;
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
        m_ToolbarModule.Register(*this);
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
            m_ContextMenu.RegisterBuiltInActions();

            const ProjectContext& projectCtx = projectManager.GetCurrentProjectCtx();
            if (!projectCtx.Settings.EditorDefaultSceneName.empty())
            {
                const std::string& defaultSceneName = projectCtx.Settings.EditorDefaultSceneName;
                if (!m_SceneEditor.LoadScene(*this, defaultSceneName))
                {
                    ME_CORE_WARN(
                        "Failed to load editor default scene '{}'.",
                        defaultSceneName);

                    if (defaultSceneName != "default" &&
                        m_SceneEditor.LoadScene(*this, "default"))
                    {
                        ME_CORE_INFO("Editor: loaded fallback scene 'default'.");
                    }
                }
                else
                {
                    ME_CORE_INFO(
                        "Editor default scene '{}' loaded successfully.",
                        defaultSceneName);
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
        m_ContextMenu.Shutdown();
        m_ProjectAssetWatcher.StopWatching();
        m_ContentBrowser.GetModel().Clear();
        m_AssetWorkflow.SetSelectedAsset(nullptr);
        m_AssetWorkflow.SetContentBrowserInspectorActive(false);
        m_InspectorModule.ClearInspectionTarget();
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

    bool Editor::InitializeImGuiBackend()
    {
        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(WindowSystem::Get().GetWindowHandle());
        if (windowHandle == nullptr)
        {
            ME_CORE_ERROR("Editor: GLFW window handle is null.");
            return false;
        }

        const EditorImGuiBackend::RendererApi api = RHIBackendSelection::IsVulkan()
            ? EditorImGuiBackend::RendererApi::Vulkan
            : EditorImGuiBackend::RendererApi::OpenGL;

        if (!m_ImGuiBackend.Initialize(api, windowHandle))
        {
            ME_CORE_ERROR("Editor: ImGui backend initialization failed.");
            return false;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (api == EditorImGuiBackend::RendererApi::Vulkan)
        {
            RHI* rhi = RenderSystem::Get().GetRHI();
            auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
            if (vulkanRhi == nullptr || !m_ImGuiBackend.InitializeVulkanRenderer(*vulkanRhi))
            {
                ME_CORE_ERROR("Editor: ImGui Vulkan renderer initialization failed.");
                return false;
            }
        }
#endif

        return true;
    }

    void Editor::Initialize(int argc, char** argv)
    {
        const std::optional<CommandLineResult> commandLine =
            ApplicationCommandLine::TryParse(argc, argv);
        if (!commandLine.has_value())
        {
            m_ExitRequested = true;
            return;
        }

        Initialize(argc, argv, *commandLine);
    }

    void Editor::Initialize(int argc, char** argv, const CommandLineResult& commandLine)
    {
        (void)argc;
        (void)argv;

        m_Engine = new Engine();
        m_Engine->Initialize(commandLine);

        RenderSystem::Get().SetPresentPassEnabled(false);
        if (RHIBackendSelection::IsVulkan())
        {
            RenderSystem::Get().GetRHI()->RHISetBackbufferClearColor(Vector3(0.1f, 0.1f, 0.1f));
            ME_CORE_INFO("Editor: Vulkan full Editor path (ED-F01); scene renders to viewport RT.");
        }

        ImGui::CreateContext();
        InstallEditorCrashDiagnostics();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.FontGlobalScale = 1.0f;
        m_Appearance.ApplyDefaultTheme();

        if (!InitializeImGuiBackend())
        {
            m_ExitRequested = true;
            return;
        }
        m_Appearance.SetImGuiBackend(&m_ImGuiBackend);

        WindowSystem::Get().SetCursorVisible(true);

        m_ViewportRegistry.SetContext(this);
        m_MaterialEditor = std::make_unique<MaterialEditor>();

        m_EditorGUIManager.Initialize(*this);
        m_InputHub.Initialize(*this);
        RegisterModules();

        const std::optional<std::filesystem::path> projectDescriptorPath =
            ResolveProjectDescriptorPath(commandLine);
        if (!projectDescriptorPath.has_value())
        {
            m_ExitRequested = true;
            return;
        }

        if (!OpenProject(projectDescriptorPath->string()))
        {
            m_ExitRequested = true;
            return;
        }
        m_PendingInitialFontAtlasRebuild = true;
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
        m_ContextMenu.Shutdown();

        m_ImGuiBackend.Shutdown();
        ImGui::DestroyContext();

        if (m_Engine)
        {
            m_Engine->Shutdown();
            delete m_Engine;
            m_Engine = nullptr;
        }
    }

    void Editor::Run()
    {
        WindowSystem& windowSystem = WindowSystem::Get();
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        bool fontAtlasGpuMarked = false;

        while (!windowSystem.ShouldClose() && !m_ExitRequested)
        {
            DebugDrawService::Get().ClearFrameQueues();

            const float deltaTime = m_Engine->CalculateDeltaTime();
            m_Engine->PollEvents();
            m_Engine->TickLogicalFrame(deltaTime);
            m_PlayInEditorSession.TickPIE(deltaTime);
            m_SceneEditor.SyncSelectionWithScene();

            if (m_ActiveSubModule)
            {
                m_ActiveSubModule->Tick(deltaTime);
            }

            UpdateWindowTitle();

            if (m_PendingInitialFontAtlasRebuild)
            {
                m_Appearance.RebuildUiFontAtlas();
                m_PendingInitialFontAtlasRebuild = false;
            }

            m_ImGuiBackend.NewFrame();
            ImGui::NewFrame();

            m_EditorGUIManager.Tick(deltaTime);
            m_ProjectAssetWatcher.Tick(deltaTime);
            m_InputHub.ProcessInput(*this);

            SceneManager::Get().SendAllEndOfFrameUpdates();

            m_Engine->TickRendererFrame(deltaTime);

            m_ImGuiBackend.RenderDrawData(rhi);
            if (!fontAtlasGpuMarked)
            {
                m_Appearance.MarkFontAtlasGpuInitialized();
                fontAtlasGpuMarked = true;
            }
            if (RenderSystem::HasInstance())
            {
                RenderSystem::Get().PresentFrame();
            }
            else
            {
                windowSystem.SwapBuffers();
            }
        }
    }

    Application* CreateApplication()
    {
        return new Editor();
    }
}

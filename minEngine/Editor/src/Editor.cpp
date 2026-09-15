#include "Editor.h"

#include "Platform/EditorCrashDiagnostics.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Material/MaterialEditorSession.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditorSession.h"

#include "main.h"

#include "imgui.h"

#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/ProductBranding.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/Vulkan/VulkanRHI.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"

#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/Document/EditorDocumentTypes.h"
#include "Shell/EditorSettingsDefaults.h"
#include "UI/Appearance/EditorAppearance.h"

#include <GLFW/glfw3.h>
#include <filesystem>
#include <optional>

namespace minEngine
{
    EditorCommandStack& Editor::GetCommandStack()
    {
        if (EditorCommandStack* activeStack = m_DocumentHost.GetActiveCommandStack())
        {
            return *activeStack;
        }
        return m_FallbackCommandStack;
    }

    std::optional<std::filesystem::path> Editor::ResolveProjectDescriptorPath(
        const CommandLineResult& commandLine)
    {
        if (!commandLine.ProjectDescriptorPath.has_value())
        {
            ME_LOG(LogEditor, Error, "Editor requires a project descriptor path.");
            ME_LOG(LogEditor, Error, "Usage: Maximum.exe --project <path-to-project.meproject> (see --help).");
            return std::nullopt;
        }

        const std::filesystem::path descriptorPath = *commandLine.ProjectDescriptorPath;
        if (descriptorPath.extension() != ".meproject")
        {
            ME_LOG(LogEditor, Error, 
                "Project path '{}' is not a .meproject descriptor.",
                descriptorPath.string());
            return std::nullopt;
        }

        if (!std::filesystem::exists(descriptorPath))
        {
            ME_LOG(LogEditor, Error, "Project descriptor '{}' does not exist.", descriptorPath.string());
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
        m_SubModules.push_back(m_AnimationGraphEditor.get());

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
        m_AnimationGraphEditor->Register(*this);

        m_DocumentHost.SetContext(this);
        RegisterBuiltinEditorDocumentTypes(*this, m_DocumentHost);

        ActivateSubModule(SceneEditor::kModuleId, true);
    }

    void Editor::BootstrapDocumentHost()
    {
        const std::string& scenePath = m_SceneEditor.GetOpenedSceneAssetPath();
        if (scenePath.empty())
        {
            return;
        }

        if (m_DocumentHost.FindSessionByAssetKey(scenePath) != nullptr)
        {
            return;
        }

        const std::string title = std::filesystem::path(scenePath).filename().string();
        m_DocumentHost.AdoptOpenDocument("Scene", scenePath, title);
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
        return ActivateSubModule(moduleId, true);
    }

    bool Editor::ActivateSubModule(std::string_view moduleId, bool resetLayout)
    {
        EditorSubModule* target = FindSubModule(moduleId);
        if (!target || !target->CanActivate())
        {
            return false;
        }

        if (m_ActiveSubModule == target)
        {
            m_EditorGUIManager.OnActiveSubModuleChanged(false);
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
        m_EditorGUIManager.OnActiveSubModuleChanged(resetLayout);
        return true;
    }

    bool Editor::OpenProject(const std::string& projectPath)
    {
        m_ProjectAssetWatcher.StopWatching();

        ProjectManager& projectManager = ProjectManager::Get();
        ProjectOpenResult result = projectManager.OpenProject(projectPath);
        if (result.IsSuccess())
        {
            ME_LOG(LogEditor, Info, result.Message);

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
                    ME_LOG(LogEditor, Warn, 
                        "Failed to load editor default scene '{}'.",
                        defaultSceneName);

                    if (defaultSceneName != "default" &&
                        m_SceneEditor.LoadScene(*this, "default"))
                    {
                        ME_LOG(LogEditor, Info, "Editor: loaded fallback scene 'default'.");
                    }
                }
                else
                {
                    ME_LOG(LogEditor, Info, 
                        "Editor default scene '{}' loaded successfully.",
                        defaultSceneName);
                }
            }

            m_MaterialEditor->RefreshMaterialList();
            m_AnimationGraphEditor->RefreshGraphList();
            m_SceneEditor.OnProjectOpened();
            if (SceneManager::HasInstance())
            {
                SetInspectingScene(SceneManager::Get().GetEditorScene());
            }
            BootstrapDocumentHost();

            const std::filesystem::path projectContentRoot = PathRegistry::Get().GetProjectContentRoot();
            m_ProjectAssetWatcher.StartWatching(projectContentRoot);
            m_ContentBrowser.GetModel().ResetForProject(projectContentRoot);

            return true;
        }

        ME_LOG(LogEditor, Error, result.Message);
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
        m_FallbackCommandStack.SetMaxDepth(ResolveMaxUndoStackDepth(projectCtx.Settings.Editor.MaxUndoStackDepth));
    }

    void Editor::ApplyAppearanceSettingsFromProject()
    {
        const ProjectContext& projectCtx = ProjectManager::Get().GetCurrentProjectCtx();
        m_Appearance.LoadFromAppearanceSettings(projectCtx.Settings.Appearance);
    }

    void Editor::ResetCommandStackForNewDocument()
    {
        m_FallbackCommandStack.Clear();
        m_DocumentHost.ClearAllCommandStacks();
    }

    bool Editor::InitializeImGuiBackend()
    {
        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(WindowSystem::Get().GetWindowHandle());
        if (windowHandle == nullptr)
        {
            ME_LOG(LogEditor, Error, "Editor: GLFW window handle is null.");
            return false;
        }

        const EditorImGuiBackend::RendererApi api = RHIBackendSelection::IsVulkan()
            ? EditorImGuiBackend::RendererApi::Vulkan
            : EditorImGuiBackend::RendererApi::OpenGL;

        if (!m_ImGuiBackend.Initialize(api, windowHandle))
        {
            ME_LOG(LogEditor, Error, "Editor: ImGui backend initialization failed.");
            return false;
        }

#if defined(MINENGINE_HAS_VULKAN)
        if (api == EditorImGuiBackend::RendererApi::Vulkan)
        {
            RHI* rhi = RenderSystem::Get().GetRHI();
            auto* vulkanRhi = dynamic_cast<VulkanRHI*>(rhi);
            if (vulkanRhi == nullptr || !m_ImGuiBackend.InitializeVulkanRenderer(*vulkanRhi))
            {
                ME_LOG(LogEditor, Error, "Editor: ImGui Vulkan renderer initialization failed.");
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
            ME_LOG(LogEditor, Info, "Editor: Vulkan full Editor path (ED-F01); scene renders to viewport RT.");
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
        m_AnimationGraphEditor = std::make_unique<AnimationGraphEditor>();

        m_EditorGUIManager.Initialize(*this);
        m_InputHub.Initialize(*this);
        m_DocumentHost.RegisterInputCommands(m_InputHub);
        m_PlayInEditorSession.SetHostContext(this);
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
        std::string documentSuffix;
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
                const char* dirtyMark = session.Dirty ? " *" : "";
                documentSuffix = materialLabel + dirtyMark;
            }
        }
        else if (
            m_ActiveSubModule
            && m_ActiveSubModule->GetModuleId() == AnimationGraphEditor::kModuleId)
        {
            if (AnimationGraphEditor* animGraphEditor =
                    dynamic_cast<AnimationGraphEditor*>(m_ActiveSubModule))
            {
                const AnimationGraphEditorSession& session = animGraphEditor->GetSession();
                std::string graphLabel = "Animation Graph";
                if (session.HasOpenGraph())
                {
                    const std::filesystem::path graphPath(session.AssetPath);
                    graphLabel = graphPath.filename().string();
                    if (graphLabel.empty())
                    {
                        graphLabel = session.AssetPath;
                    }
                }
                const char* dirtyMark = session.Dirty ? " *" : "";
                documentSuffix = graphLabel + dirtyMark;
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

            const char* dirtyMark = sceneEditor->IsSceneDirty() ? " *" : "";
            documentSuffix = sceneDisplayName + dirtyMark;
        }

        const std::string windowTitle = FormatEditorWindowTitle(documentSuffix);
        if (windowTitle != m_LastWindowTitle)
        {
            WindowSystem::Get().SetTitle(windowTitle.c_str());
            m_LastWindowTitle = windowTitle;
        }
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

        if (m_AnimationGraphEditor)
        {
            m_AnimationGraphEditor->Shutdown();
        }
        m_AnimationGraphEditor.reset();

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

    void Editor::RequestExit()
    {
        if (m_AssetWorkflow.TryRequestExit(*this))
        {
            m_ExitRequested = true;
        }
    }

    void Editor::SetInspectingScene(Scene* scene)
    {
        m_InspectingScene = scene;
    }

    void Editor::Run()
    {
        WindowSystem& windowSystem = WindowSystem::Get();
        RHI* rhi = RenderSystem::HasInstance() ? RenderSystem::Get().GetRHI() : nullptr;
        bool fontAtlasGpuMarked = false;

        while (!windowSystem.ShouldClose() && !m_ExitRequested)
        {
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

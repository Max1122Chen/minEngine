#include "Editor.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Material/MaterialEditorSession.h"

#include "main.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/CLI/ApplicationCommandLine.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Engine.h"
#include "Runtime/Function/Framework/Components/StaticMeshComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompileTypes.h"
#include "Runtime/Function/Render/RenderCamera.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RHI/RHIBackend.h"
#include "Runtime/Function/Render/SceneDrawDesc.h"
#include "Runtime/Function/Render/WindowSystem.h"
#include "Runtime/Platform/FileDialog/FileDialogService.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Resource/AssetManager.h"

#include <unordered_set>

#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/EditorSettingsDefaults.h"
#include "UI/Appearance/EditorAppearance.h"

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

    bool Editor::OpenProjectForVulkanSmoke(const std::string& projectPath)
    {
        m_ProjectAssetWatcher.StopWatching();

        ProjectManager& projectManager = ProjectManager::Get();
        ProjectOpenResult result = projectManager.OpenProject(projectPath);
        if (!result.IsSuccess())
        {
            ME_CORE_ERROR(result.Message);
            return false;
        }

        ME_CORE_INFO(result.Message);
        ApplyCommandStackSettingsFromProject();
        ResetCommandStackForNewDocument();

        // Prefer `default` — `test` references EnvironmentMap (cubemap; S07f).
        constexpr const char* kVulkanSmokeSceneName = "default";
        if (!m_SceneEditor.LoadScene(*this, kVulkanSmokeSceneName))
        {
            ME_CORE_ERROR(
                "Editor: Vulkan smoke failed to load scene '{}'.",
                kVulkanSmokeSceneName);
            return false;
        }

        ME_CORE_INFO(
            "Editor: Vulkan smoke scene '{}' loaded.",
            kVulkanSmokeSceneName);

        // S07d DoD: Unlit Base only. Lit materials still emit flat set0 shadow bindings (S07e).
        if (Scene* scene = m_SceneEditor.GetActiveScene())
        {
            uint32_t forcedUnlitCount = 0;
            std::unordered_set<Material*> recompiledMaterials;
            for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                for (const std::shared_ptr<StaticMeshComponent>& meshComponent :
                     gameObject->GetComponentsOfType<StaticMeshComponent>())
                {
                    Material* material = meshComponent ? meshComponent->GetMaterial() : nullptr;
                    if (!material || recompiledMaterials.find(material) != recompiledMaterials.end())
                    {
                        continue;
                    }

                    material->m_ShadingModel = MaterialShadingModel::Unlit;
                    if (!material->Compile())
                    {
                        ME_CORE_ERROR(
                            "Editor: Vulkan smoke failed to recompile material '{}' as Unlit.",
                            material->GetName());
                        return false;
                    }
                    recompiledMaterials.insert(material);
                    ++forcedUnlitCount;
                }
            }

            ME_CORE_INFO(
                "Editor: Vulkan smoke forced {} material(s) to Unlit for S07d Base.",
                forcedUnlitCount);
        }

        if (m_MaterialEditor)
        {
            m_MaterialEditor->RefreshMaterialList();
        }
        m_SceneEditor.OnProjectOpened();
        return true;
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

        if (RHIBackendSelection::IsOpenGL())
        {
            RenderSystem::Get().SetPresentPassEnabled(false);
        }

        if (RHIBackendSelection::IsVulkan())
        {
            // S07d acceptance: project + Forward Base → Present (no ImGui / shadows / sky / post).
            ME_CORE_WARN(
                "Editor: Vulkan S07d scene smoke (no ImGui). "
                "PresentToBackBuffer only; shadows/sky/post disabled.");

            RenderSystem::Get().SetPresentPassEnabled(true);
            m_MaterialEditor = std::make_unique<MaterialEditor>();

            const std::optional<std::filesystem::path> projectDescriptorPath =
                ResolveProjectDescriptorPath(commandLine);
            if (!projectDescriptorPath.has_value())
            {
                m_ExitRequested = true;
                return;
            }

            if (!OpenProjectForVulkanSmoke(projectDescriptorPath->string()))
            {
                m_ExitRequested = true;
                return;
            }

            RHI* rhi = RenderSystem::Get().GetRHI();
            WindowSystem& windowSystem = WindowSystem::Get();
            const uint32_t width = windowSystem.GetWidth();
            const uint32_t height = windowSystem.GetHeight();
            if (!rhi || width == 0 || height == 0)
            {
                ME_CORE_ERROR("Editor: Vulkan smoke viewport init failed (missing RHI or window size).");
                m_ExitRequested = true;
                return;
            }

            m_VulkanSmokeViewport.Initialize(rhi, width, height);
            if (Scene* scene = m_SceneEditor.GetActiveScene())
            {
                m_VulkanSmokeViewport.SetObservedScene(scene->GetRenderScene());
            }
            else
            {
                ME_CORE_WARN("Editor: Vulkan smoke — no active scene after OpenProject.");
            }

            if (RenderCamera* camera = m_VulkanSmokeViewport.GetCamera())
            {
                // default.mescene meshes are far from origin (Cube ~51,-37,-23; Armadillo ~73,-46,155).
                Vector3 focus(0.0f);
                uint32_t meshCount = 0;
                if (Scene* scene = m_SceneEditor.GetActiveScene())
                {
                    for (const std::shared_ptr<GameObject>& gameObject : scene->GetAllGameObjects())
                    {
                        if (!gameObject)
                        {
                            continue;
                        }
                        for (const std::shared_ptr<StaticMeshComponent>& meshComponent :
                             gameObject->GetComponentsOfType<StaticMeshComponent>())
                        {
                            if (!meshComponent)
                            {
                                continue;
                            }
                            focus += meshComponent->GetPosition();
                            ++meshCount;
                        }
                    }
                }

                if (meshCount > 0)
                {
                    focus /= static_cast<float>(meshCount);
                }
                else
                {
                    focus = Vector3(50.0f, -40.0f, 50.0f);
                }

                const Vector3 eye = focus + Vector3(-60.0f, 45.0f, 90.0f);
                camera->SetPosition(eye);
                camera->m_AspectRatio =
                    static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
                camera->m_zNear = 1.0f;
                camera->m_zFar = 2000.0f;
                camera->SetViewMatrix(glm::lookAt(eye, focus, Vector3(0.0f, 1.0f, 0.0f)));
                camera->UpdateProjectionMatrix();
                camera->UpdateViewProjMatrix();

                ME_CORE_INFO(
                    "Editor: Vulkan smoke camera eye=({:.1f},{:.1f},{:.1f}) focus=({:.1f},{:.1f},{:.1f}) meshes={}.",
                    eye.x,
                    eye.y,
                    eye.z,
                    focus.x,
                    focus.y,
                    focus.z,
                    meshCount);
            }

            m_VulkanSceneSmokeMode = true;
            return;
        }

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
        m_Appearance.RebuildUiFontAtlas();
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
        if (RHIBackendSelection::IsVulkan())
        {
            if (m_VulkanSceneSmokeMode)
            {
                m_VulkanSmokeViewport.Shutdown();
                m_VulkanSceneSmokeMode = false;
            }

            m_ProjectAssetWatcher.StopWatching();
            if (m_MaterialEditor)
            {
                m_MaterialEditor->Shutdown();
            }
            m_MaterialEditor.reset();
            m_ContextMenu.Shutdown();

            if (m_Engine)
            {
                m_Engine->Shutdown();
                delete m_Engine;
                m_Engine = nullptr;
            }
            return;
        }

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

        if (RHIBackendSelection::IsVulkan())
        {
            while (!windowSystem.ShouldClose() && !m_ExitRequested)
            {
                const float deltaTime = m_Engine->CalculateDeltaTime();
                m_Engine->PollEvents();
                m_Engine->TickLogicalFrame(deltaTime);
                SceneManager::Get().SendAllEndOfFrameUpdates();

                if (m_VulkanSceneSmokeMode && RenderSystem::HasInstance())
                {
                    // No shadows/sky/post — Texture2DArray / scene set= land in S07e/f.
                    const SceneDrawDesc desc = m_VulkanSmokeViewport.BuildDrawDesc(
                        SceneDrawFlags::PresentToBackBuffer);
                    RenderSystem::Get().SubmitSceneDraw(desc);

                    if (!m_VulkanSmokeLoggedFirstFrame)
                    {
                        ME_CORE_INFO(
                            "Editor: Vulkan S07d smoke — first frame SubmitSceneDraw queued "
                            "(PresentToBackBuffer, Unlit Base).");
                        m_VulkanSmokeLoggedFirstFrame = true;
                    }
                }

                m_Engine->TickRendererFrame(deltaTime);
                if (RenderSystem::HasInstance())
                {
                    RenderSystem::Get().PresentFrame();
                }
            }
            return;
        }

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

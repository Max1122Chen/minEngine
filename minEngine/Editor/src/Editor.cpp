#include "Editor.h"

#include "main.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Project/ProjectManager.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Resource/SceneSerializer.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"

#include "EditorDefaultScene.h"

#include <algorithm>
#include <array>

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

    Scene* Editor::GetActiveScene() const
    {
        return SceneManager::Get().GetCurrentActiveScene().get();
    }

    std::vector<GameObject*> Editor::GetHierarchyGameObjects() const
    {
        std::vector<GameObject*> result;
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return result;
        }

        const std::vector<std::shared_ptr<GameObject>>& gameObjects = scene->GetGameObjects();
        result.reserve(gameObjects.size());
        for (const std::shared_ptr<GameObject>& gameObject : gameObjects)
        {
            if (gameObject)
            {
                result.push_back(gameObject.get());
            }
        }

        std::sort(result.begin(), result.end(), [](const GameObject* lhs, const GameObject* rhs)
        {
            return lhs->GetID() < rhs->GetID();
        });

        return result;
    }

    GameObject* Editor::GetSelectedGameObject() const
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return nullptr;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        const auto iter = gameObjectsById.find(m_SelectedGameObjectId);
        if (iter == gameObjectsById.end())
        {
            return nullptr;
        }

        return iter->second;
    }

    void Editor::SelectGameObject(uint64_t gameObjectId)
    {
        m_SelectedGameObjectId = gameObjectId;
    }

    bool Editor::IsGameObjectSelected(uint64_t gameObjectId) const
    {
        return m_SelectedGameObjectId == gameObjectId;
    }

    std::string Editor::GetGameObjectDisplayName(const GameObject& gameObject) const
    {
        return gameObject.GetName();
    }

    std::string Editor::GetSelectedGameObjectName() const
    {
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return std::string();
        }

        return gameObject->GetName();
    }

    bool Editor::RenameGameObject(uint64_t gameObjectId, const std::string& newName)
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            return false;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        const auto iter = gameObjectsById.find(gameObjectId);
        if (iter == gameObjectsById.end() || iter->second == nullptr)
        {
            return false;
        }

        GameObject* gameObject = iter->second;
        std::string sanitizedName = newName;
        if (sanitizedName.empty())
        {
            sanitizedName = "GameObject_" + std::to_string(gameObject->GetID());
        }

        if (gameObject->GetName() != sanitizedName)
        {
            gameObject->SetName(sanitizedName);
            MarkSceneDirty();
        }

        return true;
    }

    void Editor::RenameSelectedGameObject(const std::string& newName)
    {
        RenameGameObject(m_SelectedGameObjectId, newName);
    }

    std::vector<std::string> Editor::GetAllComponentTypeNames() const
    {
        return std::vector<std::string>();
    }

    bool Editor::AddComponentToSelectedGameObject(const std::string& componentTypeName)
    {
        // std::shared_ptr<GameObject> gameObject = GetSelectedGameObject();
        // if (!gameObject || componentTypeName.empty())
        // {
        //     return false;
        // }

        // std::shared_ptr<Component> component = Reflection::ReflectionSystem::Get().CreateInstanceAs<Component>(componentTypeName);
        // if (!component)
        // {
        //     ME_CORE_WARN("[Editor] Failed to create component '{}'. It may be abstract or missing default constructor.", componentTypeName);
        //     return false;
        // }

        // component->SetOwner(gameObject.get());
        // gameObject->GetComponents().push_back(component);
        // MarkSceneDirty();
        return true;
    }

    void Editor::MarkSceneDirty()
    {
        m_SceneDirty = true;
    }

    void Editor::ClearSceneDirty()
    {
        m_SceneDirty = false;
    }

    bool Editor::CreateNewScene(const std::string& scenePath)
    {
        std::shared_ptr<Scene> scene = SceneManager::Get().CreateNewScene(scenePath);
        const bool success = static_cast<bool>(scene);
        if (success)
        {
            ClearSceneDirty();
        }
        return success;
    }

    bool Editor::OpenScene(const std::string& scenePath)
    {
        const bool success = SceneManager::Get().LoadScene(scenePath);
        if (success)
        {
            ClearSceneDirty();
        }
        return success;
    }

    bool Editor::SaveCurrentScene()
    {
        Scene* currentScene = GetActiveScene();
        if (!currentScene)
        {
            return false;
        }

        std::filesystem::path scenePath = GetCurrentScenePath();
        if (scenePath.empty())
        {
            scenePath = std::filesystem::path("Assets/Scenes/EditorDefault.scene.json");
            currentScene->sceneName = scenePath.string();
        }

        if (!SceneSerializer::SaveScene(scenePath, *currentScene))
        {
            return false;
        }

        ClearSceneDirty();
        return true;
    }

    bool Editor::SaveCurrentSceneAs(const std::filesystem::path& filePath)
    {
        Scene* currentScene = GetActiveScene();
        if (!currentScene)
        {
            return false;
        }

        std::filesystem::path outputPath = filePath;
        if (outputPath.empty())
        {
            return false;
        }

        if (outputPath.extension().empty())
        {
            outputPath += ".scene.json";
        }

        if (!SceneSerializer::SaveScene(outputPath, *currentScene))
        {
            return false;
        }

        currentScene->sceneName = outputPath.string();
        ClearSceneDirty();
        return true;
    }

    std::filesystem::path Editor::GetCurrentScenePath() const
    {
        Scene* currentScene = GetActiveScene();
        if (!currentScene)
        {
            return std::filesystem::path();
        }

        return std::filesystem::path(currentScene->GetSceneName());
    }

    void Editor::SyncSelectionWithScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            return;
        }

        const std::unordered_map<uint64_t, GameObject*>& gameObjectsById = scene->GetGameObjectsById();
        if (gameObjectsById.empty())
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            return;
        }

        if (gameObjectsById.find(m_SelectedGameObjectId) != gameObjectsById.end())
        {
            return;
        }

        const std::vector<GameObject*> hierarchyGameObjects = GetHierarchyGameObjects();
        if (!hierarchyGameObjects.empty())
        {
            m_SelectedGameObjectId = hierarchyGameObjects.front()->GetID();
        }
    }

    EditorViewportClient& Editor::GetOrCreateViewportClient(const std::string& viewportId,
                                                             const std::string& viewportTitle)
    {
        const std::string key = viewportId.empty() ? viewportTitle : viewportId;
        auto iter = m_ViewportClients.find(key);
        if (iter != m_ViewportClients.end() && iter->second)
        {
            return *iter->second;
        }

        auto client = std::make_unique<EditorViewportClient>(viewportTitle.empty() ? key : viewportTitle);
        EditorViewportClient* createdClient = client.get();
        m_ViewportClients[key] = std::move(client);
        return *createdClient;
    }

    EditorViewportClient* Editor::FindViewportClient(const std::string& viewportId)
    {
        const auto iter = m_ViewportClients.find(viewportId);
        if (iter == m_ViewportClients.end() || !iter->second)
        {
            return nullptr;
        }

        return iter->second.get();
    }

    const EditorViewportClient* Editor::FindViewportClient(const std::string& viewportId) const
    {
        const auto iter = m_ViewportClients.find(viewportId);
        if (iter == m_ViewportClients.end() || !iter->second)
        {
            return nullptr;
        }

        return iter->second.get();
    }

    void Editor::RemoveViewportClient(const std::string& viewportId)
    {
        m_ViewportClients.erase(viewportId);
    }

    void Editor::ClearViewportClients()
    {
        m_ViewportClients.clear();
    }

    void Editor::Initialize(int argc, char** argv)
    {
        m_Engine = new Engine();
        m_Engine->Initialize(argc, argv);

        RuntimeGlobalContext::GetRuntimeGlobalContext().m_RenderSystem->SetPresentPassEnabled(false);

        
        // Initialize ImGui for the editor window
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.FontGlobalScale = 1.50f;
        ImGui::StyleColorsDark();
        ApplyEditorTheme();

        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->GetWindowHandle());
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init();

        RuntimeGlobalContext::GetRuntimeGlobalContext().m_WindowSystem->SetCursorVisible(true);
        m_EditorGUIManager.Initialize(*this);

        ProjectManager& projManager = ProjectManager::Get();
        ProjectOpenResult result;
        bool openAttempted = false;
        std::filesystem::path requestedProjectPath;

        // Try opening project from command-line argument first.
        if (argc > 1 && argv != nullptr && argv[1] != nullptr)
        {
            requestedProjectPath = std::filesystem::path(argv[1]);
            if (!requestedProjectPath.empty())
            {
                openAttempted = true;
                result = projManager.OpenProject(requestedProjectPath);
                if (!result.IsSuccess())
                {
                    ME_CORE_WARN("[Editor] Failed to open requested project path '{}'. Reason: {}",
                                 requestedProjectPath.string(),
                                 result.Message);
                }
            }
        }

        // If command-line argument failed or is missing, try default project candidates.
        if (!result.IsSuccess())
        {
            const std::array<std::string, 4> defaultProjectCandidates = {
                "DefaultProjects/Playground",
                "../DefaultProjects/Playground",
                "../../DefaultProjects/Playground",
                "minEngine/DefaultProjects/Playground"
            };

            for (const std::string& relativePath : defaultProjectCandidates)
            {
                const std::filesystem::path candidatePath = std::filesystem::absolute(relativePath).lexically_normal();
                if (!std::filesystem::exists(candidatePath) || !std::filesystem::is_directory(candidatePath))
                {
                    continue;
                }

                openAttempted = true;
                result = projManager.OpenProject(candidatePath);
                if (result.IsSuccess())
                {
                    ME_CORE_INFO("[Editor] Opened default project candidate '{}'.", candidatePath.string());
                    break;
                }

                ME_CORE_WARN("[Editor] Failed to open default project candidate '{}'. Reason: {}",
                             candidatePath.string(),
                             result.Message);
            }
        }

        bool sceneLoaded = false;
        if (result.IsSuccess())
        {
            ME_CORE_INFO("[Editor] {}", result.Message);

            const ProjectContext& projectContext = projManager.GetCurrentProject();
            for (const std::string& diagnostic : projectContext.Diagnostics)
            {
                ME_CORE_WARN("[Editor][ProjectDiag] {}", diagnostic);
            }

            // Fallback chain: project default scene -> engine default scene.
            const std::filesystem::path startupScenePath = projManager.ResolveEditorStartupScenePath();
            if (!startupScenePath.empty())
            {
                sceneLoaded = OpenScene(startupScenePath.string());
                if (!sceneLoaded)
                {
                    ME_CORE_ERROR("[Editor] Failed to open project startup scene at '{}'.",
                                  startupScenePath.string());
                }
            }

            const std::filesystem::path engineDefaultScenePath = projManager.GetEngineDefaultScenePath();
            if (!sceneLoaded
                && !engineDefaultScenePath.empty()
                && startupScenePath.lexically_normal() != engineDefaultScenePath.lexically_normal())
            {
                sceneLoaded = OpenScene(engineDefaultScenePath.string());
                if (!sceneLoaded)
                {
                    ME_CORE_ERROR("[Editor] Failed to open engine default scene at '{}'.",
                                  engineDefaultScenePath.string());
                }
            }
        }
        else if (openAttempted)
        {
            ME_CORE_ERROR("[Editor] Failed to open project. Reason: {}", result.Message);
        }
        else
        {
            ME_CORE_WARN("[Editor] No project argument and no default project found. Fallback to bootstrap scene.");
        }

        // Final fallback: bootstrap a simple scene in memory.
        if (!sceneLoaded)
        {
            const std::filesystem::path bootstrapScenePath = projManager.GetEngineDefaultScenePath();
            CreateNewScene(bootstrapScenePath.string());
            if (Scene* scene = GetActiveScene())
            {
                PopulateEditorDefaultScene(*scene);
                MarkSceneDirty();
            }

            ME_CORE_WARN("[Editor] Using bootstrap in-memory default scene.");
        }

    }

    void Editor::Shutdown()
    {
        m_EditorGUIManager.Shutdown();
        ClearViewportClients();

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
            SyncSelectionWithScene();

            std::string sceneDisplayName = "Untitled";
            if (const Scene* activeScene = GetActiveScene())
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

            const char* dirtySuffix = IsSceneDirty() ? " *" : "";
            const std::string windowTitle = "minEngine Editor - " + sceneDisplayName + dirtySuffix;
            windowSystem->SetTitle(windowTitle.c_str());

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



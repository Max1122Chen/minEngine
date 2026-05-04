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
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"
#include "Serialization/JsonArchive.h"
#include "Serialization/Serializer.h"
#include "Resource/AssetManager.h"

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

    bool Editor::OpenProject(const std::string &projectPath)
    {
        ProjectManager& projectManager = ProjectManager::Get();
        ProjectOpenResult result = projectManager.OpenProject(projectPath);
        if (result.IsSuccess())
        {
            ME_CORE_INFO(result.Message);
            // Try to load the editor default scene after project is opened, if no scene is currently open
            const ProjectContext& projectCtx = projectManager.GetCurrentProjectCtx();
            bool sceneLoaded = SceneManager::Get().LoadScene(projectCtx.Settings.EditorDefaultSceneName);
            if(!sceneLoaded)
            {
                ME_CORE_WARN("Failed to load editor default scene '{}'.", projectCtx.Settings.EditorDefaultSceneName);
            }
            else
            {
                ME_CORE_INFO("Editor default scene '{}' loaded successfully.", projectCtx.Settings.EditorDefaultSceneName);
                return true;
            }
        }
        else
        {
            ME_CORE_ERROR(result.Message);
        }
        return false;
    }

    void Editor::CloseProject()
    {
        // TODO: implement this
    }

    Scene *Editor::GetActiveScene() const
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

        const std::vector<std::shared_ptr<GameObject>>& gameObjects = scene->GetAllGameObjects();
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
        return m_SelectedGameObject;
    }

    bool Editor::HasSelectedGameObject() const
    {
        return GetSelectedGameObject() != nullptr;
    }

    void Editor::SelectGameObject(uint64_t gameObjectId)
    {
        m_SelectedGameObjectId = gameObjectId;
        Scene* scene = GetActiveScene();
        if (!scene)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            m_SelectedGameObject = nullptr;
            return;
        }

        m_SelectedGameObject = scene->FindGameObjectById(gameObjectId);
        if (!m_SelectedGameObject)
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        }
    }

    void Editor::ClearSelectedGameObject()
    {
        m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
        m_SelectedGameObject = nullptr;
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

    const std::vector<std::string>& Editor::GetAllComponentTypeNames() const
    {
        return m_AllComponentTypeNames;
    }

    bool Editor::AddComponentToSelectedGameObject(const std::string& componentTypeName)
    {
        GameObject* gameObject = GetSelectedGameObject();
        if (!gameObject)        
        {
            return false;
        }
        if(!gameObject->AddComponent(componentTypeName))
        {
            ME_CORE_ERROR("Failed to add component of type '{}' to GameObject '{}'.", componentTypeName, gameObject->GetName());
            return false;
        }
        MarkSceneDirty();
        return true;
    }

    bool Editor::RemoveComponentFromGO(GameObject &gameObject, Component &targetComponent)
    {
        // Make sure the component belongs to the game object before trying to remove it.
        if (targetComponent.GetOwner() != &gameObject)
        {
            ME_CORE_ERROR("Failed to remove component '{}' from GameObject '{}': component does not belong to the specified GameObject.", targetComponent.GetClass()->GetName(), gameObject.GetName());
            return false;
        }
        if(gameObject.RemoveComponent(targetComponent))
        {
            MarkSceneDirty();
            return true;
        }
        return false;
    }

    void Editor::SaveCurrentScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)        
        {
            ME_CORE_ERROR("No active scene to save.");
            return;
        }
        if (SceneManager::Get().SaveCurrentScene())
        {
            ClearSceneDirty();
            ME_CORE_INFO("Scene '{}' saved successfully.", scene->GetSceneName());
        }
        else
        {
            ME_CORE_ERROR("Failed to save scene '{}'.", scene->GetSceneName());
        }
    }

    void Editor::AddEmptyGOToScene()
    {
        Scene* scene = GetActiveScene();
        if (!scene)        
        {
            ME_CORE_ERROR("No active scene to add GameObject to.");
            return;
        }
        std::shared_ptr<GameObject> newGO = scene->CreateGameObject();
        if (newGO)
        {
            newGO->SetName("GameObject");
            MarkSceneDirty();
            SelectGameObject(newGO->GetID());
            ME_CORE_INFO("Added new GameObject '{}' to scene '{}'.", newGO->GetName(), scene->GetSceneName());
        }
        else
        {
            ME_CORE_ERROR("Failed to create new GameObject in scene '{}'.", scene->GetSceneName());
        }
    }

    bool Editor::RemoveGameObjectFromScene(uint64_t gameObjectId)
    {
        Scene* scene = GetActiveScene();
        if (!scene)        
        {
            ME_CORE_ERROR("No active scene to remove GameObject from.");
            return false;
        }
        if (scene->RemoveGameObjectById(gameObjectId))
        {
            MarkSceneDirty();
            if (IsGameObjectSelected(gameObjectId))
            {
                ClearSelectedGameObject();
            }
            ME_CORE_INFO("Removed GameObject with ID {} from scene '{}'.", gameObjectId, scene->GetSceneName());
            return true;
        }
        else
        {
            ME_CORE_ERROR("Failed to remove GameObject with ID {} from scene '{}'.", gameObjectId, scene->GetSceneName());
            return false;
        }
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
        createdClient->m_Editor = this;
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

    void Editor::InitializeAllComponentTypeNames()
    {
        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
        const std::vector<const Reflection::MEClass*>& allClasses = reflectionSystem.GetAllClasses();
        for (const Reflection::MEClass* classInfo : allClasses)
        {
            if (classInfo->IsA(reflectionSystem.FindClass<Component>()))
            {
                m_AllComponentTypeNames.push_back(classInfo->GetName());
            }
        }
    }

    bool Editor::LoadEngineConfig()
    {
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path configPath = cwd / ("EngineConfig" + std::string(kEngineConfigExtension));
        if (!std::filesystem::exists(configPath))
        {
            ME_CORE_ERROR("Engine config file not found at current working directory: '{}'.", cwd.string());
            return false;
        }
        
        Serialization::JsonReaderArchive reader;
        Serialization::SerializeResult result = 
            Serialization::Serializer::FromFile(configPath.string(), 
                minEngine::Reflection::GetClassName<EngineConfig>(), 
                &m_EngineConfig,
                reader,
                Serialization::SerializerOptions
                {
                    .enumAsString = true,
                    .strictTypeCheck = true,
                    .skipUnknownField = true,
                    .writeObjectTypeName = false,
                    .allowObjectPtrSerialization = true
                });
        if (!result.ok)
        {
            ME_CORE_ERROR("Failed to load engine config from file '{}'. Error: {}. Field path: {}", configPath.string(), result.message, result.fieldPath);
            return false;
        }
        return true;
    }

    void Editor::Initialize(int argc, char** argv)
    {
        m_Engine = new Engine();
        m_Engine->Initialize(argc, argv);

        RuntimeGlobalContext::Get().m_RenderSystem->SetPresentPassEnabled(false);
        
        // Initialize ImGui for the editor window
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.FontGlobalScale = 1.50f;
        ImGui::StyleColorsDark();
        ApplyEditorTheme();

        GLFWwindow* windowHandle = static_cast<GLFWwindow*>(RuntimeGlobalContext::Get().m_WindowSystem->GetWindowHandle());
        ImGui_ImplGlfw_InitForOpenGL(windowHandle, true);
        ImGui_ImplOpenGL3_Init();

        RuntimeGlobalContext::Get().m_WindowSystem->SetCursorVisible(true);
        m_EditorGUIManager.Initialize(*this);

        InitializeAllComponentTypeNames();

        // Load engine config before opening project, so that any config in the file can take effect before project loading.
        bool engineConfigLoaded = LoadEngineConfig();
        // Scan engine default assets if engine config is loaded successfully, so that the default assets can be used when opening project and loading scenes.
        if (engineConfigLoaded)
        {
            AssetManager::Get().ScanAssets(m_EngineConfig.EngineDefaultAssetsRoot);
        }

        std::string projectPath;
        if (argc > 1)
        {
            projectPath = argv[1];
            
        }
        else
        {
            // Open a default project for easier development and testing if no project path is provided via command line arguments.
            // This is just a convenience for development and can be removed later.
            projectPath = "D:/Dev/GitRepo/minEngine/minEngine/MyMEProject";
        }

        bool projectOpened = OpenProject(projectPath);
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
        WindowSystem* windowSystem = RuntimeGlobalContext::Get().m_WindowSystem.get();
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



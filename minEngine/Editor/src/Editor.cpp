#include "Editor.h"

#include "main.h"

#include "imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneSerializer.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Core/Reflection/ReflectionSample.h"

#include <algorithm>

namespace minEngine
{
    std::shared_ptr<Scene> Editor::GetActiveScene() const
    {
        return SceneManager::GetSceneManager().GetCurrentActiveScene();
    }

    std::vector<std::shared_ptr<GameObject>> Editor::GetHierarchyGameObjects() const
    {
        std::vector<std::shared_ptr<GameObject>> result;
        const std::shared_ptr<Scene> scene = GetActiveScene();
        if (!scene)
        {
            return result;
        }

        result.reserve(scene->m_GameObjects.size());
        for (const auto& [id, gameObject] : scene->m_GameObjects)
        {
            if (gameObject)
            {
                result.push_back(gameObject);
            }
        }

        std::sort(result.begin(), result.end(), [](const std::shared_ptr<GameObject>& lhs, const std::shared_ptr<GameObject>& rhs)
        {
            return lhs->m_ID < rhs->m_ID;
        });

        return result;
    }

    std::shared_ptr<GameObject> Editor::GetSelectedGameObject() const
    {
        const std::shared_ptr<Scene> scene = GetActiveScene();
        if (!scene)
        {
            return nullptr;
        }

        const auto iter = scene->m_GameObjects.find(m_SelectedGameObjectId);
        if (iter == scene->m_GameObjects.end())
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
        const std::shared_ptr<GameObject> gameObject = GetSelectedGameObject();
        if (!gameObject)
        {
            return std::string();
        }

        return gameObject->GetName();
    }

    bool Editor::RenameGameObject(uint64_t gameObjectId, const std::string& newName)
    {
        const std::shared_ptr<Scene> scene = GetActiveScene();
        if (!scene)
        {
            return false;
        }

        const auto iter = scene->m_GameObjects.find(gameObjectId);
        if (iter == scene->m_GameObjects.end() || !iter->second)
        {
            return false;
        }

        std::shared_ptr<GameObject> gameObject = iter->second;
        std::string sanitizedName = newName;
        if (sanitizedName.empty())
        {
            sanitizedName = "GameObject_" + std::to_string(gameObject->m_ID);
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
        std::shared_ptr<Scene> scene = SceneManager::GetSceneManager().CreateNewScene(scenePath);
        const bool success = static_cast<bool>(scene);
        if (success)
        {
            ClearSceneDirty();
        }
        return success;
    }

    bool Editor::OpenScene(const std::string& scenePath)
    {
        SceneManager::GetSceneManager().LoadScene(scenePath);
        const bool success = static_cast<bool>(GetActiveScene());
        if (success)
        {
            ClearSceneDirty();
        }
        return success;
    }

    bool Editor::SaveCurrentScene()
    {
        std::shared_ptr<Scene> currentScene = GetActiveScene();
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

        if (!SceneSerializer::SaveScene(*currentScene, scenePath))
        {
            return false;
        }

        ClearSceneDirty();
        return true;
    }

    bool Editor::SaveCurrentSceneAs(const std::filesystem::path& filePath)
    {
        std::shared_ptr<Scene> currentScene = GetActiveScene();
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

        if (!SceneSerializer::SaveScene(*currentScene, outputPath))
        {
            return false;
        }

        currentScene->sceneName = outputPath.string();
        ClearSceneDirty();
        return true;
    }

    std::filesystem::path Editor::GetCurrentScenePath() const
    {
        const std::shared_ptr<Scene> currentScene = GetActiveScene();
        if (!currentScene)
        {
            return std::filesystem::path();
        }

        return std::filesystem::path(currentScene->GetSceneName());
    }

    void Editor::SyncSelectionWithScene()
    {
        const std::shared_ptr<Scene> scene = GetActiveScene();
        if (!scene || scene->m_GameObjects.empty())
        {
            m_SelectedGameObjectId = std::numeric_limits<uint64_t>::max();
            return;
        }

        if (scene->m_GameObjects.find(m_SelectedGameObjectId) != scene->m_GameObjects.end())
        {
            return;
        }

        auto iter = std::min_element(scene->m_GameObjects.begin(), scene->m_GameObjects.end(),
            [](const auto& lhs, const auto& rhs)
            {
                return lhs.first < rhs.first;
            });

        if (iter != scene->m_GameObjects.end())
        {
            m_SelectedGameObjectId = iter->first;
        }
    }

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

        CreateNewScene("Assets/Scenes/EditorDefault.scene.json");

        const Reflection::TypeInfo* reflectionSampleTypeInfo = Reflection::ReflectionSystem::Get().GetTypeInfo<ReflectionSampleClass>();
        if (reflectionSampleTypeInfo == nullptr)
        {
            ME_CORE_WARN("[Reflection] ReflectionSampleClass type info not found at editor startup.");
        }
        else
        {
            ME_CORE_INFO("[Reflection] Type: {} (size: {}, properties: {})",
                         reflectionSampleTypeInfo->name,
                         reflectionSampleTypeInfo->size,
                         reflectionSampleTypeInfo->fields.size());
            for (const auto& field : reflectionSampleTypeInfo->fields)
            {
                ME_CORE_INFO("[Reflection] ReflectionSampleClass property: {} | type: {} | hasAccessor: {}",
                             field.name,
                             field.typeName,
                             field.mutableAccessor != nullptr);

                if (field.metadata.empty())
                {
                    ME_CORE_INFO("[Reflection]   metadata: <none>");
                    continue;
                }

                for (const auto& metadataEntry : field.metadata)
                {
                    ME_CORE_INFO("[Reflection]   metadata: {} = {}",
                                 metadataEntry.first,
                                 metadataEntry.second);
                }
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
            SyncSelectionWithScene();

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



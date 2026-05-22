#include "EditorGUIManager.h"

#include "Editor.h"
#include "EditorUIMode.h"
#include "Material/MaterialEditor.h"

#include "imgui_internal.h"

#include "UI/EditorWindows/ConsoleWindow.h"
#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/InspectorWindow.h"
#include "UI/EditorWindows/MainMenuWindow.h"
#include "UI/EditorWindows/SceneEditingViewportWindow.h"
#include "UI/EditorWindows/MaterialGraphWindow.h"
#include "UI/EditorWindows/MaterialPreviewWindow.h"
#include "UI/EditorWindows/MaterialDetailsWindow.h"

namespace minEngine
{
    void EditorGUIManager::Initialize(Editor& editor)
    {
        m_Editor = &editor;
        m_UIMode = EditorUIMode::SceneEditing;

        RegisterWindow(std::make_unique<MainMenuWindow>(editor));
        RegisterWindow(std::make_unique<SceneEditingViewportWindow>(editor));
        RegisterWindow(std::make_unique<HierarchyWindow>(editor));
        RegisterWindow(std::make_unique<InspectorWindow>(editor));
        RegisterWindow(std::make_unique<ConsoleWindow>(editor));

        RegisterWindow(std::make_unique<MaterialGraphWindow>(editor));
        RegisterWindow(std::make_unique<MaterialPreviewWindow>(editor));
        RegisterWindow(std::make_unique<MaterialDetailsWindow>(editor));

        ApplyUIModeWindowVisibility();
    }

    void EditorGUIManager::SetUIMode(EditorUIMode mode)
    {
        if (m_UIMode == mode)
        {
            return;
        }

        m_UIMode = mode;
        ApplyUIModeWindowVisibility();

        if (m_Editor)
        {
            m_Editor->requestResetLayout = true;
            m_Editor->dockLayoutInitialized = false;

            if (mode == EditorUIMode::MaterialEditing)
            {
                m_Editor->GetMaterialEditor().OnEnterMode();
            }
            else
            {
                m_Editor->GetMaterialEditor().OnExitMode();
            }
        }
    }

    void EditorGUIManager::ApplyUIModeWindowVisibility()
    {
        for (const auto& window : m_Windows)
        {
            if (!window)
            {
                continue;
            }

            const EditorWindowSuite suite = window->GetWindowSuite();
            if (suite == EditorWindowSuite::Shared)
            {
                continue;
            }

            const bool shouldOpen = IsWindowActiveForUIMode(suite, m_UIMode);
            window->SetOpen(shouldOpen);
        }
    }

    Editor& EditorGUIManager::GetEditor()
    {
        return *m_Editor;
    }

    const Editor& EditorGUIManager::GetEditor() const
    {
        return *m_Editor;
    }

    void EditorGUIManager::Tick(float deltaTime)
    {
        Editor& editor = GetEditor();

        const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
        TickLayout(dockspaceId);

        if (m_UIMode == EditorUIMode::MaterialEditing)
        {
            editor.GetMaterialEditor().Tick(deltaTime);
        }

        TickWindows();
        DrawWindows();

        editor.lastDeltaTime = deltaTime;
    }

    void EditorGUIManager::TickLayout(ImGuiID dockspaceId)
    {
        if (dockspaceId == 0)
        {
            return;
        }

        Editor& editor = GetEditor();
        if (!editor.dockLayoutInitialized || editor.requestResetLayout)
        {
            if (m_UIMode == EditorUIMode::MaterialEditing)
            {
                BuildMaterialEditingDockLayout(dockspaceId);
            }
            else
            {
                BuildSceneEditingDockLayout(dockspaceId);
            }

            editor.dockLayoutInitialized = true;
            editor.requestResetLayout = false;
        }
    }

    void EditorGUIManager::BuildSceneEditingDockLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID mainArea = dockspaceId;
        ImGuiID inspectorArea = ImGui::DockBuilderSplitNode(mainArea, ImGuiDir_Right, kDefaultInspectorSplitRatio, nullptr, &mainArea);
        ImGuiID hierarchyArea = ImGui::DockBuilderSplitNode(mainArea, ImGuiDir_Right, kDefaultHierarchySplitRatio, nullptr, &mainArea);
        ImGuiID consoleArea = ImGui::DockBuilderSplitNode(mainArea, ImGuiDir_Down, kDefaultConsoleSplitRatio, nullptr, &mainArea);

        ImGui::DockBuilderDockWindow("Viewport", mainArea);
        ImGui::DockBuilderDockWindow("Console", consoleArea);
        ImGui::DockBuilderDockWindow("Hierarchy", hierarchyArea);
        ImGui::DockBuilderDockWindow("Inspector", inspectorArea);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorGUIManager::BuildMaterialEditingDockLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID centerArea = dockspaceId;
        ImGuiID consoleArea = ImGui::DockBuilderSplitNode(centerArea, ImGuiDir_Down, 0.28f, nullptr, &centerArea);
        ImGuiID graphArea = ImGui::DockBuilderSplitNode(centerArea, ImGuiDir_Right, 0.58f, nullptr, &centerArea);
        ImGuiID detailsArea = ImGui::DockBuilderSplitNode(centerArea, ImGuiDir_Down, 0.42f, nullptr, &centerArea);

        ImGui::DockBuilderDockWindow("Material Graph", graphArea);
        ImGui::DockBuilderDockWindow("Material Preview", centerArea);
        ImGui::DockBuilderDockWindow("Material Details", detailsArea);
        ImGui::DockBuilderDockWindow("Console", consoleArea);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorGUIManager::Shutdown()
    {
        for (auto iter = m_Windows.rbegin(); iter != m_Windows.rend(); ++iter)
        {
            (*iter)->OnDetach();
        }
        m_Windows.clear();
        m_IndexById.clear();
        m_Editor = nullptr;
    }

    EditorWindow* EditorGUIManager::RegisterWindow(std::unique_ptr<EditorWindow> window)
    {
        if (!window)
        {
            return nullptr;
        }

        const std::string id = window->GetId();
        const auto existing = m_IndexById.find(id);
        if (existing != m_IndexById.end())
        {
            return m_Windows[existing->second].get();
        }

        window->OnAttach();
        m_Windows.emplace_back(std::move(window));
        const size_t newIndex = m_Windows.size() - 1;
        m_IndexById[id] = newIndex;
        return m_Windows[newIndex].get();
    }

    bool EditorGUIManager::ToggleWindow(const std::string& id)
    {
        EditorWindow* window = FindWindow(id);
        if (!window)
        {
            return false;
        }

        window->SetOpen(!window->IsOpen());
        return true;
    }

    EditorWindow* EditorGUIManager::FindWindow(const std::string& id)
    {
        const auto iter = m_IndexById.find(id);
        if (iter == m_IndexById.end())
        {
            return nullptr;
        }

        return m_Windows[iter->second].get();
    }

    const EditorWindow* EditorGUIManager::FindWindow(const std::string& id) const
    {
        const auto iter = m_IndexById.find(id);
        if (iter == m_IndexById.end())
        {
            return nullptr;
        }

        return m_Windows[iter->second].get();
    }

    void EditorGUIManager::TickWindows()
    {
        for (const auto& window : m_Windows)
        {
            if (!window->IsOpen())
            {
                continue;
            }

            if (!IsWindowActiveForUIMode(window->GetWindowSuite(), m_UIMode))
            {
                continue;
            }

            window->OnTick();
        }
    }

    void EditorGUIManager::DrawWindows()
    {
        for (const auto& window : m_Windows)
        {
            if (!window->IsOpen())
            {
                continue;
            }

            if (!IsWindowActiveForUIMode(window->GetWindowSuite(), m_UIMode))
            {
                continue;
            }

            window->OnDraw();
        }
    }
}

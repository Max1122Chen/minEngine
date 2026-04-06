#include "EditorGUIManager.h"

#include "Editor.h"

#include "imgui_internal.h"

#include "UI/EditorWindows/ConsoleWindow.h"
#include "UI/EditorWindows/HierarchyWindow.h"
#include "UI/EditorWindows/InspectorWindow.h"
#include "UI/EditorWindows/MainMenuWindow.h"
#include "UI/EditorWindows/ToolbarWindow.h"
#include "UI/EditorWindows/ViewportWindow.h"

namespace minEngine
{
    void EditorGUIManager::Initialize(Editor& editor)
    {
        m_Editor = &editor;
        RegisterWindow(std::make_unique<MainMenuWindow>(editor));
        // RegisterWindow(std::make_unique<ToolbarWindow>(editor));
        RegisterWindow(std::make_unique<ViewportWindow>(editor));
        RegisterWindow(std::make_unique<HierarchyWindow>(editor));
        RegisterWindow(std::make_unique<InspectorWindow>(editor));
        RegisterWindow(std::make_unique<ConsoleWindow>(editor));
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
            BuildDefaultDockLayout(dockspaceId);
            editor.dockLayoutInitialized = true;
            editor.requestResetLayout = false;
        }
    }

    void EditorGUIManager::BuildDefaultDockLayout(ImGuiID dockspaceId)
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

            window->OnDraw();
        }

        Editor& editor = GetEditor();
        if (editor.showDemoWindow)
        {
            ImGui::ShowDemoWindow(&editor.showDemoWindow);
        }
    }
}

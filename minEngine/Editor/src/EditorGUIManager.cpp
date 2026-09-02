#include "EditorGUIManager.h"

#include "Shell/EditorChrome.h"
#include "Shell/EditorSubModule.h"
#include "Shell/IEditorContext.h"

#include "imgui_internal.h"

namespace minEngine
{
    void EditorGUIManager::Initialize(IEditorContext& context)
    {
        m_Context = &context;
        ApplyActiveSubModuleWindowVisibility();
    }

    void EditorGUIManager::OnActiveSubModuleChanged()
    {
        ApplyActiveSubModuleWindowVisibility();

        if (!m_Context)
        {
            return;
        }

        m_Context->RequestResetLayout() = true;
        m_Context->DockLayoutInitialized() = false;
    }

    void EditorGUIManager::ApplyActiveSubModuleWindowVisibility()
    {
        for (const auto& window : m_Windows)
        {
            if (!window)
            {
                continue;
            }

            if (window->GetOwnerModuleId().empty())
            {
                continue;
            }

            window->SetOpen(window->IsVisibleForActiveModule());
        }
    }

    void EditorGUIManager::Tick(float deltaTime)
    {
        if (!m_Context)
        {
            return;
        }

        EditorChrome::BeginFrame(*m_Context);

        // Dock into the main viewport work area (accounts for MainMenuBar).
        // Do not wrap DockSpace in a padded host window — that creates a visible inset frame.
        const ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(
            0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

        TickLayout(dockspaceId);

        TickWindows();
        DrawWindows();

        m_Context->SetLastDeltaTime(deltaTime);
    }

    void EditorGUIManager::TickLayout(ImGuiID dockspaceId)
    {
        if (!m_Context || dockspaceId == 0)
        {
            return;
        }

        if (!m_Context->DockLayoutInitialized() || m_Context->RequestResetLayout())
        {
            if (EditorSubModule* active = m_Context->GetActiveSubModule())
            {
                active->ApplyDefaultLayout(*m_Context, dockspaceId);
            }

            m_Context->DockLayoutInitialized() = true;
            m_Context->RequestResetLayout() = false;
        }
    }

    void EditorGUIManager::Shutdown()
    {
        for (auto iter = m_Windows.rbegin(); iter != m_Windows.rend(); ++iter)
        {
            (*iter)->OnDetach();
        }
        m_Windows.clear();
        m_IndexById.clear();
        m_Context = nullptr;
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
            if (!window->IsOpen() || !window->IsVisibleForActiveModule())
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
            if (!window->IsOpen() || !window->IsVisibleForActiveModule())
            {
                continue;
            }

            if (window->GetId() == "main_menu")
            {
                continue;
            }

            window->OnDraw();
        }
    }
}

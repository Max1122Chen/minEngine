#pragma once

#include "Core.h"

#include "imgui.h"

#include "UI/EditorWindows/EditorWindow.h"

namespace minEngine
{
    class IEditorContext;

    class EditorGUIManager
    {
    public:
        void Initialize(IEditorContext& context);
        void Tick(float deltaTime);
        void Shutdown();

        void OnActiveSubModuleChanged();

        EditorWindow* RegisterWindow(std::unique_ptr<EditorWindow> window);
        bool ToggleWindow(const std::string& id);
        EditorWindow* FindWindow(const std::string& id);
        const EditorWindow* FindWindow(const std::string& id) const;

        const std::vector<std::unique_ptr<EditorWindow>>& GetWindows() const
        {
            return m_Windows;
        }

    private:
        void ApplyActiveSubModuleWindowVisibility();
        void TickLayout(ImGuiID dockspaceId);
        void TickWindows();
        void DrawWindows();

        std::vector<std::unique_ptr<EditorWindow>> m_Windows;
        std::unordered_map<std::string, size_t> m_IndexById;
        IEditorContext* m_Context = nullptr;
    };
}

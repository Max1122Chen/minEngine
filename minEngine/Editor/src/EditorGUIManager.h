#pragma once
#include "Core.h"

#include "imgui.h"

#include "UI/EditorWindows/EditorWindow.h"

namespace minEngine
{
    class Editor;
    enum class EditorUIMode;
}

namespace minEngine
{
    class EditorGUIManager
    {
    public:
        void Initialize(Editor& editor);
        void Tick(float deltaTime);
        void Shutdown();

        EditorUIMode GetUIMode() const { return m_UIMode; }
        void SetUIMode(EditorUIMode mode);

        EditorWindow* RegisterWindow(std::unique_ptr<EditorWindow> window);
        bool ToggleWindow(const std::string& id);
        EditorWindow* FindWindow(const std::string& id);
        const EditorWindow* FindWindow(const std::string& id) const;

        const std::vector<std::unique_ptr<EditorWindow>>& GetWindows() const
        {
            return m_Windows;
        }

    private:
        static constexpr float kDefaultInspectorSplitRatio = 0.22f;
        static constexpr float kDefaultHierarchySplitRatio = 0.28f;
        static constexpr float kDefaultConsoleSplitRatio = 0.30f;

        Editor& GetEditor();
        const Editor& GetEditor() const;

        void TickLayout(ImGuiID dockspaceId);
        void BuildSceneEditingDockLayout(ImGuiID dockspaceId);
        void BuildMaterialEditingDockLayout(ImGuiID dockspaceId);
        void ApplyUIModeWindowVisibility();

        void TickWindows();
        void DrawWindows();

    private:
        EditorUIMode m_UIMode = EditorUIMode::SceneEditing;
        std::vector<std::unique_ptr<EditorWindow>> m_Windows;
        std::unordered_map<std::string, size_t> m_IndexById;
        Editor* m_Editor = nullptr;
    };
}
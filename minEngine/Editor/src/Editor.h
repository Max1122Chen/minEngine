#pragma once

#include "Core.h"
#include "minEngine.h"

#include "EditorGUIManager.h"

namespace minEngine
{
    class Editor : public Application
    {
    public:
        Editor() = default;
        ~Editor() override = default;

        void Initialize() override;
        void Shutdown() override;
        void Run() override;

        EditorGUIManager& GetGUIManager()
        {
            return m_EditorGUIManager;
        }

        const EditorGUIManager& GetGUIManager() const
        {
            return m_EditorGUIManager;
        }

        void RequestExit()
        {
            m_ExitRequested = true;
        }

    public:
        bool isPlaying = false;
        bool showDemoWindow = false;
        float lastDeltaTime = 0.0f;

        bool dockLayoutInitialized = false;
        bool requestResetLayout = false;

        bool viewportHovered = false;
        bool viewportFocused = false;

        std::vector<std::string> hierarchyItems {"MainCamera", "DirectionalLight", "Cube_01", "Plane_01"};
        int selectedHierarchyIndex = 0;
        std::string inspectorName = "MainCamera";

        float inspectorPosition[3] = {0.0f, 0.0f, 0.0f};
        float inspectorRotation[3] = {0.0f, 0.0f, 0.0f};
        float inspectorScale[3] = {1.0f, 1.0f, 1.0f};
        float inspectorTint[3] = {1.0f, 1.0f, 1.0f};

    private:
        Engine* m_Engine = nullptr;
        EditorGUIManager m_EditorGUIManager;
        bool m_ExitRequested = false;
    };

    Application* CreateApplication();
}

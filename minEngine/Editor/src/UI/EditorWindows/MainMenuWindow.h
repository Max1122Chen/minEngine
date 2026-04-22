#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Function/Framework/Scene/SceneManager.h"

namespace minEngine
{
    class MainMenuWindow final : public EditorWindow
    {
    public:
        explicit MainMenuWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override;

    private:
        void DrawFileMenu();
        void DrawEditMenu();
        void DrawViewMenu();
        void DrawToolsMenu();
        void DrawHelpMenu();

        const std::string m_Id = "main_menu";
        const std::string m_Title = "MainMenu";
    };
}

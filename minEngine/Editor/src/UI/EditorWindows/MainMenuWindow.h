#pragma once

#include "Core.h"

#include "imgui.h"

#include "UI/EditorWindows/EditorWindow.h"

namespace minEngine
{
    class MainMenuWindow final : public EditorWindow
    {
    public:
        explicit MainMenuWindow(IEditorContext& context)
            : EditorWindow(context)
        {
        }

        const std::string& GetId() const override { return m_Id; }
        const std::string& GetTitle() const override { return m_Title; }

        void OnDraw() override;

    private:
        void DrawFileMenu();
        void DrawEditMenu();
        void DrawViewMenu();
        void DrawWindowModeMenu();
        void DrawToolsMenu();
        void DrawHelpMenu();

        const std::string m_Id = "main_menu";
        const std::string m_Title = "MainMenu";
    };
}

#include "EditorChrome.h"

#include "EditorGUIManager.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/MainMenuWindow.h"

namespace minEngine
{
    void EditorChrome::BeginFrame(IEditorContext& context)
    {
        if (MainMenuWindow* mainMenuWindow =
                dynamic_cast<MainMenuWindow*>(context.GetGUIManager().FindWindow("main_menu")))
        {
            mainMenuWindow->DrawChrome();
        }

        // Reserve strip for document tabs before DockSpaceOverViewport so open/focus
        // that happens during DrawWindows still lays out under a stable work area.
        context.GetDocumentHost().ReserveTabBarWorkArea();
    }

    void EditorChrome::EndFrame(IEditorContext& context)
    {
        // Draw after windows/modals so OpenOrFocus in the same frame can SetSelected.
        if (MainMenuWindow* mainMenuWindow =
                dynamic_cast<MainMenuWindow*>(context.GetGUIManager().FindWindow("main_menu")))
        {
            mainMenuWindow->DrawDocumentTabBar();
        }
    }
}

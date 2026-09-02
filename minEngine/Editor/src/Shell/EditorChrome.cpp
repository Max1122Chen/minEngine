#include "EditorChrome.h"

#include "EditorGUIManager.h"
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
    }
}

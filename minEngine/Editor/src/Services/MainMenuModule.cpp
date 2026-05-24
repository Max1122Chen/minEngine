#include "Services/MainMenuModule.h"

#include "EditorGUIManager.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/MainMenuWindow.h"

namespace minEngine
{
    void MainMenuModule::Register(IEditorContext& context)
    {
        context.GetGUIManager().RegisterWindow(std::make_unique<MainMenuWindow>(context));
    }

    void MainMenuModule::Shutdown()
    {
    }
}

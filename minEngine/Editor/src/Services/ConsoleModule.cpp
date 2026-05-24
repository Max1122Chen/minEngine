#include "Services/ConsoleModule.h"

#include "EditorGUIManager.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/ConsoleWindow.h"

namespace minEngine
{
    void ConsoleModule::Register(IEditorContext& context)
    {
        context.GetGUIManager().RegisterWindow(std::make_unique<ConsoleWindow>(context));
    }

    void ConsoleModule::Shutdown()
    {
    }
}

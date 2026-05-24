#include "Services/InspectorModule.h"

#include "EditorGUIManager.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/InspectorWindow.h"

namespace minEngine
{
    void InspectorModule::Register(IEditorContext& context)
    {
        context.GetGUIManager().RegisterWindow(std::make_unique<InspectorWindow>(context));
    }

    void InspectorModule::Shutdown()
    {
    }
}

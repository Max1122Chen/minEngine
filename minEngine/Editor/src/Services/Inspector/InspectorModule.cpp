#include "Services/Inspector/InspectorModule.h"

#include "EditorGUIManager.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/InspectorWindow.h"

namespace minEngine
{
    void InspectorModule::Register(IEditorContext& context)
    {
        m_Context = &context;
        context.GetGUIManager().RegisterWindow(std::make_unique<InspectorWindow>(context));
    }

    void InspectorModule::Shutdown()
    {
        m_ThumbnailService.Shutdown();
        m_Context = nullptr;
    }

    void InspectorModule::SetInspectionTarget(const AssetMeta* meta)
    {
        m_ThumbnailService.SetInspectionTarget(meta);
    }

    void InspectorModule::ClearInspectionTarget()
    {
        m_ThumbnailService.ClearInspectionTarget();
    }
}

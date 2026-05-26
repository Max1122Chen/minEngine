#include "Services/InspectorModule.h"

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
        m_AssetInspection.Shutdown();
        m_Context = nullptr;
    }

    void InspectorModule::SetInspectionTarget(const AssetMeta* meta)
    {
        m_AssetInspection.SetInspectionTarget(meta);
    }

    void InspectorModule::ClearInspectionTarget()
    {
        m_AssetInspection.ClearInspectionTarget();
    }
}

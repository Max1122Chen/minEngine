#include "Services/ContentBrowser/ContentBrowserModule.h"

#include "EditorGUIManager.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/IEditorContext.h"
#include "UI/EditorWindows/ContentBrowserWindow.h"

namespace minEngine
{
    void ContentBrowserModule::Register(IEditorContext& context)
    {
        m_Context = &context;
        m_Model = std::make_unique<AssetTreeModel>();
        context.GetGUIManager().RegisterWindow(
            std::make_unique<ContentBrowserWindow>(context, *m_Model));
    }

    void ContentBrowserModule::Shutdown()
    {
        if (m_Model)
        {
            m_Model->Clear();
        }
        m_Model.reset();
        m_Context = nullptr;
    }

    AssetTreeModel& ContentBrowserModule::GetModel()
    {
        return *m_Model;
    }

    const AssetTreeModel& ContentBrowserModule::GetModel() const
    {
        return *m_Model;
    }
}

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
        m_Selection.Clear();
        m_PendingAssetRenamePath.clear();
        m_PendingBatchRename = false;
        m_PendingCreateFolder = false;
        m_PendingCreateFolderParent.clear();
        m_PendingFolderRenamePath.clear();
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

    void ContentBrowserModule::RequestBeginAssetRename(std::string assetPath)
    {
        m_PendingAssetRenamePath = std::move(assetPath);
    }

    std::string ContentBrowserModule::ConsumePendingAssetRenamePath()
    {
        std::string pending = std::move(m_PendingAssetRenamePath);
        m_PendingAssetRenamePath.clear();
        return pending;
    }

    void ContentBrowserModule::RequestBeginBatchRename()
    {
        m_PendingBatchRename = true;
    }

    bool ContentBrowserModule::ConsumePendingBatchRename()
    {
        const bool pending = m_PendingBatchRename;
        m_PendingBatchRename = false;
        return pending;
    }

    void ContentBrowserModule::RequestCreateFolderDialog(std::string parentDirectoryRel)
    {
        m_PendingCreateFolder = true;
        m_PendingCreateFolderParent = std::move(parentDirectoryRel);
    }

    bool ContentBrowserModule::ConsumePendingCreateFolder(std::string& outParentDirectoryRel)
    {
        if (!m_PendingCreateFolder)
        {
            return false;
        }
        m_PendingCreateFolder = false;
        outParentDirectoryRel = std::move(m_PendingCreateFolderParent);
        m_PendingCreateFolderParent.clear();
        return true;
    }
    void ContentBrowserModule::RequestBeginFolderRename(std::string directoryRel)
    {
        m_PendingFolderRenamePath = std::move(directoryRel);
    }

    std::string ContentBrowserModule::ConsumePendingFolderRenamePath()
    {
        std::string path = std::move(m_PendingFolderRenamePath);
        m_PendingFolderRenamePath.clear();
        return path;
    }

}

#pragma once

#include "Core.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Services/ContentBrowser/ContentBrowserSelection.h"
#include "Shell/EditorServiceModule.h"

#include <memory>
#include <string>

namespace minEngine
{
    class IEditorContext;

    class ContentBrowserModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "ContentBrowser";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;

        AssetTreeModel& GetModel();
        const AssetTreeModel& GetModel() const;

        ContentBrowserSelection& GetSelection() { return m_Selection; }
        const ContentBrowserSelection& GetSelection() const { return m_Selection; }

        /** Queue inline rename for next Content Browser draw (context menu / deferred). */
        void RequestBeginAssetRename(std::string assetPath);
        /** Returns pending path and clears it; empty if none. */
        std::string ConsumePendingAssetRenamePath();

        void RequestBeginBatchRename();
        bool ConsumePendingBatchRename();

        void RequestCreateFolderDialog(std::string parentDirectoryRel);
        bool ConsumePendingCreateFolder(std::string& outParentDirectoryRel);

        void RequestBeginFolderRename(std::string directoryRel);
        std::string ConsumePendingFolderRenamePath();

    private:
        IEditorContext* m_Context = nullptr;
        std::unique_ptr<AssetTreeModel> m_Model;
        ContentBrowserSelection m_Selection;
        std::string m_PendingAssetRenamePath;
        bool m_PendingBatchRename = false;
        bool m_PendingCreateFolder = false;
        std::string m_PendingCreateFolderParent;
        std::string m_PendingFolderRenamePath;
    };
}

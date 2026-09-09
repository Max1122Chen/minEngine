#pragma once

#include "Core.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
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

        /** Queue inline rename for next Content Browser draw (context menu / deferred). */
        void RequestBeginAssetRename(std::string assetPath);
        /** Returns pending path and clears it; empty if none. */
        std::string ConsumePendingAssetRenamePath();

    private:
        IEditorContext* m_Context = nullptr;
        std::unique_ptr<AssetTreeModel> m_Model;
        std::string m_PendingAssetRenamePath;
    };
}

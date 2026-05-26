#pragma once

#include "Core.h"
#include "Services/ContentBrowser/AssetTreeModel.h"
#include "Shell/EditorServiceModule.h"

#include <memory>

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

    private:
        IEditorContext* m_Context = nullptr;
        std::unique_ptr<AssetTreeModel> m_Model;
    };
}

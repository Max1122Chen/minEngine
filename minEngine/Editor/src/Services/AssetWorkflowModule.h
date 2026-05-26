#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"

namespace minEngine
{
    class AssetMeta;
    class IEditorContext;

    class AssetWorkflowModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "AssetWorkflow";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;

        bool OpenAsset(const AssetMeta& meta);
        void ImportAssetDialog();

    private:
        IEditorContext* m_Context = nullptr;
    };
}

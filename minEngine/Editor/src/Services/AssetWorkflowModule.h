#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"
#include "Shell/IEditorInspectorSource.h"

#include <string>

namespace minEngine
{
    class AssetMeta;
    class AssetWorkflowModule;
    class IEditorContext;

    class AssetWorkflowInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit AssetWorkflowInspectorSource(AssetWorkflowModule& owner);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        AssetWorkflowModule& m_Owner;
    };

    class AssetWorkflowModule : public EditorServiceModule
    {
    public:
        static constexpr const char* kModuleId = "AssetWorkflow";

        std::string_view GetModuleId() const override { return kModuleId; }
        void Register(IEditorContext& context) override;
        void Shutdown() override;

        bool OpenAsset(const AssetMeta& meta);
        void ImportAssetDialog();

        void SetSelectedAsset(const AssetMeta* meta);
        const AssetMeta* GetSelectedAsset() const;

        void SetContentBrowserInspectorActive(bool active);
        bool IsContentBrowserInspectorActive() const;

        IEditorInspectorSource* GetInspectorSource();
        const IEditorInspectorSource* GetInspectorSource() const;

        void DeleteSelectedAsset();

    private:
        IEditorContext* m_Context = nullptr;
        std::string m_SelectedAssetPath;
        bool m_ContentBrowserInspectorActive = false;
        AssetWorkflowInspectorSource m_InspectorSource{*this};
    };
}

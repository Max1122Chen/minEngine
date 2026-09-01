#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"
#include "Shell/IEditorInspectorSource.h"
#include "UI/Dialogs/EditorUnsavedChangesDialog.h"

#include <functional>
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

        void DrawModals();

        bool TryOpenAsset(const AssetMeta& meta);
        bool TryOpenSceneByPath(const std::string& projectRelativePath);
        void OpenSceneDialog();
        bool TryNewScene();
        bool TryCreateSceneInDirectory(std::string_view directoryRel);
        bool TryCreateMaterialInDirectory(std::string_view directoryRel);
        bool TryRequestExit(IEditorContext& context);

        bool OpenAsset(const AssetMeta& meta);
        void ImportAssetDialog(std::string_view destDirectoryRel = {});

        void SetSelectedAsset(const AssetMeta* meta);
        const AssetMeta* GetSelectedAsset() const;

        void SetContentBrowserInspectorActive(bool active);
        bool IsContentBrowserInspectorActive() const;

        IEditorInspectorSource* GetInspectorSource();
        const IEditorInspectorSource* GetInspectorSource() const;

        void DeleteSelectedAsset();

        IEditorContext* GetEditorContext() const { return m_Context; }

    private:
        enum class PendingUnsavedCheckKind
        {
            None,
            OpenScene,
            OpenMaterial,
            Exit
        };

        bool IsSceneDirty() const;
        bool IsMaterialDirty() const;
        bool SaveSceneDocument();
        bool SaveMaterialDocument();

        bool RunWithUnsavedCheck(
            const char* message,
            std::function<bool()> isDirtyCallback,
            std::function<bool()> saveCallback,
            std::function<void()> proceedCallback);

        void HandleUnsavedDialogChoice(UnsavedChangesChoice choice);
        void RefreshContentBrowser();

        IEditorContext* m_Context = nullptr;
        std::string m_SelectedAssetPath;
        bool m_ContentBrowserInspectorActive = false;
        AssetWorkflowInspectorSource m_InspectorSource{*this};

        EditorUnsavedChangesDialog m_UnsavedDialog;
        std::function<void()> m_PendingProceed;
        std::function<bool()> m_PendingSave;
        PendingUnsavedCheckKind m_PendingCheckKind = PendingUnsavedCheckKind::None;
    };
}

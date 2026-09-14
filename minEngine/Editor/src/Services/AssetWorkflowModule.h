#pragma once

#include "Core.h"
#include "Shell/EditorServiceModule.h"
#include "Shell/IEditorInspectorSource.h"
#include "UI/Dialogs/EditorImportDialog.h"
#include "UI/Dialogs/EditorUnsavedChangesDialog.h"
#include "Runtime/Resource/AssetManager.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

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

        /** Navigate CB to the asset's folder, select it, and ensure the CB window is open. */
        bool RevealAssetInContentBrowser(std::string_view assetPath);

        void SetContentBrowserInspectorActive(bool active);
        bool IsContentBrowserInspectorActive() const;

        IEditorInspectorSource* GetInspectorSource();
        const IEditorInspectorSource* GetInspectorSource() const;

        void DeleteSelectedAsset();
        bool TryReimportSelectedAsset();

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
        bool IsAnimationGraphDirty() const;
        bool SaveSceneDocument();
        bool SaveMaterialDocument();
        bool SaveAnimationGraphDocument();

        bool RunWithUnsavedCheck(
            const char* message,
            std::function<bool()> isDirtyCallback,
            std::function<bool()> saveCallback,
            std::function<void()> proceedCallback);

        void HandleUnsavedDialogChoice(UnsavedChangesChoice choice);
        void HandleImportDialogAction(EditorImportDialogAction action);
        void RefreshContentBrowser();

        static std::vector<const ImportProductDescriptor*> CollectCompatibleImportProducts(
            const std::filesystem::path& sourcePath);

        IEditorContext* m_Context = nullptr;
        std::string m_SelectedAssetPath;
        bool m_ContentBrowserInspectorActive = false;
        AssetWorkflowInspectorSource m_InspectorSource{*this};

        EditorUnsavedChangesDialog m_UnsavedDialog;
        EditorImportDialog m_ImportDialog;
        std::function<void()> m_PendingProceed;
        std::function<bool()> m_PendingSave;
        PendingUnsavedCheckKind m_PendingCheckKind = PendingUnsavedCheckKind::None;
    };
}

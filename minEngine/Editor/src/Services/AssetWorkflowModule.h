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

        friend class AssetWorkflowInspectorSource;

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
        bool TryCreateMaterialInstanceInDirectory(std::string_view directoryRel);
        bool TryCreateSpriteInDirectory(std::string_view directoryRel);
        bool TryCreateSpriteAtlasInDirectory(std::string_view directoryRel);
        bool TryCreateSpriteAtlasFromSelection(const std::vector<const AssetMeta*>& selectedSprites);
        bool TryCreateDataAssetInDirectory(std::string_view directoryRel);
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

        void DeleteSelectedAsset(bool bUnpackOpenPrefabInstanceRefs = false);
        /** Queue delete confirm modal for selected asset paths (best-effort warnings). */
        void RequestConfirmDeleteAssets(
            std::vector<std::string> paths,
            bool bUnpackOpenPrefabInstanceRefs = false);
        void RequestConfirmDeleteFolder(std::string directoryRel);
        bool TryBatchRenameAssets(
            const std::vector<std::string>& orderedPaths,
            std::string_view baseStem,
            std::string& outError);
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
        bool IsSpriteDirty() const;
        bool IsSpriteAtlasDirty() const;
        bool IsPrefabDirty() const;
        bool SaveSceneDocument();
        bool SaveMaterialDocument();
        bool SaveAnimationGraphDocument();
        bool SaveSpriteDocument();
        bool SaveSpriteAtlasDocument();
        bool SavePrefabDocument();
        void DrawMaterialInstanceInspector(const AssetMeta& meta);
        void DrawDataAssetInspector(const AssetMeta& meta);

        bool RunWithUnsavedCheck(
            const char* message,
            std::function<bool()> isDirtyCallback,
            std::function<bool()> saveCallback,
            std::function<void()> proceedCallback);

        void HandleUnsavedDialogChoice(UnsavedChangesChoice choice);
        void HandleImportDialogAction(EditorImportDialogAction action);
        void DrawDeleteConfirmModal();
        void ExecutePendingDelete();
        std::vector<std::string> CollectDeleteWarnings(const std::vector<std::string>& paths);
        void RefreshContentBrowser();

        static std::vector<const ImportProductDescriptor*> CollectCompatibleImportProducts(
            const std::filesystem::path& sourcePath);

        IEditorContext* m_Context = nullptr;
        std::string m_SelectedAssetPath;
        bool m_ContentBrowserInspectorActive = false;
        AssetWorkflowInspectorSource m_InspectorSource{*this};

        EditorUnsavedChangesDialog m_UnsavedDialog;
        EditorImportDialog m_ImportDialog;

        bool m_ShowDeleteConfirm = false;
        bool m_PendingDeleteUnpack = false;
        bool m_PendingDeleteIsFolder = false;
        std::vector<std::string> m_PendingDeletePaths;
        std::string m_PendingDeleteFolder;
        std::vector<std::string> m_DeleteWarningLines;
        std::function<void()> m_PendingProceed;
        std::function<bool()> m_PendingSave;
        PendingUnsavedCheckKind m_PendingCheckKind = PendingUnsavedCheckKind::None;
        char m_NewMicOverrideName[64] = {};
        float m_NewMicOverrideValue = 0.0f;
    };
}

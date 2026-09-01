#include "Services/AssetWorkflowModule.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Services/Inspector/InspectorModule.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "UI/Inspector/InspectorPreviewPresenter.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"

#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"

#include <filesystem>

namespace minEngine
{
    namespace
    {
        std::string TryMakeProjectRelativeAssetPath(const std::filesystem::path& absolutePath)
        {
            const std::filesystem::path contentRoot = PathRegistry::Get().GetProjectContentRoot();
            if (contentRoot.empty())
            {
                return std::string();
            }

            std::error_code errorCode;
            const std::filesystem::path normalizedAbsolute = absolutePath.lexically_normal();
            const std::filesystem::path normalizedRoot = contentRoot.lexically_normal();
            const std::filesystem::path relative =
                std::filesystem::relative(normalizedAbsolute, normalizedRoot, errorCode);
            if (errorCode)
            {
                return std::string();
            }

            return relative.generic_string();
        }

        void RefreshContentBrowserModel(IEditorContext& editor)
        {
            AssetTreeModel& model = editor.GetContentBrowser().GetModel();
            model.RebuildDirectoryTree();
            model.RebuildCurrentDirectoryAssetList();
        }
    }

    AssetWorkflowInspectorSource::AssetWorkflowInspectorSource(AssetWorkflowModule& owner)
        : m_Owner(owner)
    {
    }

    bool AssetWorkflowInspectorSource::HasInspectableSelection() const
    {
        return m_Owner.GetSelectedAsset() != nullptr;
    }

    void AssetWorkflowInspectorSource::DrawInspector()
    {
        IEditorContext* context = m_Owner.GetEditorContext();
        if (context == nullptr)
        {
            ImGui::Begin("Inspector");
            ImGui::TextUnformatted("No editor context.");
            ImGui::End();
            return;
        }

        if (!EditorWindowTypography::BeginPanel(*context, "Inspector"))
        {
            return;
        }

        EditorTypographyScope bodyTypography(
            context->GetEditorAppearance(),
            EditorTypographyRole::Body);

        const AssetMeta* selected = m_Owner.GetSelectedAsset();
        if (selected == nullptr)
        {
            ImGui::TextUnformatted("No asset selected.");
            ImGui::End();
            return;
        }

        InspectorPreviewPresenter::DrawSquarePreviewSlot(
            *context,
            context->GetInspectorModule().GetThumbnailService());

        ImGui::Text("Name: %s", selected->AssetName.c_str());
        ImGui::Text("Path: %s", selected->AssetPath.c_str());
        ImGui::Text("Type: %s", selected->AssetType.c_str());
        ImGui::Text("Guid: %s", selected->Guid.ToString().c_str());
        ImGui::End();
    }

    void AssetWorkflowModule::Register(IEditorContext& context)
    {
        m_Context = &context;
    }

    void AssetWorkflowModule::Shutdown()
    {
        m_SelectedAssetPath.clear();
        m_ContentBrowserInspectorActive = false;
        m_PendingProceed = nullptr;
        m_PendingSave = nullptr;
        m_PendingCheckKind = PendingUnsavedCheckKind::None;
        m_UnsavedDialog.Close();
        m_Context = nullptr;
    }

    void AssetWorkflowModule::DrawModals()
    {
        const UnsavedChangesChoice choice = m_UnsavedDialog.Draw();
        if (choice != UnsavedChangesChoice::None)
        {
            HandleUnsavedDialogChoice(choice);
        }
    }

    bool AssetWorkflowModule::IsSceneDirty() const
    {
        const SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        return sceneEditor != nullptr && sceneEditor->IsSceneDirty();
    }

    bool AssetWorkflowModule::IsMaterialDirty() const
    {
        const MaterialEditor* materialEditor = GetMaterialEditor(m_Context);
        return materialEditor != nullptr && materialEditor->GetSession().Dirty;
    }

    bool AssetWorkflowModule::SaveSceneDocument()
    {
        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (sceneEditor == nullptr)
        {
            return false;
        }

        return sceneEditor->SaveCurrentScene(*m_Context);
    }

    bool AssetWorkflowModule::SaveMaterialDocument()
    {
        MaterialEditor* materialEditor = GetMaterialEditor(m_Context);
        if (materialEditor == nullptr)
        {
            return false;
        }

        return materialEditor->SaveActiveMaterial();
    }

    bool AssetWorkflowModule::RunWithUnsavedCheck(
        const char* message,
        std::function<bool()> isDirtyCallback,
        std::function<bool()> saveCallback,
        std::function<void()> proceedCallback)
    {
        if (!isDirtyCallback || !proceedCallback)
        {
            return false;
        }

        if (!isDirtyCallback())
        {
            proceedCallback();
            return true;
        }

        if (m_UnsavedDialog.IsOpen())
        {
            return false;
        }

        m_PendingProceed = std::move(proceedCallback);
        m_PendingSave = std::move(saveCallback);
        m_UnsavedDialog.Open(message);
        return false;
    }

    void AssetWorkflowModule::HandleUnsavedDialogChoice(UnsavedChangesChoice choice)
    {
        if (choice == UnsavedChangesChoice::Cancel || choice == UnsavedChangesChoice::None)
        {
            m_PendingProceed = nullptr;
            m_PendingSave = nullptr;
            m_PendingCheckKind = PendingUnsavedCheckKind::None;
            return;
        }

        if (choice == UnsavedChangesChoice::Save)
        {
            if (!m_PendingSave || !m_PendingSave())
            {
                ME_CORE_WARN("AssetWorkflow: save failed; workflow action cancelled.");
                m_PendingProceed = nullptr;
                m_PendingSave = nullptr;
                m_PendingCheckKind = PendingUnsavedCheckKind::None;
                return;
            }
        }

        if (m_PendingProceed)
        {
            m_PendingProceed();
        }

        m_PendingProceed = nullptr;
        m_PendingSave = nullptr;
        m_PendingCheckKind = PendingUnsavedCheckKind::None;
    }

    void AssetWorkflowModule::RefreshContentBrowser()
    {
        if (m_Context != nullptr)
        {
            RefreshContentBrowserModel(*m_Context);
        }
    }

    bool AssetWorkflowModule::OpenAsset(const AssetMeta& meta)
    {
        if (!m_Context)
        {
            return false;
        }

        if (EditorSubModule* materialModule = m_Context->FindSubModule(MaterialEditor::kModuleId))
        {
            if (materialModule->CanOpenAsset(meta) && materialModule->OpenAsset(meta))
            {
                m_Context->ActivateSubModule(MaterialEditor::kModuleId);
                return true;
            }
        }

        if (EditorSubModule* sceneModule = m_Context->FindSubModule(SceneEditor::kModuleId))
        {
            if (sceneModule->CanOpenAsset(meta) && sceneModule->OpenAsset(meta))
            {
                m_Context->ActivateSubModule(SceneEditor::kModuleId);
                return true;
            }
        }

        return false;
    }

    bool AssetWorkflowModule::TryOpenAsset(const AssetMeta& meta)
    {
        if (!m_Context)
        {
            return false;
        }

        const bool openingMaterial = meta.AssetType == "Material";
        const bool openingScene = meta.AssetType == "Scene";
        if (!openingMaterial && !openingScene)
        {
            ME_CORE_WARN(
                "AssetWorkflow: unsupported asset type '{}' for '{}'.",
                meta.AssetType,
                meta.AssetPath);
            return false;
        }

        const char* message = openingMaterial
            ? "Save changes to the current material before opening another asset?"
            : "Save changes to the current scene before opening another scene?";

        auto proceed = [this, meta]()
        {
            if (!OpenAsset(meta))
            {
                ME_CORE_WARN("AssetWorkflow: failed to open asset '{}'.", meta.AssetPath);
                return;
            }

            SetSelectedAsset(&meta);
        };

        if (openingMaterial)
        {
            return RunWithUnsavedCheck(
                message,
                [this]() { return IsMaterialDirty(); },
                [this]() { return SaveMaterialDocument(); },
                std::move(proceed));
        }

        return RunWithUnsavedCheck(
            message,
            [this]() { return IsSceneDirty(); },
            [this]() { return SaveSceneDocument(); },
            std::move(proceed));
    }

    bool AssetWorkflowModule::TryOpenSceneByPath(const std::string& projectRelativePath)
    {
        if (!AssetManager::HasInstance())
        {
            return false;
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(projectRelativePath);
        if (meta == nullptr)
        {
            ME_CORE_WARN("AssetWorkflow: scene asset '{}' is not registered.", projectRelativePath);
            return false;
        }

        return TryOpenAsset(*meta);
    }

    void AssetWorkflowModule::OpenSceneDialog()
    {
        if (!m_Context)
        {
            return;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path projectContentRoot = paths.GetProjectContentRoot();
        if (projectContentRoot.empty())
        {
            ME_CORE_ERROR("OpenSceneDialog: ProjectContentRoot is not set.");
            return;
        }

        FileDialogRequest request;
        request.Title = "Open Scene";
        request.Filters = AssetTypeRegistry::Get().BuildFileDialogFiltersForAssetType("Scene");
        request.bAllowMultiple = false;
        request.InitialDirectory = projectContentRoot;

        const FileDialogResult dialogResult = m_Context->GetFileDialogService().OpenFiles(request);
        if (dialogResult.bCancelled || dialogResult.Paths.empty())
        {
            return;
        }

        const std::string projectRelativePath = TryMakeProjectRelativeAssetPath(dialogResult.Paths.front());
        if (projectRelativePath.empty())
        {
            ME_CORE_ERROR(
                "OpenSceneDialog: selected file '{}' is outside project content root.",
                dialogResult.Paths.front().string());
            return;
        }

        TryOpenSceneByPath(projectRelativePath);
    }

    bool AssetWorkflowModule::TryNewScene()
    {
        if (!m_Context)
        {
            return false;
        }

        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (sceneEditor == nullptr)
        {
            return false;
        }

        return RunWithUnsavedCheck(
            "Save changes to the current scene before creating a new scene?",
            [this]() { return IsSceneDirty(); },
            [this]() { return SaveSceneDocument(); },
            [this, sceneEditor]()
            {
                if (!sceneEditor->CreateNewSceneDocument(*m_Context))
                {
                    ME_CORE_WARN("AssetWorkflow: failed to create a new scene document.");
                    return;
                }

                m_Context->ActivateSubModule(SceneEditor::kModuleId);
            });
    }

    bool AssetWorkflowModule::TryCreateSceneInDirectory(std::string_view directoryRel)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        const std::string directory = std::string(directoryRel);
        return RunWithUnsavedCheck(
            "Save changes to the current scene before creating a new scene?",
            [this]() { return IsSceneDirty(); },
            [this]() { return SaveSceneDocument(); },
            [this, directory]()
            {
                std::shared_ptr<Scene> createdScene =
                    AssetManager::Get().CreateAsset<Scene>("NewScene", directory);
                if (!createdScene)
                {
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(createdScene->GetGuid());
                if (meta == nullptr)
                {
                    ME_CORE_WARN("AssetWorkflow: created scene has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_CORE_WARN("AssetWorkflow: failed to open created scene '{}'.", meta->AssetPath);
                    return;
                }

                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryCreateMaterialInDirectory(std::string_view directoryRel)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        const std::string directory = std::string(directoryRel);
        return RunWithUnsavedCheck(
            "Save changes to the current material before creating a new material?",
            [this]() { return IsMaterialDirty(); },
            [this]() { return SaveMaterialDocument(); },
            [this, directory]()
            {
                std::shared_ptr<Material> createdMaterial =
                    AssetManager::Get().CreateAsset<Material>("NewMaterial", directory);
                if (!createdMaterial)
                {
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(createdMaterial->GetGuid());
                if (meta == nullptr)
                {
                    ME_CORE_WARN("AssetWorkflow: created material has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_CORE_WARN("AssetWorkflow: failed to open created material '{}'.", meta->AssetPath);
                    return;
                }

                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryRequestExit(IEditorContext& context)
    {
        const bool sceneDirty = IsSceneDirty();
        const bool materialDirty = IsMaterialDirty();
        if (!sceneDirty && !materialDirty)
        {
            return true;
        }

        const char* message = sceneDirty && materialDirty
            ? "Save scene and material changes before exiting?"
            : sceneDirty
                ? "Save scene changes before exiting?"
                : "Save material changes before exiting?";

        return RunWithUnsavedCheck(
            message,
            [sceneDirty, materialDirty]() { return sceneDirty || materialDirty; },
            [this]()
            {
                bool saved = true;
                if (IsSceneDirty())
                {
                    saved = SaveSceneDocument() && saved;
                }
                if (IsMaterialDirty())
                {
                    saved = SaveMaterialDocument() && saved;
                }
                return saved;
            },
            [&context]() { context.ConfirmExit(); });
    }

    void AssetWorkflowModule::ImportAssetDialog(std::string_view destDirectoryRel)
    {
        if (!m_Context)
        {
            return;
        }

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path projectContentRoot = paths.GetProjectContentRoot();
        if (projectContentRoot.empty())
        {
            ME_CORE_ERROR("ImportAssetDialog: ProjectContentRoot is not set.");
            return;
        }

        FileDialogRequest request;
        request.Title = "Import Assets";
        request.Filters = AssetTypeRegistry::Get().BuildFileDialogFilters();
        request.bAllowMultiple = true;
        request.InitialDirectory = projectContentRoot;

        const FileDialogResult dialogResult = m_Context->GetFileDialogService().OpenFiles(request);
        if (dialogResult.bCancelled || dialogResult.Paths.empty())
        {
            return;
        }

        std::filesystem::path destDirectory = projectContentRoot;
        if (!destDirectoryRel.empty())
        {
            destDirectory /= std::filesystem::path(destDirectoryRel);
        }
        std::error_code createError;
        std::filesystem::create_directories(destDirectory, createError);
        if (createError)
        {
            ME_CORE_ERROR(
                "ImportAssetDialog: failed to create destination directory '{}': {}",
                destDirectory.string(),
                createError.message());
            return;
        }

        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(destDirectory);

        int successCount = 0;
        int failCount = 0;

        AssetManager::AssetRegistryBroadcastBatchScope batchScope;

        for (const std::filesystem::path& sourcePath : dialogResult.Paths)
        {
            const ImportAssetResult importResult =
                AssetManager::Get().ImportAsset(sourcePath, destDirectory);
            if (!importResult.bSuccess)
            {
                ++failCount;
                ME_CORE_ERROR(
                    "ImportAssetDialog: failed to import '{}': {}",
                    sourcePath.string(),
                    importResult.ErrorMessage);
                continue;
            }

            ++successCount;
            ME_CORE_INFO(
                "ImportAssetDialog: imported '{}' as '{}'",
                sourcePath.string(),
                importResult.Meta.AssetPath);
        }

        ME_CORE_INFO("ImportAssetDialog: {} succeeded, {} failed.", successCount, failCount);
    }

    void AssetWorkflowModule::SetSelectedAsset(const AssetMeta* meta)
    {
        if (meta == nullptr)
        {
            if (m_SelectedAssetPath.empty())
            {
                return;
            }

            m_SelectedAssetPath.clear();
            if (m_Context)
            {
                m_Context->GetInspectorModule().ClearInspectionTarget();
            }
            return;
        }

        if (meta->AssetPath == m_SelectedAssetPath)
        {
            return;
        }

        m_SelectedAssetPath = meta->AssetPath;

        if (m_Context)
        {
            m_Context->GetInspectorModule().SetInspectionTarget(meta);
        }
    }

    const AssetMeta* AssetWorkflowModule::GetSelectedAsset() const
    {
        if (m_SelectedAssetPath.empty() || !AssetManager::HasInstance())
        {
            return nullptr;
        }

        return AssetManager::Get().FindAssetMetaByPath(m_SelectedAssetPath);
    }

    void AssetWorkflowModule::SetContentBrowserInspectorActive(bool active)
    {
        m_ContentBrowserInspectorActive = active;
    }

    bool AssetWorkflowModule::IsContentBrowserInspectorActive() const
    {
        return m_ContentBrowserInspectorActive;
    }

    IEditorInspectorSource* AssetWorkflowModule::GetInspectorSource()
    {
        return &m_InspectorSource;
    }

    const IEditorInspectorSource* AssetWorkflowModule::GetInspectorSource() const
    {
        return &m_InspectorSource;
    }

    void AssetWorkflowModule::DeleteSelectedAsset()
    {
        const AssetMeta* selected = GetSelectedAsset();
        if (selected == nullptr)
        {
            return;
        }

        const std::string assetPath = selected->AssetPath;

        std::string errorMessage;
        if (!AssetManager::Get().DeleteAsset(assetPath, errorMessage))
        {
            ME_CORE_ERROR("DeleteSelectedAsset failed for '{}': {}", assetPath, errorMessage);
            return;
        }

        m_SelectedAssetPath.clear();
        ME_CORE_INFO("DeleteSelectedAsset: removed '{}'", assetPath);
    }
}

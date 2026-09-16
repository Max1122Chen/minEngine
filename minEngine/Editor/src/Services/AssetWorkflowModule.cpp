#include "Services/AssetWorkflowModule.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Services/Inspector/InspectorModule.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/IEditorContext.h"
#include "Shell/Document/EditorDocumentHost.h"
#include "EditorGUIManager.h"
#include "UI/EditorWindows/EditorWindow.h"
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
        if (!selected->SourcePath.empty())
        {
            ImGui::Text("Source: %s", selected->SourcePath.c_str());
            if (ImGui::Button("Reimport"))
            {
                m_Owner.TryReimportSelectedAsset();
            }
        }
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
        m_ImportDialog.Close();
        m_Context = nullptr;
    }

    void AssetWorkflowModule::DrawModals()
    {
        const UnsavedChangesChoice choice = m_UnsavedDialog.Draw();
        if (choice != UnsavedChangesChoice::None)
        {
            HandleUnsavedDialogChoice(choice);
        }

        const EditorImportDialogAction importAction = m_ImportDialog.Draw();
        if (importAction != EditorImportDialogAction::None)
        {
            HandleImportDialogAction(importAction);
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

    bool AssetWorkflowModule::IsAnimationGraphDirty() const
    {
        const AnimationGraphEditor* animGraphEditor = GetAnimationGraphEditor(m_Context);
        return animGraphEditor != nullptr && animGraphEditor->GetSession().Dirty;
    }

    bool AssetWorkflowModule::IsPrefabDirty() const
    {
        const SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (sceneEditor == nullptr || !sceneEditor->IsEditingPrefabStage())
        {
            return false;
        }

        return sceneEditor->GetPrefabStages().IsDirty(sceneEditor->GetPrefabStages().GetActiveAssetKey());
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

    bool AssetWorkflowModule::SaveAnimationGraphDocument()
    {
        AnimationGraphEditor* animGraphEditor = GetAnimationGraphEditor(m_Context);
        if (animGraphEditor == nullptr)
        {
            return false;
        }

        return animGraphEditor->SaveActiveGraph();
    }

    bool AssetWorkflowModule::SavePrefabDocument()
    {
        SceneEditor* sceneEditor = GetSceneEditor(m_Context);
        if (sceneEditor == nullptr || m_Context == nullptr)
        {
            return false;
        }

        std::string error;
        if (!sceneEditor->GetPrefabStages().SaveActive(*m_Context, &error))
        {
            ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to save Prefab: {}", error);
            return false;
        }

        return true;
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
                ME_LOG(LogEditor, Warn, "AssetWorkflow: save failed; workflow action cancelled.");
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

        return m_Context->GetDocumentHost().OpenOrFocus(meta);
    }

    bool AssetWorkflowModule::TryOpenAsset(const AssetMeta& meta)
    {
        if (!m_Context)
        {
            return false;
        }

        const bool openingMaterial = meta.AssetType == "Material";
        const bool openingScene = meta.AssetType == "Scene";
        const bool openingAnimationGraph = meta.AssetType == "AnimationGraph";
        const bool openingPrefab = meta.AssetType == "Prefab";
        if (!openingMaterial && !openingScene && !openingAnimationGraph && !openingPrefab)
        {
            ME_LOG(LogEditor, Warn, 
                "AssetWorkflow: unsupported asset type '{}' for '{}'.",
                meta.AssetType,
                meta.AssetPath);
            return false;
        }

        auto proceed = [this, meta]()
        {
            if (!OpenAsset(meta))
            {
                ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open asset '{}'.", meta.AssetPath);
                return;
            }

            SetSelectedAsset(&meta);
        };

        if (openingMaterial)
        {
            return RunWithUnsavedCheck(
                "Save changes to the current material before opening another asset?",
                [this]() { return IsMaterialDirty(); },
                [this]() { return SaveMaterialDocument(); },
                std::move(proceed));
        }

        if (openingAnimationGraph)
        {
            return RunWithUnsavedCheck(
                "Save changes to the current animation graph before opening another asset?",
                [this]() { return IsAnimationGraphDirty(); },
                [this]() { return SaveAnimationGraphDocument(); },
                std::move(proceed));
        }

        if (openingPrefab)
        {
            return RunWithUnsavedCheck(
                "Save changes to the current Prefab before opening another asset?",
                [this]() { return IsPrefabDirty(); },
                [this]() { return SavePrefabDocument(); },
                std::move(proceed));
        }

        return RunWithUnsavedCheck(
            "Save changes to the current scene before opening another scene?",
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
            ME_LOG(LogEditor, Warn, "AssetWorkflow: scene asset '{}' is not registered.", projectRelativePath);
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
            ME_LOG(LogEditor, Error, "OpenSceneDialog: ProjectContentRoot is not set.");
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
            ME_LOG(LogEditor, Error, 
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
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to create a new scene document.");
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
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: created scene has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open created scene '{}'.", meta->AssetPath);
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
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: created material has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open created material '{}'.", meta->AssetPath);
                    return;
                }

                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryRequestExit(IEditorContext& context)
    {
        const bool sceneDirty = IsSceneDirty();
        const bool materialDirty = IsMaterialDirty();
        const bool animGraphDirty = IsAnimationGraphDirty();
        if (!sceneDirty && !materialDirty && !animGraphDirty)
        {
            return true;
        }

        const char* message = "Save unsaved document changes before exiting?";
        if (sceneDirty && !materialDirty && !animGraphDirty)
        {
            message = "Save scene changes before exiting?";
        }
        else if (!sceneDirty && materialDirty && !animGraphDirty)
        {
            message = "Save material changes before exiting?";
        }
        else if (!sceneDirty && !materialDirty && animGraphDirty)
        {
            message = "Save animation graph changes before exiting?";
        }

        return RunWithUnsavedCheck(
            message,
            [sceneDirty, materialDirty, animGraphDirty]()
            { return sceneDirty || materialDirty || animGraphDirty; },
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
                if (IsAnimationGraphDirty())
                {
                    saved = SaveAnimationGraphDocument() && saved;
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
            ME_LOG(LogEditor, Error, "ImportAssetDialog: ProjectContentRoot is not set.");
            return;
        }

        FileDialogRequest request;
        request.Title = "Import Assets";
        request.Filters = AssetTypeRegistry::Get().BuildFileDialogFilters();
        const std::vector<FileDialogFilter> importSourceFilters =
            AssetTypeRegistry::Get().BuildImportSourceFileDialogFilters();
        request.Filters.insert(
            request.Filters.begin(),
            importSourceFilters.begin(),
            importSourceFilters.end());
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
            ME_LOG(LogEditor, Error, 
                "ImportAssetDialog: failed to create destination directory '{}': {}",
                destDirectory.string(),
                createError.message());
            return;
        }

        EditorFilesystemMutationPass::NoteMutatedAbsolutePath(destDirectory);

        std::vector<std::filesystem::path> autoImportPaths;
        std::vector<std::string> autoImportProductIds;
        std::vector<std::filesystem::path> pendingChoicePaths;
        autoImportPaths.reserve(dialogResult.Paths.size());
        autoImportProductIds.reserve(dialogResult.Paths.size());
        pendingChoicePaths.reserve(dialogResult.Paths.size());

        for (const std::filesystem::path& sourcePath : dialogResult.Paths)
        {
            const std::vector<const ImportProductDescriptor*> compatible =
                CollectCompatibleImportProducts(sourcePath);
            if (compatible.empty())
            {
                ME_LOG(LogEditor, Error, 
                    "ImportAssetDialog: no import product accepts '{}'",
                    sourcePath.string());
                continue;
            }

            if (compatible.size() == 1 && !compatible[0]->bNeedsSkeletonPicker)
            {
                autoImportPaths.push_back(sourcePath);
                autoImportProductIds.push_back(compatible[0]->ProductId);
            }
            else
            {
                pendingChoicePaths.push_back(sourcePath);
            }
        }

        int successCount = 0;
        int failCount = 0;

        {
            AssetManager::AssetRegistryBroadcastBatchScope batchScope;
            for (size_t index = 0; index < autoImportPaths.size(); ++index)
            {
                ImportRequest importRequest;
                importRequest.SourcePath = autoImportPaths[index];
                importRequest.DestDirectory = destDirectory;
                importRequest.ProductId = autoImportProductIds[index];

                const ImportResult importResult = AssetManager::Get().Import(importRequest);
                if (!importResult.bSuccess)
                {
                    ++failCount;
                    ME_LOG(LogEditor, Error, 
                        "ImportAssetDialog: failed to import '{}': {}",
                        autoImportPaths[index].string(),
                        importResult.ErrorMessage);
                    continue;
                }

                ++successCount;
                const std::string createdPath = importResult.Created.empty()
                    ? std::string()
                    : importResult.Created.front().AssetPath;
                ME_LOG(LogEditor, Info, 
                    "ImportAssetDialog: imported '{}' as '{}' (product '{}')",
                    autoImportPaths[index].string(),
                    createdPath,
                    autoImportProductIds[index]);
            }
        }

        if (!pendingChoicePaths.empty())
        {
            m_ImportDialog.Open(std::move(pendingChoicePaths), destDirectory);
        }

        ME_LOG(LogEditor, Info, 
            "ImportAssetDialog: {} auto-imported succeeded, {} failed; {} pending product choice.",
            successCount,
            failCount,
            m_ImportDialog.IsOpen() ? m_ImportDialog.GetSourcePaths().size() : 0);
    }

    std::vector<const ImportProductDescriptor*> AssetWorkflowModule::CollectCompatibleImportProducts(
        const std::filesystem::path& sourcePath)
    {
        std::vector<const ImportProductDescriptor*> compatible;
        const std::string extension = sourcePath.extension().string();
        for (const ImportProductDescriptor& product : AssetManager::Get().GetImportProducts())
        {
            if (product.AcceptsSourceExtension != nullptr
                && product.AcceptsSourceExtension(extension))
            {
                compatible.push_back(&product);
            }
        }

        return compatible;
    }

    void AssetWorkflowModule::HandleImportDialogAction(EditorImportDialogAction action)
    {
        if (action == EditorImportDialogAction::Cancel || action == EditorImportDialogAction::None)
        {
            m_ImportDialog.Close();
            return;
        }

        const std::string productId = m_ImportDialog.GetSelectedProductId();
        const std::string skeletonAssetPath = m_ImportDialog.GetSkeletonAssetPath();
        const std::filesystem::path destDirectory = m_ImportDialog.GetDestDirectory();
        const std::vector<std::filesystem::path> sourcePaths = m_ImportDialog.GetSourcePaths();
        m_ImportDialog.Close();

        if (productId.empty() || sourcePaths.empty())
        {
            return;
        }

        int successCount = 0;
        int failCount = 0;

        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        for (const std::filesystem::path& sourcePath : sourcePaths)
        {
            ImportRequest importRequest;
            importRequest.SourcePath = sourcePath;
            importRequest.DestDirectory = destDirectory;
            importRequest.ProductId = productId;
            importRequest.SkeletonAssetPath = skeletonAssetPath;

            const ImportResult importResult = AssetManager::Get().Import(importRequest);
            if (!importResult.bSuccess)
            {
                ++failCount;
                ME_LOG(LogEditor, Error, 
                    "ImportAssetDialog: failed to import '{}' as '{}': {}",
                    sourcePath.string(),
                    productId,
                    importResult.ErrorMessage);
                continue;
            }

            ++successCount;
            const std::string createdPath = importResult.Created.empty()
                ? std::string()
                : importResult.Created.front().AssetPath;
            ME_LOG(LogEditor, Info, 
                "ImportAssetDialog: '{}' → '{}' (product '{}')",
                sourcePath.string(),
                createdPath,
                productId);
        }

        ME_LOG(LogEditor, Info, 
            "ImportAssetDialog product import: {} succeeded, {} failed.",
            successCount,
            failCount);

        RefreshContentBrowser();
    }

    bool AssetWorkflowModule::TryReimportSelectedAsset()
    {
        const AssetMeta* selected = GetSelectedAsset();
        if (selected == nullptr)
        {
            return false;
        }

        std::string errorMessage;
        if (!AssetManager::Get().Reimport(selected->AssetPath, errorMessage))
        {
            ME_LOG(LogEditor, Error, 
                "Reimport failed for '{}': {}",
                selected->AssetPath,
                errorMessage);
            return false;
        }

        ME_LOG(LogEditor, Info, "Reimported '{}'", selected->AssetPath);
        RefreshContentBrowser();
        return true;
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

    bool AssetWorkflowModule::RevealAssetInContentBrowser(std::string_view assetPath)
    {
        if (!m_Context || assetPath.empty() || !AssetManager::HasInstance())
        {
            return false;
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(std::string(assetPath));
        if (meta == nullptr)
        {
            ME_LOG(LogEditor, Warn, "RevealInContentBrowser: asset not found '{}'.", assetPath);
            return false;
        }

        const std::filesystem::path assetRel(meta->AssetPath);
        const std::string parentRel = assetRel.parent_path().lexically_normal().generic_string();
        const std::string directoryRel = (parentRel == "." || parentRel == "..") ? std::string() : parentRel;

        AssetTreeModel& model = m_Context->GetContentBrowser().GetModel();
        model.SetCurrentDirectory(directoryRel);
        SetSelectedAsset(meta);

        if (EditorWindow* browserWindow = m_Context->GetGUIManager().FindWindow("ContentBrowser"))
        {
            browserWindow->SetOpen(true);
        }

        return true;
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

    void AssetWorkflowModule::DeleteSelectedAsset(bool bUnpackOpenPrefabInstanceRefs)
    {
        const AssetMeta* selected = GetSelectedAsset();
        if (selected == nullptr)
        {
            return;
        }

        const std::string assetPath = selected->AssetPath;
        const std::string assetType = selected->AssetType;

        if (assetType == "Prefab" && m_Context != nullptr)
        {
            if (SceneEditor* sceneEditor = GetSceneEditor(m_Context))
            {
                if (sceneEditor->GetPrefabStages().HasStage(assetPath))
                {
                    ME_LOG(
                        LogEditor,
                        Error,
                        "DeleteSelectedAsset: close Prefab Stage for '{}' before deleting.",
                        assetPath);
                    return;
                }
            }
        }

        std::string errorMessage;
        if (!AssetManager::Get().DeleteAsset(assetPath, errorMessage, bUnpackOpenPrefabInstanceRefs))
        {
            ME_LOG(LogEditor, Error, "DeleteSelectedAsset failed for '{}': {}", assetPath, errorMessage);
            return;
        }

        m_SelectedAssetPath.clear();
        ME_LOG(
            LogEditor,
            Info,
            "DeleteSelectedAsset: removed '{}'{}",
            assetPath,
            bUnpackOpenPrefabInstanceRefs ? " (unpacked open instances)" : "");
    }
}

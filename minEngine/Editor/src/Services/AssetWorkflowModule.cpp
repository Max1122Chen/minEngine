#include "Services/AssetWorkflowModule.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "SubEditor/Sprite/SpriteEditor.h"
#include "SubEditor/SpriteAtlas/SpriteAtlasEditor.h"
#include "Services/Asset/AssetEditorServices.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Services/ContentBrowser/ContentBrowserSelection.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Services/Inspector/InspectorModule.h"
#include "Services/Sprite/SpriteEditorServices.h"
#include "Shell/EditorCommandStack.h"
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
#include "Runtime/Function/Render/MaterialInstance.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"

#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"

#include <algorithm>
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
        if (selected->AssetType == "MaterialInstance")
        {
            m_Owner.DrawMaterialInstanceInspector(*selected);
        }
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

        DrawDeleteConfirmModal();
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

    bool AssetWorkflowModule::IsSpriteDirty() const
    {
        const SpriteEditor* spriteEditor = GetSpriteEditor(m_Context);
        return spriteEditor != nullptr && spriteEditor->GetSession().Dirty;
    }

    bool AssetWorkflowModule::IsSpriteAtlasDirty() const
    {
        const SpriteAtlasEditor* atlasEditor = GetSpriteAtlasEditor(m_Context);
        return atlasEditor != nullptr && atlasEditor->GetSession().Dirty;
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

    bool AssetWorkflowModule::SaveSpriteDocument()
    {
        SpriteEditor* spriteEditor = GetSpriteEditor(m_Context);
        if (spriteEditor == nullptr)
        {
            return false;
        }

        return spriteEditor->SaveActiveSprite();
    }

    bool AssetWorkflowModule::SaveSpriteAtlasDocument()
    {
        SpriteAtlasEditor* atlasEditor = GetSpriteAtlasEditor(m_Context);
        if (atlasEditor == nullptr)
        {
            return false;
        }

        return atlasEditor->SaveActiveAtlas();
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

        if (meta.AssetType == "MaterialInstance")
        {
            if (!AssetManager::HasInstance())
            {
                return false;
            }

            const std::shared_ptr<MaterialInstance> instance =
                AssetManager::Get().LoadAsset<MaterialInstance>(meta.AssetPath);
            if (!instance)
            {
                ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to load material instance '{}'.", meta.AssetPath);
                return false;
            }

            SetSelectedAsset(&meta);
            return true;
        }

        const bool openingMaterial = meta.AssetType == "Material";
        const bool openingScene = meta.AssetType == "Scene";
        const bool openingAnimationGraph = meta.AssetType == "AnimationGraph";
        const bool openingSprite = meta.AssetType == "Sprite";
        const bool openingSpriteAtlas = meta.AssetType == "SpriteAtlas";
        const bool openingPrefab = meta.AssetType == "Prefab";
        if (!openingMaterial && !openingScene && !openingAnimationGraph && !openingSprite &&
            !openingSpriteAtlas && !openingPrefab)
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

        if (openingSprite)
        {
            return RunWithUnsavedCheck(
                "Save changes to the current sprite before opening another asset?",
                [this]() { return IsSpriteDirty(); },
                [this]() { return SaveSpriteDocument(); },
                std::move(proceed));
        }

        if (openingSpriteAtlas)
        {
            return RunWithUnsavedCheck(
                "Save changes to the current sprite atlas before opening another asset?",
                [this]() { return IsSpriteAtlasDirty(); },
                [this]() { return SaveSpriteAtlasDocument(); },
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
                std::string createdPath;
                std::string createdGuid;
                const AssetEditorResult createResult = m_Context->GetAssetEditorServices().Create(
                    "Scene",
                    directory,
                    "NewScene",
                    createdPath,
                    createdGuid);
                if (!createResult.Ok)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: Create Scene failed — {}", createResult.Message);
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
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
                std::string createdPath;
                std::string createdGuid;
                const AssetEditorResult createResult = m_Context->GetAssetEditorServices().Create(
                    "Material",
                    directory,
                    "NewMaterial",
                    createdPath,
                    createdGuid);
                if (!createResult.Ok)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: Create Material failed — {}", createResult.Message);
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
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

    bool AssetWorkflowModule::TryCreateSpriteInDirectory(std::string_view directoryRel)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        const std::string directory = std::string(directoryRel);
        return RunWithUnsavedCheck(
            "Save changes to the current sprite before creating a new sprite?",
            [this]() { return IsSpriteDirty(); },
            [this]() { return SaveSpriteDocument(); },
            [this, directory]()
            {
                std::string createdPath;
                std::string createdGuid;
                const AssetEditorResult createResult = m_Context->GetAssetEditorServices().Create(
                    "Sprite",
                    directory,
                    "NewSprite",
                    createdPath,
                    createdGuid);
                if (!createResult.Ok)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: Create Sprite failed — {}", createResult.Message);
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
                if (meta == nullptr)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: created sprite has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open created sprite '{}'.", meta->AssetPath);
                    return;
                }

                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryCreateSpriteAtlasInDirectory(std::string_view directoryRel)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        const std::string directory = std::string(directoryRel);
        return RunWithUnsavedCheck(
            "Save changes to the current sprite atlas before creating a new atlas?",
            [this]() { return IsSpriteAtlasDirty(); },
            [this]() { return SaveSpriteAtlasDocument(); },
            [this, directory]()
            {
                std::string createdPath;
                std::string createdGuid;
                const AssetEditorResult createResult = m_Context->GetAssetEditorServices().Create(
                    "SpriteAtlas",
                    directory,
                    "NewSpriteAtlas",
                    createdPath,
                    createdGuid);
                if (!createResult.Ok)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: Create SpriteAtlas failed — {}", createResult.Message);
                    return;
                }

                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
                if (meta == nullptr)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: created atlas has no registry meta.");
                    return;
                }

                RefreshContentBrowser();
                if (!OpenAsset(*meta))
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open created atlas '{}'.", meta->AssetPath);
                    return;
                }

                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryCreateSpriteAtlasFromSelection(const std::vector<const AssetMeta*>& selectedSprites)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        std::vector<const AssetMeta*> spritesOnly;
        spritesOnly.reserve(selectedSprites.size());
        for (const AssetMeta* meta : selectedSprites)
        {
            if (meta != nullptr && meta->AssetType == "Sprite")
            {
                spritesOnly.push_back(meta);
            }
        }
        if (spritesOnly.empty())
        {
            ME_LOG(LogEditor, Warn, "AssetWorkflow: Create Sprite Atlas requires at least one Sprite selection.");
            return false;
        }

        std::string directory = "Assets";
        {
            const std::filesystem::path firstPath(spritesOnly.front()->AssetPath);
            if (firstPath.has_parent_path())
            {
                directory = firstPath.parent_path().generic_string();
            }
        }

        return RunWithUnsavedCheck(
            "Save changes to the current sprite atlas before creating a new atlas?",
            [this]() { return IsSpriteAtlasDirty(); },
            [this]() { return SaveSpriteAtlasDocument(); },
            [this, directory, spritesOnly]()
            {
                SpriteEditorServices& services = m_Context->GetSpriteEditorServices();
                std::shared_ptr<SpriteAtlas> atlas;
                std::string atlasPath;
                const GraphEditorResult createResult = services.CreateAtlas(
                    m_Context->GetAssetEditorServices(),
                    directory,
                    "NewSpriteAtlas",
                    &atlas,
                    &atlasPath,
                    nullptr);
                if (!createResult.Ok || !atlas)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: CreateAtlas failed — {}", createResult.Message);
                    return;
                }

                std::vector<std::shared_ptr<Sprite>> members;
                members.reserve(spritesOnly.size());
                for (const AssetMeta* meta : spritesOnly)
                {
                    std::shared_ptr<Sprite> sprite = AssetManager::Get().LoadAsset<Sprite>(meta->AssetPath);
                    if (sprite)
                    {
                        members.push_back(sprite);
                    }
                }

                EditorCommandStack localStack;
                const GraphEditorResult addResult = services.AddAtlasMembers(localStack, *atlas, members);
                if (!addResult.Ok)
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: AddAtlasMembers failed — {}", addResult.Message);
                }

                if (RenderSystem::HasInstance() && RenderSystem::Get().GetRHI() != nullptr)
                {
                    const GraphEditorResult rebuildResult =
                        services.RebuildAtlas(*atlas, *RenderSystem::Get().GetRHI());
                    if (!rebuildResult.Ok)
                    {
                        ME_LOG(LogEditor, Warn, "AssetWorkflow: initial Rebuild failed — {}", rebuildResult.Message);
                    }
                    else
                    {
                        services.SaveAtlas(*atlas);
                        for (const std::shared_ptr<Sprite>& sprite : members)
                        {
                            if (sprite)
                            {
                                services.SaveSprite(*sprite);
                            }
                        }
                    }
                }

                RefreshContentBrowser();
                const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(atlasPath);
                if (meta == nullptr)
                {
                    meta = AssetManager::Get().FindAssetMetaByGuid(atlas->GetGuid());
                }
                if (meta == nullptr)
                {
                    return;
                }

                if (!OpenAsset(*meta))
                {
                    ME_LOG(LogEditor, Warn, "AssetWorkflow: failed to open created atlas '{}'.", meta->AssetPath);
                    return;
                }
                SetSelectedAsset(meta);
            });
    }

    bool AssetWorkflowModule::TryCreateMaterialInstanceInDirectory(std::string_view directoryRel)
    {
        if (!m_Context || !AssetManager::HasInstance())
        {
            return false;
        }

        const std::string directory = std::string(directoryRel);
        std::string createdPath;
        std::string createdGuid;
        const AssetEditorResult createResult = m_Context->GetAssetEditorServices().Create(
            "MaterialInstance",
            directory,
            "NewMaterialInstance",
            createdPath,
            createdGuid);
        if (!createResult.Ok)
        {
            ME_LOG(LogEditor, Warn, "AssetWorkflow: Create MaterialInstance failed — {}", createResult.Message);
            return false;
        }

        const AssetMeta* createdMeta = AssetManager::Get().FindAssetMetaByPath(createdPath);
        if (createdMeta == nullptr)
        {
            ME_LOG(LogEditor, Warn, "AssetWorkflow: created material instance has no registry meta.");
            return false;
        }

        RefreshContentBrowser();
        SetSelectedAsset(createdMeta);
        return true;
    }

    void AssetWorkflowModule::DrawMaterialInstanceInspector(const AssetMeta& meta)
    {
        if (!AssetManager::HasInstance())
        {
            return;
        }

        const std::shared_ptr<MaterialInstance> instance =
            AssetManager::Get().LoadAsset<MaterialInstance>(meta.AssetPath);
        if (!instance)
        {
            ImGui::TextUnformatted("Failed to load material instance.");
            return;
        }

        const Material* parent = instance->GetParent();
        ImGui::Separator();
        ImGui::Text("Parent: %s", parent != nullptr ? parent->GetName().c_str() : "(none)");
        ImGui::TextUnformatted("Scalar overrides");

        for (const MaterialScalarOverrideEntry& entry : instance->GetScalarOverrides())
        {
            float value = entry.Value;
            ImGui::PushID(entry.ParameterName.c_str());
            if (ImGui::DragFloat(entry.ParameterName.c_str(), &value, 0.01f))
            {
                instance->SetScalarParameter(entry.ParameterName, value);
            }
            ImGui::PopID();
        }

        ImGui::InputText("New param", m_NewMicOverrideName, sizeof(m_NewMicOverrideName));
        ImGui::DragFloat("New value", &m_NewMicOverrideValue, 0.01f);
        if (ImGui::Button("Add override") && m_NewMicOverrideName[0] != '\0')
        {
            instance->SetScalarParameter(m_NewMicOverrideName, m_NewMicOverrideValue);
            m_NewMicOverrideName[0] = '\0';
        }

        if (ImGui::Button("Save"))
        {
            if (!AssetManager::Get().SaveAsset(meta.AssetPath, *instance))
            {
                ME_LOG(LogEditor, Error, "AssetWorkflow: failed to save material instance '{}'.", meta.AssetPath);
            }
            else
            {
                ME_LOG(LogEditor, Info, "AssetWorkflow: saved material instance '{}'.", meta.AssetPath);
            }
        }
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

        const std::string destRel =
            destDirectoryRel.empty() ? std::string(".") : std::string(destDirectoryRel);

        AssetEditorServices& assetServices = m_Context->GetAssetEditorServices();
        {
            AssetManager::AssetRegistryBroadcastBatchScope batchScope;
            for (size_t index = 0; index < autoImportPaths.size(); ++index)
            {
                AssetImportProductSpec productSpec;
                productSpec.ProductId = autoImportProductIds[index];

                std::vector<AssetImportCreatedInfo> created;
                std::vector<std::string> warnings;
                const AssetEditorResult importResult = assetServices.Import(
                    autoImportPaths[index].string(),
                    destRel,
                    {productSpec},
                    false,
                    created,
                    warnings);
                if (!importResult.Ok)
                {
                    ++failCount;
                    ME_LOG(LogEditor, Error,
                        "ImportAssetDialog: failed to import '{}': {}",
                        autoImportPaths[index].string(),
                        importResult.Message);
                    continue;
                }

                ++successCount;
                const std::string createdPath = created.empty() ? std::string() : created.front().AssetPath;
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

        const PathRegistry& paths = PathRegistry::Get();
        const std::filesystem::path projectContentRoot = paths.GetProjectContentRoot();
        std::string destRel = ".";
        if (!projectContentRoot.empty())
        {
            std::error_code relativeError;
            const std::filesystem::path relativeDest =
                std::filesystem::relative(destDirectory, projectContentRoot, relativeError);
            if (!relativeError)
            {
                destRel = relativeDest.generic_string();
                if (destRel.empty())
                {
                    destRel = ".";
                }
            }
        }

        AssetImportProductSpec productSpec;
        productSpec.ProductId = productId;
        if (!skeletonAssetPath.empty())
        {
            productSpec.Options["skeletonAssetPath"] = skeletonAssetPath;
        }

        AssetEditorServices& assetServices = m_Context->GetAssetEditorServices();
        AssetManager::AssetRegistryBroadcastBatchScope batchScope;
        for (const std::filesystem::path& sourcePath : sourcePaths)
        {
            std::vector<AssetImportCreatedInfo> created;
            std::vector<std::string> warnings;
            const AssetEditorResult importResult = assetServices.Import(
                sourcePath.string(),
                destRel,
                {productSpec},
                false,
                created,
                warnings);
            if (!importResult.Ok)
            {
                ++failCount;
                ME_LOG(LogEditor, Error,
                    "ImportAssetDialog: failed to import '{}' as '{}': {}",
                    sourcePath.string(),
                    productId,
                    importResult.Message);
                continue;
            }

            ++successCount;
            const std::string createdPath = created.empty() ? std::string() : created.front().AssetPath;
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

        if (m_Context == nullptr)
        {
            return false;
        }

        const AssetEditorResult reimportResult =
            m_Context->GetAssetEditorServices().Reimport(selected->AssetPath);
        if (!reimportResult.Ok)
        {
            ME_LOG(LogEditor, Error,
                "Reimport failed for '{}': {}",
                selected->AssetPath,
                reimportResult.Message);
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
        std::vector<std::string> paths;
        if (m_Context != nullptr)
        {
            paths = m_Context->GetContentBrowser().GetSelection().GetSelectedAssetPaths();
        }
        if (paths.empty())
        {
            const AssetMeta* selected = GetSelectedAsset();
            if (selected != nullptr)
            {
                paths.push_back(selected->AssetPath);
            }
        }
        if (paths.empty())
        {
            return;
        }
        RequestConfirmDeleteAssets(std::move(paths), bUnpackOpenPrefabInstanceRefs);
    }

    void AssetWorkflowModule::RequestConfirmDeleteAssets(
        std::vector<std::string> paths,
        bool bUnpackOpenPrefabInstanceRefs)
    {
        if (paths.empty())
        {
            return;
        }
        m_PendingDeleteIsFolder = false;
        m_PendingDeleteFolder.clear();
        m_PendingDeletePaths = std::move(paths);
        m_PendingDeleteUnpack = bUnpackOpenPrefabInstanceRefs;
        m_DeleteWarningLines = CollectDeleteWarnings(m_PendingDeletePaths);
        m_ShowDeleteConfirm = true;
    }

    void AssetWorkflowModule::RequestConfirmDeleteFolder(std::string directoryRel)
    {
        if (directoryRel.empty())
        {
            // Content root delete is never allowed.
            ME_LOG(LogEditor, Error, "Cannot delete content root.");
            return;
        }
        m_PendingDeleteIsFolder = true;
        m_PendingDeleteFolder = std::move(directoryRel);
        m_PendingDeletePaths.clear();
        m_PendingDeleteUnpack = false;
        m_DeleteWarningLines.clear();
        m_DeleteWarningLines.push_back("Only empty folders can be deleted.");
        m_ShowDeleteConfirm = true;
    }

    bool AssetWorkflowModule::TryBatchRenameAssets(
        const std::vector<std::string>& orderedPaths,
        std::string_view baseStem,
        std::string& outError)
    {
        outError.clear();
        AssetEditorServices services;
        const AssetEditorResult result = services.BatchRename(orderedPaths, baseStem);
        if (!result.Ok)
        {
            outError = result.Message;
            return false;
        }
        RefreshContentBrowser();
        return true;
    }

    std::vector<std::string> AssetWorkflowModule::CollectDeleteWarnings(
        const std::vector<std::string>& paths)
    {
        std::vector<std::string> warnings;
        if (m_Context == nullptr)
        {
            return warnings;
        }

        EditorDocumentHost& host = m_Context->GetDocumentHost();
        SceneEditor* sceneEditor = GetSceneEditor(m_Context);

        for (const std::string& path : paths)
        {
            if (const EditorDocumentSession* session = host.FindSessionByAssetKey(path))
            {
                warnings.push_back(
                    path + (session->IsDirty() ? " is open (dirty)." : " is open in an editor session."));
            }
            if (sceneEditor != nullptr && sceneEditor->GetPrefabStages().HasStage(path))
            {
                warnings.push_back(path + " has an open Prefab Stage — close it before deleting.");
            }
            const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(path);
            if (meta != nullptr && meta->AssetType == "Prefab")
            {
                warnings.push_back(
                    path + ": Prefab instance refs in the open scene are a hard gate (use Unpack & Delete if needed).");
            }
        }

        if (warnings.empty())
        {
            warnings.push_back("No loaded-session warnings (full reference graph is Out of scope).");
        }
        return warnings;
    }

    void AssetWorkflowModule::DrawDeleteConfirmModal()
    {
        if (!m_ShowDeleteConfirm)
        {
            return;
        }

        ImGui::OpenPopup("Delete Assets##AssetWorkflow");
        if (!ImGui::BeginPopupModal("Delete Assets##AssetWorkflow", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            return;
        }

        if (m_PendingDeleteIsFolder)
        {
            ImGui::Text("Delete empty folder?");
            ImGui::TextUnformatted(m_PendingDeleteFolder.c_str());
        }
        else
        {
            ImGui::Text("Delete %d asset(s)?", static_cast<int>(m_PendingDeletePaths.size()));
            const int showCount = (std::min)(8, static_cast<int>(m_PendingDeletePaths.size()));
            for (int i = 0; i < showCount; ++i)
            {
                ImGui::BulletText("%s", m_PendingDeletePaths[static_cast<size_t>(i)].c_str());
            }
            if (static_cast<int>(m_PendingDeletePaths.size()) > showCount)
            {
                ImGui::Text("...and %d more", static_cast<int>(m_PendingDeletePaths.size()) - showCount);
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Warnings:");
        for (const std::string& line : m_DeleteWarningLines)
        {
            ImGui::BulletText("%s", line.c_str());
        }

        if (ImGui::Button("Delete", ImVec2(120, 0)))
        {
            ExecutePendingDelete();
            m_ShowDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            m_ShowDeleteConfirm = false;
            m_PendingDeletePaths.clear();
            m_PendingDeleteFolder.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    void AssetWorkflowModule::ExecutePendingDelete()
    {
        if (m_Context == nullptr)
        {
            return;
        }

        AssetEditorServices& assetServices = m_Context->GetAssetEditorServices();
        if (m_PendingDeleteIsFolder)
        {
            const AssetEditorResult deleteResult = assetServices.DeleteFolder(m_PendingDeleteFolder);
            if (!deleteResult.Ok)
            {
                ME_LOG(
                    LogEditor,
                    Error,
                    "DeleteFolder failed for '{}': {}",
                    m_PendingDeleteFolder,
                    deleteResult.Message);
            }
            else
            {
                ME_LOG(LogEditor, Info, "Deleted folder '{}'", m_PendingDeleteFolder);
                m_Context->GetContentBrowser().GetSelection().Clear();
            }
            m_PendingDeleteFolder.clear();
            RefreshContentBrowser();
            return;
        }

        for (const std::string& assetPath : m_PendingDeletePaths)
        {
            if (AssetManager::Get().FindAssetMetaByPath(assetPath) == nullptr)
            {
                continue;
            }

            const AssetEditorResult deleteResult =
                assetServices.Delete(*m_Context, assetPath, m_PendingDeleteUnpack);
            if (!deleteResult.Ok)
            {
                ME_LOG(LogEditor, Error, "DeleteAsset failed for '{}': {}", assetPath, deleteResult.Message);
                continue;
            }
            ME_LOG(LogEditor, Info, "Deleted '{}'", assetPath);
        }

        m_SelectedAssetPath.clear();
        m_PendingDeletePaths.clear();
        if (m_Context != nullptr)
        {
            m_Context->GetContentBrowser().GetSelection().Clear();
        }
        RefreshContentBrowser();
    }
}


#include "Services/AssetWorkflowModule.h"

#include "SubEditor/Material/MaterialEditor.h"
#include "SubEditor/Scene/SceneEditor.h"
#include "Services/ContentBrowser/ContentBrowserModule.h"
#include "Services/Inspector/InspectorModule.h"
#include "Shell/IEditorContext.h"
#include "UI/Inspector/InspectorPreviewPresenter.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetTypeRegistry.h"
#include "Runtime/Resource/EditorFilesystemMutationPass.h"

#include "Runtime/Resource/AssetMeta.h"

#include "imgui.h"

#include <filesystem>

namespace minEngine
{
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
            context->GetInspectorModule().GetAssetInspection());

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
        m_Context = nullptr;
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

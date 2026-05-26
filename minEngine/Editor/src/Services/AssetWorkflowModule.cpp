#include "Services/AssetWorkflowModule.h"

#include "Material/MaterialEditor.h"
#include "Scene/SceneEditor.h"
#include "Shell/IEditorContext.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Paths/PathRegistry.h"
#include "Runtime/Platform/FileDialog/IFileDialogService.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetTypeRegistry.h"

#include "Runtime/Resource/AssetMeta.h"

#include <filesystem>

namespace minEngine
{
    void AssetWorkflowModule::Register(IEditorContext& context)
    {
        m_Context = &context;
    }

    void AssetWorkflowModule::Shutdown()
    {
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

    void AssetWorkflowModule::ImportAssetDialog()
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

        const std::filesystem::path destDirectory = projectContentRoot / "Imported";
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

        int successCount = 0;
        int failCount = 0;

        AssetManager::SuppressExternalSyncScope suppressScope;

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
}

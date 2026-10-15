#include "Runtime/Resource/Loaders/MaterialLoader.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Resource/AssetSerializeHelper.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/Material/MaterialCompiler/MaterialCompiler.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Core/Object/ObjectCast.h"

namespace minEngine
{
    namespace
    {
        std::shared_ptr<Material> LoadDeserialized(const AssetMeta& meta, std::string* outError)
        {
            std::shared_ptr<Material> material = NewObject<Material>(meta.AssetName, nullptr, meta.Guid);

            const std::string absoluteAssetPath =
                AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();

            if (!AssetSerializeHelper::ReadObjectFromFile(absoluteAssetPath, *material, outError))
            {
                return nullptr;
            }

            std::string graphError;
            if (!material->FinalizeGraphAfterLoad(&graphError))
            {
                if (outError)
                {
                    *outError = graphError;
                }
                ME_LOG(LogAsset, Error, 
                    "MaterialLoader: finalize graph failed for '{}': {}",
                    meta.AssetPath,
                    graphError);
                return nullptr;
            }

            return material;
        }
    }

    std::shared_ptr<Material> MaterialLoader::Load(const AssetMeta& meta, std::string* outError)
    {
        std::shared_ptr<Material> material = LoadDeserialized(meta, outError);
        if (!material)
        {
            return nullptr;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            ME_LOG(LogAsset, Error, "MaterialLoader: RHI unavailable while loading {}.", meta.AssetPath);
            return nullptr;
        }

        MaterialCompileContext ctx;
        ctx.RHI = rhi;
        if (!MaterialCompiler::Compile(*material, ctx))
        {
            ME_LOG(LogAsset, Error, "MaterialLoader: compile failed for {}.", meta.AssetPath);
            for (const MaterialCompileDiagnostic& diagnostic : material->m_LastCompileDiagnostics)
            {
                ME_LOG(LogAsset, Error, "  {}", diagnostic.Message);
            }
            return nullptr;
        }

        return material;
    }

    bool MaterialLoader::Save(const AssetMeta& meta, const Material& asset, std::string* outError)
    {
        const std::string absoluteAssetPath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta.AssetPath).string();
        return AssetSerializeHelper::WriteObjectToFile(absoluteAssetPath, asset, outError);
    }

    bool MaterialLoader::SaveHandler(
        const AssetManager& /*manager*/,
        const AssetMeta& meta,
        const Asset& asset,
        std::string& outError)
    {
        const Material* material = Cast<Material>(&asset);
        if (material == nullptr)
        {
            outError = "SaveHandler: expected Material";
            return false;
        }
        return Save(meta, *material, &outError);
    }

    std::shared_ptr<Asset> MaterialLoader::CreateHandler(
        AssetManager& manager,
        const std::string& assetName,
        const std::string& directoryRel,
        std::string& outError)
    {
        const std::string relativePath =
            manager.AllocateUniqueAssetRelativePath(directoryRel, assetName, ".memtl");
        if (relativePath.empty())
        {
            outError = "failed to allocate unique path";
            ME_LOG(LogAsset, Error, "MaterialLoader::Create: failed to allocate unique path for '{}'.", assetName);
            return nullptr;
        }

        const std::filesystem::path absolutePath = manager.ResolveAssetAbsolutePath(relativePath);
        std::error_code createError;
        std::filesystem::create_directories(absolutePath.parent_path(), createError);
        if (createError)
        {
            ME_LOG(LogAsset, Error, 
                "MaterialLoader::Create: failed to create directory '{}': {}",
                absolutePath.parent_path().string(),
                createError.message());
            return nullptr;
        }

        const std::string materialName = absolutePath.stem().string();
        const GUID assetGuid = GenerateGUID();
        std::shared_ptr<Material> material = NewObject<Material>(materialName, nullptr, assetGuid);
        material->m_ShadingModel = MaterialShadingModel::Unlit;

        if (!manager.SaveRegisteredAsset(AssetManager::MakePendingSaveMeta(relativePath, materialName, "Material", assetGuid), *material))
        {
            std::error_code removeError;
            std::filesystem::remove(absolutePath, removeError);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(absolutePath);

        AssetMeta meta = manager.RegisterAsset(relativePath, "Material", &assetGuid);
        if (meta.AssetPath.empty())
        {
            ME_LOG(LogAsset, Error, "MaterialLoader::Create: RegisterAsset failed for '{}'.", relativePath);
            return nullptr;
        }

        manager.NoteEditorFilesystemMutation(manager.BuildMetaAbsolutePath(meta.AssetPath));

        ME_LOG(LogAsset, Info, "MaterialLoader::Create: created '{}'.", meta.AssetPath);
        manager.CacheCreatedAsset(meta.AssetPath, material);
        return std::static_pointer_cast<Asset>(material);
    }

}

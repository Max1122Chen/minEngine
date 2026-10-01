#include "Services/Sprite/SpriteEditorServices.h"

#include "Commands/Sprite/EditorSpriteCommands.h"
#include "Services/Asset/AssetEditorServices.h"
#include "Shell/EditorCommandStack.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

namespace minEngine
{
    GraphEditorResult SpriteEditorServices::OkResult()
    {
        GraphEditorResult result;
        result.Ok = true;
        result.Code = GraphEditorError::None;
        return result;
    }

    GraphEditorResult SpriteEditorServices::Fail(GraphEditorError code, std::string message)
    {
        GraphEditorResult result;
        result.Ok = false;
        result.Code = code;
        result.Message = std::move(message);
        return result;
    }

    GraphEditorResult SpriteEditorServices::CreateSpriteFromTexture(
        AssetEditorServices& assetServices,
        const std::shared_ptr<Texture2D>& texture,
        const Vector4& sourceRectPx,
        std::string_view directory,
        std::string_view name,
        std::shared_ptr<Sprite>* outSprite,
        std::string* outPath,
        std::string* outGuid)
    {
        if (outSprite != nullptr)
        {
            outSprite->reset();
        }
        if (outPath != nullptr)
        {
            outPath->clear();
        }
        if (outGuid != nullptr)
        {
            outGuid->clear();
        }

        if (!texture)
        {
            return Fail(GraphEditorError::InvalidArgument, "texture is required.");
        }
        if (directory.empty() || name.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "directory and name are required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail(GraphEditorError::Internal, "AssetManager is not available.");
        }

        std::string createdPath;
        std::string createdGuid;
        const AssetEditorResult createResult =
            assetServices.Create("Sprite", directory, name, createdPath, createdGuid);
        if (!createResult.Ok)
        {
            return Fail(
                GraphEditorError::Internal,
                createResult.Message.empty() ? "Create Sprite failed." : createResult.Message);
        }

        std::shared_ptr<Sprite> sprite = AssetManager::Get().LoadAsset<Sprite>(createdPath);
        if (!sprite)
        {
            return Fail(GraphEditorError::Internal, "Failed to load created Sprite.");
        }

        sprite->ConfigureFromTexture(texture, sourceRectPx);

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
        if (meta == nullptr)
        {
            return Fail(GraphEditorError::Internal, "Created Sprite has no registry meta.");
        }

        if (!AssetManager::Get().SaveAsset(meta->AssetPath, *sprite))
        {
            return Fail(GraphEditorError::Internal, "Failed to save created Sprite.");
        }

        if (outSprite != nullptr)
        {
            *outSprite = sprite;
        }
        if (outPath != nullptr)
        {
            *outPath = meta->AssetPath;
        }
        if (outGuid != nullptr)
        {
            *outGuid = meta->Guid.ToString();
        }
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::SetSpriteSourceRect(
        EditorCommandStack& commandStack,
        Sprite& sprite,
        const Vector4& sourceRectPx)
    {
        if (sprite.GetAtlas() != nullptr)
        {
            ME_LOG(
                LogAsset,
                Info,
                "SpriteEditorServices: '{}' source rect changed while in atlas; ClearBaked — Rebuild required.",
                sprite.GetName());
        }

        commandStack.Execute(std::make_unique<EditorSetSpriteSourceRectCommand>(sprite, sourceRectPx));
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::CreateAtlas(
        AssetEditorServices& assetServices,
        std::string_view directory,
        std::string_view name,
        std::shared_ptr<SpriteAtlas>* outAtlas,
        std::string* outPath,
        std::string* outGuid)
    {
        if (outAtlas != nullptr)
        {
            outAtlas->reset();
        }
        if (outPath != nullptr)
        {
            outPath->clear();
        }
        if (outGuid != nullptr)
        {
            outGuid->clear();
        }

        if (directory.empty() || name.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "directory and name are required.");
        }
        if (!AssetManager::HasInstance())
        {
            return Fail(GraphEditorError::Internal, "AssetManager is not available.");
        }

        std::string createdPath;
        std::string createdGuid;
        const AssetEditorResult createResult =
            assetServices.Create("SpriteAtlas", directory, name, createdPath, createdGuid);
        if (!createResult.Ok)
        {
            return Fail(
                GraphEditorError::Internal,
                createResult.Message.empty() ? "Create SpriteAtlas failed." : createResult.Message);
        }

        std::shared_ptr<SpriteAtlas> atlas = AssetManager::Get().LoadAsset<SpriteAtlas>(createdPath);
        if (!atlas)
        {
            return Fail(GraphEditorError::Internal, "Failed to load created SpriteAtlas.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByPath(createdPath);
        if (meta == nullptr)
        {
            return Fail(GraphEditorError::Internal, "Created SpriteAtlas has no registry meta.");
        }

        if (!AssetManager::Get().SaveAsset(meta->AssetPath, *atlas))
        {
            return Fail(GraphEditorError::Internal, "Failed to save created SpriteAtlas.");
        }

        if (outAtlas != nullptr)
        {
            *outAtlas = atlas;
        }
        if (outPath != nullptr)
        {
            *outPath = meta->AssetPath;
        }
        if (outGuid != nullptr)
        {
            *outGuid = meta->Guid.ToString();
        }
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::AddAtlasMembers(
        EditorCommandStack& commandStack,
        SpriteAtlas& atlas,
        const std::vector<std::shared_ptr<Sprite>>& sprites)
    {
        if (sprites.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "sprites list is empty.");
        }

        for (const std::shared_ptr<Sprite>& sprite : sprites)
        {
            if (!sprite)
            {
                return Fail(GraphEditorError::InvalidArgument, "sprites contains null entry.");
            }
            if (sprite->GetAtlas() != nullptr && sprite->GetAtlas() != &atlas)
            {
                return Fail(
                    GraphEditorError::InvalidArgument,
                    "Sprite '" + sprite->GetName() + "' already belongs to another atlas.");
            }
        }

        commandStack.Execute(std::make_unique<EditorAddAtlasMembersCommand>(atlas, sprites));
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::RemoveAtlasMember(
        EditorCommandStack& commandStack,
        SpriteAtlas& atlas,
        const std::shared_ptr<Sprite>& sprite)
    {
        if (!sprite)
        {
            return Fail(GraphEditorError::InvalidArgument, "sprite is required.");
        }

        commandStack.Execute(std::make_unique<EditorRemoveAtlasMemberCommand>(atlas, sprite));
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::RebuildAtlas(SpriteAtlas& atlas, RHI& rhi)
    {
        const SpriteAtlasRebuildResult result = atlas.Rebuild(rhi);
        if (!result.Ok)
        {
            return Fail(GraphEditorError::Internal, result.Message.empty() ? "Rebuild failed." : result.Message);
        }
        return OkResult();
    }


    GraphEditorResult SpriteEditorServices::SetAtlasPadding(
        SpriteAtlas& atlas,
        int32_t padding,
        SpriteAtlasPaddingMode mode)
    {
        atlas.SetPadding(padding);
        atlas.SetPaddingMode(mode);
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::SaveSprite(const Sprite& sprite)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail(GraphEditorError::Internal, "AssetManager is not available.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(sprite.GetGuid());
        if (meta == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Sprite has no registry meta.");
        }

        if (!AssetManager::Get().SaveAsset(meta->AssetPath, sprite))
        {
            return Fail(GraphEditorError::Internal, "SaveSprite failed.");
        }
        return OkResult();
    }

    GraphEditorResult SpriteEditorServices::SaveAtlas(const SpriteAtlas& atlas)
    {
        if (!AssetManager::HasInstance())
        {
            return Fail(GraphEditorError::Internal, "AssetManager is not available.");
        }

        const AssetMeta* meta = AssetManager::Get().FindAssetMetaByGuid(atlas.GetGuid());
        if (meta == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "SpriteAtlas has no registry meta.");
        }

        if (!AssetManager::Get().SaveAsset(meta->AssetPath, atlas))
        {
            return Fail(GraphEditorError::Internal, "SaveAtlas failed.");
        }
        return OkResult();
    }
}

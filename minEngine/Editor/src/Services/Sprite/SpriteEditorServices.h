#pragma once

#include "Services/Graph/GraphEditorResult.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlas.h"

#include "Runtime/Core/Math/Math.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class AssetEditorServices;
    class EditorCommandStack;
    class RHI;
    class Sprite;
    class SpriteAtlas;
    class Texture2D;

    /**
     * Sprite / SpriteAtlas authoring primitives (SPR-F04).
     * Undoable ops push onto the caller-provided command stack (document session when F05 opens tabs).
     * Rebuild / Save are non-undo. No ImGui / no Automation JSON parsing.
     */
    class SpriteEditorServices
    {
    public:
        GraphEditorResult CreateSpriteFromTexture(
            AssetEditorServices& assetServices,
            const std::shared_ptr<Texture2D>& texture,
            const Vector4& sourceRectPx,
            std::string_view directory,
            std::string_view name,
            std::shared_ptr<Sprite>* outSprite,
            std::string* outPath = nullptr,
            std::string* outGuid = nullptr);

        GraphEditorResult SetSpriteSourceRect(
            EditorCommandStack& commandStack,
            Sprite& sprite,
            const Vector4& sourceRectPx);

        GraphEditorResult CreateAtlas(
            AssetEditorServices& assetServices,
            std::string_view directory,
            std::string_view name,
            std::shared_ptr<SpriteAtlas>* outAtlas,
            std::string* outPath = nullptr,
            std::string* outGuid = nullptr);

        GraphEditorResult AddAtlasMembers(
            EditorCommandStack& commandStack,
            SpriteAtlas& atlas,
            const std::vector<std::shared_ptr<Sprite>>& sprites);

        GraphEditorResult RemoveAtlasMember(
            EditorCommandStack& commandStack,
            SpriteAtlas& atlas,
            const std::shared_ptr<Sprite>& sprite);

        GraphEditorResult RebuildAtlas(SpriteAtlas& atlas, RHI& rhi);

        GraphEditorResult SetAtlasPadding(
            SpriteAtlas& atlas,
            int32_t padding,
            SpriteAtlasPaddingMode mode);

        GraphEditorResult SaveSprite(const Sprite& sprite);
        GraphEditorResult SaveAtlas(const SpriteAtlas& atlas);

    private:
        static GraphEditorResult OkResult();
        static GraphEditorResult Fail(GraphEditorError code, std::string message);
    };
}

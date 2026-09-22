#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace minEngine
{
    class Sprite;
    class RHI;
    class Texture2D;

    struct SpritePackLayoutEntry
    {
        int32_t MemberIndex = 0;
        int32_t Width = 0;
        int32_t Height = 0;
        int32_t X = 0;
        int32_t Y = 0;
    };

    struct SpritePackLayoutResult
    {
        bool Ok = false;
        std::string Message;
        int32_t AtlasWidth = 0;
        int32_t AtlasHeight = 0;
        std::vector<SpritePackLayoutEntry> Entries;
    };

    /**
     * Shelf/row packer + blit helpers for SpriteAtlas::Rebuild.
     * Pixel buffers use ImageLoader orientation (flipVertical=true): row 0 = image bottom (GL).
     * SourceRectPx on Sprite is top-left authoring space (y down from image top).
     */
    class SpriteAtlasPacker
    {
    public:
        static SpritePackLayoutResult PackShelf(
            const std::vector<SpritePackLayoutEntry>& sizes,
            int32_t maxSize,
            int32_t padding);

        static bool TryLoadMemberSourcePixels(
            const Sprite& sprite,
            ImagePixels& outPixels,
            std::string* outError);

        static void BlitTopLeftRectToAtlas(
            const ImagePixels& source,
            const Vector4& sourceRectTopLeft,
            ImagePixels& atlas,
            int32_t destX,
            int32_t destY);

        static std::shared_ptr<Texture2D> CreateAtlasTexture(
            RHI& rhi,
            const ImagePixels& atlasPixels,
            const std::string& debugName);
    };
}

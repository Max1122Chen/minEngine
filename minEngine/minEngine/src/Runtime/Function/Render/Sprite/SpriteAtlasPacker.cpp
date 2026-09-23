#include "SpriteAtlasPacker.h"

#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"
#include "Runtime/Resource/Loaders/Texture2DLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace minEngine
{
    SpritePackLayoutResult SpriteAtlasPacker::PackShelf(
        const std::vector<SpritePackLayoutEntry>& sizes,
        int32_t maxSize,
        int32_t padding)
    {
        SpritePackLayoutResult result;
        if (sizes.empty())
        {
            result.Message = "PackShelf: no entries.";
            return result;
        }

        if (maxSize <= 0)
        {
            result.Message = "PackShelf: MaxSize must be positive.";
            return result;
        }

        // SPR-F07: padding is an exclusive inset ring per sprite (not a shared gap).
        const int32_t pad = std::max(0, padding);
        std::vector<SpritePackLayoutEntry> sorted = sizes;
        std::stable_sort(
            sorted.begin(),
            sorted.end(),
            [](const SpritePackLayoutEntry& a, const SpritePackLayoutEntry& b)
            {
                if (a.Height != b.Height)
                {
                    return a.Height > b.Height;
                }
                return a.Width > b.Width;
            });

        int32_t cursorX = 0;
        int32_t cursorY = 0;
        int32_t rowOuterHeight = 0;
        int32_t usedWidth = 0;
        int32_t usedHeight = 0;

        result.Entries.resize(sizes.size());

        for (const SpritePackLayoutEntry& item : sorted)
        {
            if (item.Width <= 0 || item.Height <= 0)
            {
                result.Message = "PackShelf: entry has non-positive size.";
                return result;
            }

            const int32_t outerW = item.Width + 2 * pad;
            const int32_t outerH = item.Height + 2 * pad;
            if (outerW > maxSize || outerH > maxSize)
            {
                result.Message = "PackShelf: entry with padding ring exceeds MaxSize.";
                return result;
            }

            if (cursorX > 0 && cursorX + outerW > maxSize)
            {
                cursorX = 0;
                cursorY += rowOuterHeight;
                rowOuterHeight = 0;
            }

            if (cursorY + outerH > maxSize)
            {
                result.Message = "PackShelf: exceeds MaxSize (single page).";
                return result;
            }

            SpritePackLayoutEntry placed = item;
            placed.X = cursorX + pad;
            placed.Y = cursorY + pad;
            result.Entries[static_cast<size_t>(item.MemberIndex)] = placed;

            cursorX += outerW;
            rowOuterHeight = std::max(rowOuterHeight, outerH);
            usedWidth = std::max(usedWidth, cursorX);
            usedHeight = std::max(usedHeight, cursorY + outerH);
        }

        result.AtlasWidth = std::max(1, usedWidth);
        result.AtlasHeight = std::max(1, usedHeight);
        result.Ok = true;
        return result;
    }

    void SpriteAtlasPacker::DilateBorderRing(
        ImagePixels& atlas,
        int32_t contentX,
        int32_t contentY,
        int32_t contentW,
        int32_t contentH,
        int32_t padding)
    {
        if (!atlas.IsValid() || atlas.U8 == nullptr || padding <= 0 || contentW <= 0 || contentH <= 0)
        {
            return;
        }

        const int32_t channels = atlas.Channels;
        if (channels <= 0)
        {
            return;
        }

        auto copyPixel = [&](int32_t srcX, int32_t srcY, int32_t dstX, int32_t dstY)
        {
            if (srcX < 0 || srcY < 0 || dstX < 0 || dstY < 0
                || srcX >= atlas.Width || srcY >= atlas.Height
                || dstX >= atlas.Width || dstY >= atlas.Height)
            {
                return;
            }

            const size_t srcIndex =
                (static_cast<size_t>(srcY) * static_cast<size_t>(atlas.Width)
                 + static_cast<size_t>(srcX))
                * static_cast<size_t>(channels);
            const size_t dstIndex =
                (static_cast<size_t>(dstY) * static_cast<size_t>(atlas.Width)
                 + static_cast<size_t>(dstX))
                * static_cast<size_t>(channels);
            for (int32_t c = 0; c < channels; ++c)
            {
                atlas.U8[dstIndex + static_cast<size_t>(c)] = atlas.U8[srcIndex + static_cast<size_t>(c)];
            }
        };

        const int32_t left = contentX;
        const int32_t right = contentX + contentW - 1;
        const int32_t bottom = contentY;
        const int32_t top = contentY + contentH - 1;

        for (int32_t d = 1; d <= padding; ++d)
        {
            for (int32_t y = bottom; y <= top; ++y)
            {
                copyPixel(left, y, left - d, y);
                copyPixel(right, y, right + d, y);
            }
            for (int32_t x = left; x <= right; ++x)
            {
                copyPixel(x, bottom, x, bottom - d);
                copyPixel(x, top, x, top + d);
            }

            for (int32_t dy = 1; dy <= d; ++dy)
            {
                for (int32_t dx = 1; dx <= d; ++dx)
                {
                    copyPixel(left, bottom, left - dx, bottom - dy);
                    copyPixel(right, bottom, right + dx, bottom - dy);
                    copyPixel(left, top, left - dx, top + dy);
                    copyPixel(right, top, right + dx, top + dy);
                }
            }
        }
    }

    bool SpriteAtlasPacker::TryLoadMemberSourcePixels(
        const Sprite& sprite,
        ImagePixels& outPixels,
        std::string* outError)
    {
        ImageLoader::Free(outPixels);

        if (sprite.TryCopyAuthoringPixels(outPixels))
        {
            return true;
        }

        Texture2D* sourceTexture = sprite.GetSourceTexture();
        if (sourceTexture == nullptr)
        {
            if (outError)
            {
                *outError = "Sprite has no SourceTexture.";
            }
            return false;
        }

        const AssetMeta* meta = sourceTexture->GetMeta();
        if (meta == nullptr || meta->AssetPath.empty())
        {
            if (outError)
            {
                *outError =
                    "SourceTexture has no AssetMeta path and Sprite has no authoring pixels; "
                    "call SetAuthoringPixels or use a disk-backed Texture2D.";
            }
            return false;
        }

        const std::string absolutePath =
            AssetManager::Get().ResolveAssetAbsolutePath(meta->AssetPath).string();
        std::string loadError;
        // Match Texture2DLoader: flipVertical=true so row 0 is image bottom (GL).
        if (!ImageLoader::Load(absolutePath, outPixels, true, &loadError))
        {
            if (outError)
            {
                *outError = loadError.empty() ? "ImageLoader::Load failed." : loadError;
            }
            return false;
        }

        return true;
    }

    void SpriteAtlasPacker::BlitTopLeftRectToAtlas(
        const ImagePixels& source,
        const Vector4& sourceRectTopLeft,
        ImagePixels& atlas,
        int32_t destX,
        int32_t destY)
    {
        if (!source.IsValid() || source.U8 == nullptr || !atlas.IsValid() || atlas.U8 == nullptr)
        {
            return;
        }

        const int32_t srcW = static_cast<int32_t>(std::lround(sourceRectTopLeft.z));
        const int32_t srcH = static_cast<int32_t>(std::lround(sourceRectTopLeft.w));
        const int32_t srcX = static_cast<int32_t>(std::lround(sourceRectTopLeft.x));
        const int32_t srcYTop = static_cast<int32_t>(std::lround(sourceRectTopLeft.y));
        if (srcW <= 0 || srcH <= 0)
        {
            return;
        }

        // Convert top-left authoring Y into flipped memory rows (row 0 = bottom).
        const int32_t srcRow0 = source.Height - (srcYTop + srcH);
        const int32_t channels = std::min(source.Channels, atlas.Channels);
        if (channels <= 0)
        {
            return;
        }

        for (int32_t row = 0; row < srcH; ++row)
        {
            const int32_t srcRow = srcRow0 + row;
            const int32_t dstRow = destY + row;
            if (srcRow < 0 || srcRow >= source.Height || dstRow < 0 || dstRow >= atlas.Height)
            {
                continue;
            }

            for (int32_t col = 0; col < srcW; ++col)
            {
                const int32_t srcCol = srcX + col;
                const int32_t dstCol = destX + col;
                if (srcCol < 0 || srcCol >= source.Width || dstCol < 0 || dstCol >= atlas.Width)
                {
                    continue;
                }

                const size_t srcIndex =
                    (static_cast<size_t>(srcRow) * static_cast<size_t>(source.Width)
                     + static_cast<size_t>(srcCol))
                    * static_cast<size_t>(source.Channels);
                const size_t dstIndex =
                    (static_cast<size_t>(dstRow) * static_cast<size_t>(atlas.Width)
                     + static_cast<size_t>(dstCol))
                    * static_cast<size_t>(atlas.Channels);

                for (int32_t c = 0; c < channels; ++c)
                {
                    atlas.U8[dstIndex + static_cast<size_t>(c)] =
                        source.U8[srcIndex + static_cast<size_t>(c)];
                }

                if (atlas.Channels == 4 && source.Channels < 4)
                {
                    atlas.U8[dstIndex + 3] = 255;
                }
            }
        }
    }

    std::shared_ptr<Texture2D> SpriteAtlasPacker::CreateAtlasTexture(
        RHI& rhi,
        const ImagePixels& atlasPixels,
        const std::string& debugName)
    {
        return Texture2DLoader::CreateFromPixels(rhi, atlasPixels, debugName, GenerateGUID());
    }
}

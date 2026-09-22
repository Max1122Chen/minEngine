#include "SpriteAtlas.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/Sprite/SpriteAtlasPacker.h"
#include "Runtime/Resource/Loaders/ImageLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace minEngine
{
    void SpriteAtlas::SetMaxSize(int32_t maxSize)
    {
        m_MaxSize = std::max(1, maxSize);
    }

    void SpriteAtlas::SetPadding(int32_t padding)
    {
        m_Padding = std::max(0, padding);
    }

    bool SpriteAtlas::AddMember(const std::shared_ptr<Sprite>& sprite, std::string* outError)
    {
        if (!sprite)
        {
            if (outError)
            {
                *outError = "AddMember: sprite is null.";
            }
            return false;
        }

        if (sprite->GetAtlas() != nullptr && sprite->GetAtlas() != this)
        {
            if (outError)
            {
                *outError = "AddMember: sprite already belongs to another atlas.";
            }
            return false;
        }

        for (const std::shared_ptr<Sprite>& member : m_Members)
        {
            if (member == sprite)
            {
                return true;
            }
        }

        m_Members.push_back(sprite);
        sprite->SetAtlasOwner(this);
        return true;
    }

    bool SpriteAtlas::RemoveMember(const std::shared_ptr<Sprite>& sprite)
    {
        if (!sprite)
        {
            return false;
        }

        const auto it = std::find(m_Members.begin(), m_Members.end(), sprite);
        if (it == m_Members.end())
        {
            return false;
        }

        (*it)->ClearBaked();
        (*it)->SetAtlasOwner(nullptr);
        m_Members.erase(it);
        return true;
    }

    void SpriteAtlas::ClearMembers()
    {
        for (const std::shared_ptr<Sprite>& member : m_Members)
        {
            if (member)
            {
                member->ClearBaked();
                member->SetAtlasOwner(nullptr);
            }
        }
        m_Members.clear();
        m_Slots.clear();
    }

    void SpriteAtlas::RebindMemberOwnersAfterLoad()
    {
        for (const std::shared_ptr<Sprite>& member : m_Members)
        {
            if (member)
            {
                member->SetAtlasOwner(this);
            }
        }
    }

    SpriteAtlasRebuildResult SpriteAtlas::Rebuild(RHI& rhi)
    {
        SpriteAtlasRebuildResult result;
        if (m_Members.empty())
        {
            result.Message = "Rebuild: atlas has no members.";
            return result;
        }

        std::vector<ImagePixels> sourcePixels(m_Members.size());
        std::vector<Vector4> sourceRects(m_Members.size());
        std::vector<SpritePackLayoutEntry> sizes;
        sizes.reserve(m_Members.size());

        for (size_t i = 0; i < m_Members.size(); ++i)
        {
            const std::shared_ptr<Sprite>& member = m_Members[i];
            if (!member)
            {
                result.Message = "Rebuild: null member.";
                for (ImagePixels& pixels : sourcePixels)
                {
                    ImageLoader::Free(pixels);
                }
                return result;
            }

            std::string loadError;
            if (!SpriteAtlasPacker::TryLoadMemberSourcePixels(*member, sourcePixels[i], &loadError))
            {
                result.Message = "Rebuild: failed to load source pixels for member '" + member->GetName()
                    + "': " + loadError;
                for (ImagePixels& pixels : sourcePixels)
                {
                    ImageLoader::Free(pixels);
                }
                return result;
            }

            Vector4 rect = member->ResolveSourceRectPx();
            if (rect.z <= 0.0f || rect.w <= 0.0f)
            {
                rect = Vector4(
                    0.0f,
                    0.0f,
                    static_cast<float>(sourcePixels[i].Width),
                    static_cast<float>(sourcePixels[i].Height));
            }
            sourceRects[i] = rect;

            SpritePackLayoutEntry entry;
            entry.MemberIndex = static_cast<int32_t>(i);
            entry.Width = static_cast<int32_t>(std::lround(rect.z));
            entry.Height = static_cast<int32_t>(std::lround(rect.w));
            sizes.push_back(entry);
        }

        const SpritePackLayoutResult layout =
            SpriteAtlasPacker::PackShelf(sizes, m_MaxSize, m_Padding);
        if (!layout.Ok)
        {
            result.Message = layout.Message;
            for (ImagePixels& pixels : sourcePixels)
            {
                ImageLoader::Free(pixels);
            }
            return result;
        }

        ImagePixels atlasPixels;
        atlasPixels.Storage = ImageStorage::UInt8;
        atlasPixels.Width = layout.AtlasWidth;
        atlasPixels.Height = layout.AtlasHeight;
        atlasPixels.Channels = 4;
        const size_t byteCount =
            static_cast<size_t>(atlasPixels.Width) * static_cast<size_t>(atlasPixels.Height) * 4u;
        atlasPixels.U8 = static_cast<unsigned char*>(std::malloc(byteCount));
        if (atlasPixels.U8 == nullptr)
        {
            result.Message = "Rebuild: failed to allocate atlas pixels.";
            for (ImagePixels& pixels : sourcePixels)
            {
                ImageLoader::Free(pixels);
            }
            return result;
        }
        std::memset(atlasPixels.U8, 0, byteCount);

        std::vector<SpriteAtlasSlot> pendingSlots;
        pendingSlots.reserve(m_Members.size());

        for (size_t i = 0; i < m_Members.size(); ++i)
        {
            const SpritePackLayoutEntry& placed = layout.Entries[i];
            SpriteAtlasPacker::BlitTopLeftRectToAtlas(
                sourcePixels[i],
                sourceRects[i],
                atlasPixels,
                placed.X,
                placed.Y);

            // DilateBorder: MVP copies edge into a 1px ring inside the slot when padding > 0.
            if (m_PaddingMode == SpriteAtlasPaddingMode::DilateBorder && m_Padding > 0)
            {
                // Intentionally minimal: zero padding remains; full dilate can come later.
            }

            SpriteAtlasSlot slot;
            slot.SpriteRef = m_Members[i];
            slot.X = placed.X;
            slot.Y = placed.Y;
            slot.Width = placed.Width;
            slot.Height = placed.Height;
            slot.Page = 0;
            pendingSlots.push_back(slot);
        }

        for (ImagePixels& pixels : sourcePixels)
        {
            ImageLoader::Free(pixels);
        }

        const std::string atlasTexName = GetName().empty() ? "SpriteAtlas" : (GetName() + "_Atlas");
        std::shared_ptr<Texture2D> generated =
            SpriteAtlasPacker::CreateAtlasTexture(rhi, atlasPixels, atlasTexName);
        ImageLoader::Free(atlasPixels);
        if (!generated)
        {
            result.Message = "Rebuild: CreateFromPixels failed.";
            return result;
        }

        // Commit only after successful texture create (transactional).
        m_GeneratedTextures.clear();
        m_GeneratedTextures.push_back(generated);
        m_Slots = std::move(pendingSlots);

        for (const SpriteAtlasSlot& slot : m_Slots)
        {
            if (!slot.SpriteRef)
            {
                continue;
            }

            // Slot X/Y are in flipped memory space (row 0 = bottom). Convert to top-left for BakedRect.
            const float topLeftY = static_cast<float>(generated->GetHeight() - (slot.Y + slot.Height));
            const Vector4 bakedRect(
                static_cast<float>(slot.X),
                topLeftY,
                static_cast<float>(slot.Width),
                static_cast<float>(slot.Height));
            slot.SpriteRef->SetBaked(generated, bakedRect);
            slot.SpriteRef->SetAtlasOwner(this);
        }

        result.Ok = true;
        result.Message = "Rebuild succeeded.";
        ME_LOG(LogAsset, Info, "SpriteAtlas::Rebuild: '{}' -> {}x{} ({} members).",
            GetName(),
            generated->GetWidth(),
            generated->GetHeight(),
            m_Members.size());
        return result;
    }
}

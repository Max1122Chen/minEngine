#pragma once

#include "Core.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"

#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class RHI;

    ME_ENUM()
    enum class SpriteAtlasPaddingMode : uint8_t
    {
        PadWithZero = 0,
        DilateBorder,
    };

    struct SpriteAtlasSlot
    {
        std::shared_ptr<Sprite> SpriteRef;
        int32_t X = 0;
        int32_t Y = 0;
        int32_t Width = 0;
        int32_t Height = 0;
        int32_t Page = 0;
    };

    struct SpriteAtlasRebuildResult
    {
        bool Ok = false;
        std::string Message;
    };

    ME_CLASS()
    class SpriteAtlas : public Asset
    {
        ME_GENERATED_BODY()
    public:
        SpriteAtlas() = default;
        virtual ~SpriteAtlas() = default;

        int32_t GetMaxSize() const { return m_MaxSize; }
        void SetMaxSize(int32_t maxSize);

        int32_t GetPadding() const { return m_Padding; }
        void SetPadding(int32_t padding);

        SpriteAtlasPaddingMode GetPaddingMode() const { return m_PaddingMode; }
        void SetPaddingMode(SpriteAtlasPaddingMode mode) { m_PaddingMode = mode; }

        const std::vector<std::shared_ptr<Sprite>>& GetMembers() const { return m_Members; }
        const std::vector<std::shared_ptr<Texture2D>>& GetGeneratedTextures() const { return m_GeneratedTextures; }
        const std::vector<SpriteAtlasSlot>& GetSlots() const { return m_Slots; }

        bool AddMember(const std::shared_ptr<Sprite>& sprite, std::string* outError = nullptr);
        bool RemoveMember(const std::shared_ptr<Sprite>& sprite);
        void ClearMembers();

        /**
         * Transactional Rebuild: pack Source pixels into one GeneratedTexture, then write Baked*
         * on success only. Requires RHI for CreateFromPixels.
         */
        SpriteAtlasRebuildResult Rebuild(RHI& rhi);

        /** Restore non-serialized Sprite::m_AtlasOwner after deserialize. */
        void RebindMemberOwnersAfterLoad();

    private:
        ME_PROPERTY()
        int32_t m_MaxSize = 2048;

        ME_PROPERTY()
        int32_t m_Padding = 1;

        ME_PROPERTY()
        SpriteAtlasPaddingMode m_PaddingMode = SpriteAtlasPaddingMode::PadWithZero;

        ME_PROPERTY()
        std::vector<std::shared_ptr<Sprite>> m_Members;

        ME_PROPERTY()
        std::vector<std::shared_ptr<Texture2D>> m_GeneratedTextures;

        std::vector<SpriteAtlasSlot> m_Slots;
    };
}

#include "Generated/Reflection/SpriteAtlas.gen.h"

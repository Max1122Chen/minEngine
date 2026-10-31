#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Function/Render/Texture.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class SpriteAtlas;
    struct ImagePixels;

    /**
     * One authored sprite frame: Source (always) + optional Baked (after atlas Rebuild).
     * SourceRectPx / BakedRectPx: Vector4(x, y, width, height) in top-left pixel space (y down).
     * GetUVRectNormalized(): OpenGL-style UVs for SpriteQuadMesh (v=0 bottom); converts with V flip.
     */
    ME_CLASS()
    class MINENGINE_API Sprite : public Asset
    {
        ME_GENERATED_BODY()
    public:
        Sprite() = default;
        virtual ~Sprite();

        void SetSourceTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetSourceTexture() const { return m_SourceTexture.get(); }
        const std::shared_ptr<Texture2D>& GetSourceTextureShared() const { return m_SourceTexture; }

        /** (x, y, w, h) top-left pixel space. Width/height <= 0 means "use full source texture". */
        void SetSourceRectPx(const Vector4& rectPx);
        Vector4 GetSourceRectPx() const { return m_SourceRectPx; }

        Texture2D* GetBakedTexture() const { return m_BakedTexture.get(); }
        const std::shared_ptr<Texture2D>& GetBakedTextureShared() const { return m_BakedTexture; }
        Vector4 GetBakedRectPx() const { return m_BakedRectPx; }

        SpriteAtlas* GetAtlas() const { return m_AtlasOwner; }

        Texture2D* GetRuntimeTexture() const;
        std::shared_ptr<Texture2D> GetRuntimeTextureShared() const;
        Vector4 GetUVRectNormalized() const;

        Vector4 ResolveSourceRectPx() const;
        void ClearBaked();

        /**
         * Optional CPU pixels for Rebuild when SourceTexture has no AssetMeta path
         * (in-memory / tests). Not serialized. Copies U8 LDR pixels.
         */
        bool SetAuthoringPixels(const ImagePixels& pixels);
        void ClearAuthoringPixels();
        bool HasAuthoringPixels() const { return !m_AuthoringPixels.empty(); }
        bool TryCopyAuthoringPixels(ImagePixels& outPixels) const;

        void ConfigureFromTexture(const std::shared_ptr<Texture2D>& texture, const Vector4& sourceRectPx);

        static Vector4 MakeFullTextureRect(const Texture2D& texture);
        static Vector4 PixelRectToUvRectTopLeft(const Vector4& rectPx, uint32_t textureWidth, uint32_t textureHeight);

    private:
        friend class SpriteAtlas;

        void SetAtlasOwner(SpriteAtlas* atlas) { m_AtlasOwner = atlas; }
        void SetBaked(const std::shared_ptr<Texture2D>& texture, const Vector4& rectPx);

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSourceTexture", Getter = "GetSourceTextureShared"))
        std::shared_ptr<Texture2D> m_SourceTexture{ nullptr };

        /** (x, y, w, h) top-left; w/h <= 0 => full texture at resolve time. */
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSourceRectPx", Getter = "GetSourceRectPx"))
        Vector4 m_SourceRectPx{ 0.0f, 0.0f, 0.0f, 0.0f };

        /**
         * Bake output, runtime only. SpriteAtlas::Rebuild creates the page texture from pixels, so
         * it owns no asset identity: a persisted guid would name nothing on the next load. The rect
         * is the member's place on that page and means nothing without it, so the pair travels
         * together - neither is written to the asset file.
         */
        std::shared_ptr<Texture2D> m_BakedTexture{ nullptr };

        Vector4 m_BakedRectPx{ 0.0f, 0.0f, 0.0f, 0.0f };

        SpriteAtlas* m_AtlasOwner = nullptr;

        std::vector<uint8_t> m_AuthoringPixels;
        int32_t m_AuthoringWidth = 0;
        int32_t m_AuthoringHeight = 0;
        int32_t m_AuthoringChannels = 0;
    };
}

#include "Generated/Reflection/Sprite.gen.h"

#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Components/WidgetGraphicComponent.h"
#include "Runtime/Function/Render/Texture.h"
#include "Runtime/Function/UI/UITypes.h"

#include <memory>

namespace minEngine
{
    class Sprite;

    /**
     * Visual style for a ScreenUI widget on the same GameObject.
     * Geometry comes from WidgetComponent; without Widget this does not draw.
     * Source: Raw Texture2D (full UV) or Sprite (runtime texture + UV rect).
     */
    ME_CLASS()
    class ImageComponent : public WidgetGraphicComponent
    {
        ME_GENERATED_BODY()
    public:
        ImageComponent();
        virtual ~ImageComponent() override = default;

        void SetSourceMode(EUIImageSourceMode mode);
        EUIImageSourceMode GetSourceMode() const { return m_SourceMode; }

        void SetTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetTexture() const { return m_Texture.get(); }
        const std::shared_ptr<Texture2D>& GetTextureShared() const { return m_Texture; }

        void SetSprite(const std::shared_ptr<Sprite>& sprite);
        Sprite* GetSprite() const { return m_Sprite.get(); }
        const std::shared_ptr<Sprite>& GetSpriteShared() const { return m_Sprite; }

        /** Sets Mode=Raw and Texture in one call. */
        void SetRawTexture(const std::shared_ptr<Texture2D>& texture);

        /** Sets Mode=Sprite and Sprite in one call. */
        void SetSpriteSource(const std::shared_ptr<Sprite>& sprite);

        void SetColor(const LinearColor& color);
        LinearColor GetColor() const { return m_Color; }

        /** Draw texture for current mode; null if nothing to sample. */
        std::shared_ptr<Texture2D> ResolveDrawTextureShared() const;
        Texture2D* ResolveDrawTexture() const { return ResolveDrawTextureShared().get(); }

        /** Draw UV (u0,v0,u1,v1) OpenGL-style; degenerate when not drawable. */
        Vector4 ResolveDrawUVRect() const;

        bool CanDraw() const;

        static bool IsUVRectDegenerate(const Vector4& uvRect);

        bool CanContribute() const override;
        bool BuildDrawData(const Vector2& widgetSizePx, WidgetDrawData& outData) override;
        void ApplyMaterialParameters(Material& material) const override;

    private:
        void NotifySiblingWidgetDirty();

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSourceMode", Getter = "GetSourceMode"))
        EUIImageSourceMode m_SourceMode{ EUIImageSourceMode::Raw };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetTexture", Getter = "GetTextureShared"))
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetSprite", Getter = "GetSpriteShared"))
        std::shared_ptr<Sprite> m_Sprite{ nullptr };

        /**
         * Linear tint. Final ScreenUI opacity = texture.a * color.a (white texture if none).
         */
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColor", Getter = "GetColor"))
        LinearColor m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };
}

#include "Generated/Reflection/ImageComponent.gen.h"

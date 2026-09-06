#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Render/Texture.h"

namespace minEngine
{
    /**
     * Visual style for a ScreenUI widget on the same GameObject.
     * Geometry comes from WidgetComponent; without Widget this does not draw.
     */
    ME_CLASS()
    class ImageComponent : public Component
    {
        ME_GENERATED_BODY(ImageComponent)
    public:
        ImageComponent();
        virtual ~ImageComponent() override = default;

        void SetTexture(const std::shared_ptr<Texture2D>& texture);
        Texture2D* GetTexture() const { return m_Texture.get(); }
        const std::shared_ptr<Texture2D>& GetTextureShared() const { return m_Texture; }

        void SetColor(const LinearColor& color);
        LinearColor GetColor() const { return m_Color; }

    private:
        void NotifySiblingWidgetDirty();

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetTexture", Getter = "GetTextureShared"))
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        /**
         * Linear tint. Final ScreenUI opacity = texture.a * color.a (white texture if none).
         */
        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColor", Getter = "GetColor"))
        LinearColor m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };
}

#include "Generated/Reflection/ImageComponent.gen.h"

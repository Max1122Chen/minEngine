#pragma once

#include "Core.h"
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

        void SetColor(const Vector4& color);
        Vector4 GetColor() const { return m_Color; }

    private:
        void NotifySiblingWidgetDirty();

        ME_PROPERTY()
        std::shared_ptr<Texture2D> m_Texture{ nullptr };

        ME_PROPERTY()
        Vector4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };
}

#include "Generated/Reflection/ImageComponent.gen.h"

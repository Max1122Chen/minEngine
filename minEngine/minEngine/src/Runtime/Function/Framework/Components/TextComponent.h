#pragma once

#include "Core.h"
#include "Runtime/Core/Math/Color.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Render/RHI/RHIBuffers.h"
#include "Runtime/Function/UI/UITypes.h"
#include "Runtime/Resource/Font.h"

#include <memory>
#include <string>

namespace minEngine
{
    class FontGlyphAtlas;
    class Material;
    class Texture2D;
    class WidgetComponent;

    /**
     * ScreenUI text visual on the same GameObject as WidgetComponent.
     * Geometry comes from Widget; without Widget this does not draw.
     */
    ME_CLASS()
    class TextComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        TextComponent();
        virtual ~TextComponent() override = default;

        void SetContent(const std::string& content);
        const std::string& GetContent() const { return m_Content; }

        void SetFont(const std::shared_ptr<Font>& font);
        Font* GetFont() const { return m_Font.get(); }
        const std::shared_ptr<Font>& GetFontShared() const { return m_Font; }

        void SetFontSize(float fontSize);
        float GetFontSize() const { return m_FontSize; }

        void SetColor(const LinearColor& color);
        LinearColor GetColor() const { return m_Color; }

        void SetHorizontalAlignment(EUITextHorizontalAlignment alignment);
        EUITextHorizontalAlignment GetHorizontalAlignment() const { return m_HorizontalAlignment; }

        /** True when Content/Font/Size allow a draw attempt (mesh may still be empty). */
        bool CanAttemptDraw() const;

        bool RebuildMeshIfNeeded(const Vector2& widgetSize);
        Texture2D* GetAtlasTexture() const { return m_AtlasTexture.get(); }
        const std::shared_ptr<Texture2D>& GetAtlasTextureShared() const { return m_AtlasTexture; }
        Material* GetRuntimeMaterial() const { return m_RuntimeMaterial.get(); }
        RHIBuffer* GetVertexBuffer() const { return m_VertexBuffer.get(); }
        RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer.get(); }
        RHIVertexInputLayout* GetVertexInputLayout() const { return m_VertexInputLayout; }
        uint32_t GetQuadCount() const { return m_QuadCount; }
        bool HasDrawableMesh() const { return m_QuadCount > 0 && m_VertexBuffer && m_IndexBuffer; }

    private:
        void NotifySiblingWidgetDirty();
        void MarkTextMeshDirty();
        bool EnsureAtlas();
        bool EnsureRuntimeMaterial();

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetContent", Getter = "GetContent"))
        std::string m_Content{ "Hello UI-F04" };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetFont", Getter = "GetFontShared"))
        std::shared_ptr<Font> m_Font{ nullptr };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetFontSize", Getter = "GetFontSize"))
        float m_FontSize{ 24.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetColor", Getter = "GetColor"))
        LinearColor m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetHorizontalAlignment", Getter = "GetHorizontalAlignment"))
        EUITextHorizontalAlignment m_HorizontalAlignment{ EUITextHorizontalAlignment::Left };

        bool m_bMeshDirty{ true };
        uint32_t m_QuadCount{ 0 };
        Vector2 m_LastWidgetSize{ 0.0f, 0.0f };

        std::shared_ptr<FontGlyphAtlas> m_Atlas;
        std::shared_ptr<Texture2D> m_AtlasTexture;
        std::shared_ptr<Material> m_RuntimeMaterial;
        RHIBufferRef m_VertexBuffer;
        RHIBufferRef m_IndexBuffer;
        RHIVertexInputLayout* m_VertexInputLayout{ nullptr };
    };
}

#include "Generated/Reflection/TextComponent.gen.h"

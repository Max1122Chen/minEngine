#include "TextComponent.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlas.h"
#include "Runtime/Function/UI/Font/FontGlyphAtlasCache.h"
#include "Runtime/Function/UI/Font/TextMeshBuilder.h"
#include "Runtime/Function/UI/Font/TextVertexLayoutShared.h"

namespace minEngine
{
    TextComponent::TextComponent() = default;

    void TextComponent::SetContent(const std::string& content)
    {
        if (m_Content == content)
        {
            return;
        }
        m_Content = content;
        MarkTextMeshDirty();
        NotifySiblingWidgetDirty();
    }

    void TextComponent::SetFont(const std::shared_ptr<Font>& font)
    {
        if (m_Font == font)
        {
            return;
        }
        m_Font = font;
        m_Atlas.reset();
        m_AtlasTexture.reset();
        MarkTextMeshDirty();
        NotifySiblingWidgetDirty();
    }

    void TextComponent::SetFontSize(float fontSize)
    {
        if (m_FontSize == fontSize)
        {
            return;
        }
        m_FontSize = fontSize;
        m_Atlas.reset();
        m_AtlasTexture.reset();
        MarkTextMeshDirty();
        NotifySiblingWidgetDirty();
    }

    void TextComponent::SetColor(const LinearColor& color)
    {
        if (m_Color == color)
        {
            return;
        }
        m_Color = color;
        NotifySiblingWidgetDirty();
    }

    void TextComponent::SetHorizontalAlignment(EUITextHorizontalAlignment alignment)
    {
        if (m_HorizontalAlignment == alignment)
        {
            return;
        }
        m_HorizontalAlignment = alignment;
        MarkTextMeshDirty();
        NotifySiblingWidgetDirty();
    }

    bool TextComponent::CanAttemptDraw() const
    {
        // A style source that is switched off contributes nothing, even with valid data: the
        // widget beside it assembles the picture, so its own switch has to reach this gate.
        if (!IsActive())
        {
            return false;
        }

        return m_Font && m_Font->IsValid() && m_FontSize > 0.0f;
    }

    void TextComponent::MarkTextMeshDirty()
    {
        m_bMeshDirty = true;
    }

    void TextComponent::NotifySiblingWidgetDirty()
    {
        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return;
        }

        for (const std::shared_ptr<WidgetComponent>& widget : owner->GetComponentsOfType<WidgetComponent>())
        {
            if (widget)
            {
                widget->MarkRenderStateDirty();
            }
        }
    }

    bool TextComponent::EnsureAtlas()
    {
        if (m_Atlas && m_Atlas->IsValid())
        {
            return true;
        }
        if (!CanAttemptDraw())
        {
            return false;
        }

        m_Atlas = FontGlyphAtlasCache::Get().GetOrCreate(*m_Font, m_FontSize);
        return m_Atlas && m_Atlas->IsValid();
    }

    bool TextComponent::CanContribute() const
    {
        return CanAttemptDraw();
    }

    bool TextComponent::BuildDrawData(const Vector2& widgetSizePx, WidgetDrawData& outData)
    {
        // The mesh is in widget-local pixels, so it has to be rebuilt for the size it is drawn at.
        if (!RebuildMeshIfNeeded(widgetSizePx) || m_AtlasTexture == nullptr)
        {
            return false;
        }

        // One glyph quad is four vertices and two triangles; the mesh builder emits exactly that
        // per quad, so the totals follow from the quad count without re-reading the buffers.
        constexpr uint32_t kVerticesPerQuad = 4u;
        constexpr uint32_t kIndicesPerQuad = 6u;

        outData.bLocalPixelMesh = true;
        outData.VertexBuffer = m_VertexBuffer;
        outData.IndexBuffer = m_IndexBuffer;
        // Text reads its vertices through one layout the whole process shares, and the cache that
        // owns it hands out a handle: passing that handle on is what lets a proxy outlive this
        // graphic and still name a layout that exists.
        outData.VertexInputLayoutRef = SharedTextVertexInputLayout();
        outData.VertexInputLayout = outData.VertexInputLayoutRef.get();
        outData.IndexCount = m_QuadCount * kIndicesPerQuad;
        outData.VertexCount = m_QuadCount * kVerticesPerQuad;
        outData.Texture = m_AtlasTexture.get();
        outData.Color = m_Color.ToVector4();
        // Glyph quads carry their own atlas coordinates, so the draw covers the whole texture.
        outData.UVRect = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        return true;
    }

    void TextComponent::ApplyMaterialParameters(Material& material) const
    {
        if (!RenderSystem::HasInstance())
        {
            return;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr || m_AtlasTexture == nullptr)
        {
            return;
        }

        ScreenUIMaterialFactory::ApplyColorAndTexture(material, m_Color, m_AtlasTexture, *rhi);
    }

    bool TextComponent::RebuildMeshIfNeeded(const Vector2& widgetSize)
    {
        if (!CanAttemptDraw())
        {
            m_QuadCount = 0;
            m_VertexBuffer.reset();
            m_IndexBuffer.reset();
            return false;
        }

        if (!m_bMeshDirty && m_LastWidgetSize == widgetSize && HasDrawableMesh())
        {
            return HasDrawableMesh();
        }

        if (!EnsureAtlas())
        {
            ME_LOG(LogUI, Warn, "TextComponent: failed to create glyph atlas.");
            m_QuadCount = 0;
            return false;
        }

        if (!RenderSystem::HasInstance())
        {
            return false;
        }
        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            return false;
        }

        TextMeshBuildResult mesh;
        if (!TextMeshBuilder::Build(*m_Atlas, m_Content, widgetSize, m_HorizontalAlignment, mesh))
        {
            m_QuadCount = 0;
            m_VertexBuffer.reset();
            m_IndexBuffer.reset();
            m_bMeshDirty = false;
            m_LastWidgetSize = widgetSize;
            return false;
        }

        m_AtlasTexture = m_Atlas->GetOrCreateTexture(*rhi);
        if (!m_AtlasTexture)
        {
            m_QuadCount = 0;
            return false;
        }

        if (mesh.QuadCount == 0)
        {
            m_QuadCount = 0;
            m_VertexBuffer.reset();
            m_IndexBuffer.reset();
            m_bMeshDirty = false;
            m_LastWidgetSize = widgetSize;
            return false;
        }

        RHIBufferCreateDesc vbDesc;
        vbDesc.Usage = RHIBufferUsage::Vertex;
        vbDesc.ByteSize = static_cast<uint32_t>(mesh.InterleavedVertices.size() * sizeof(float));
        vbDesc.ElementCount = mesh.QuadCount * 4u;
        m_VertexBuffer = rhi->RHICreateBuffer(vbDesc, mesh.InterleavedVertices.data());

        RHIBufferCreateDesc ibDesc;
        ibDesc.Usage = RHIBufferUsage::Index;
        ibDesc.ByteSize = static_cast<uint32_t>(mesh.Indices.size() * sizeof(uint32_t));
        ibDesc.ElementCount = static_cast<uint32_t>(mesh.Indices.size());
        m_IndexBuffer = rhi->RHICreateBuffer(ibDesc, mesh.Indices.data());

        m_VertexInputLayout = TextMeshBuilder::GetOrCreateVertexInputLayout(*rhi);
        m_QuadCount = mesh.QuadCount;
        m_bMeshDirty = false;
        m_LastWidgetSize = widgetSize;
        return HasDrawableMesh();
    }
}

#include "ImageComponent.h"

#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Render/Material.h"
#include "Runtime/Function/Render/RenderSystem.h"
#include "Runtime/Function/Render/RHI/RHI.h"
#include "Runtime/Function/Render/ScreenUI/ScreenUIMaterialFactory.h"
#include "Runtime/Function/Render/Sprite/Sprite.h"

#include <cmath>

namespace minEngine
{
    ImageComponent::ImageComponent() = default;

    void ImageComponent::SetSourceMode(EUIImageSourceMode mode)
    {
        if (m_SourceMode == mode)
        {
            return;
        }
        m_SourceMode = mode;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetTexture(const std::shared_ptr<Texture2D>& texture)
    {
        if (m_Texture == texture)
        {
            return;
        }
        m_Texture = texture;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetSprite(const std::shared_ptr<Sprite>& sprite)
    {
        if (m_Sprite == sprite)
        {
            return;
        }
        m_Sprite = sprite;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetRawTexture(const std::shared_ptr<Texture2D>& texture)
    {
        const bool modeChanged = m_SourceMode != EUIImageSourceMode::Raw;
        const bool textureChanged = m_Texture != texture;
        if (!modeChanged && !textureChanged)
        {
            return;
        }
        m_SourceMode = EUIImageSourceMode::Raw;
        m_Texture = texture;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetSpriteSource(const std::shared_ptr<Sprite>& sprite)
    {
        const bool modeChanged = m_SourceMode != EUIImageSourceMode::Sprite;
        const bool spriteChanged = m_Sprite != sprite;
        if (!modeChanged && !spriteChanged)
        {
            return;
        }
        m_SourceMode = EUIImageSourceMode::Sprite;
        m_Sprite = sprite;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetColor(const LinearColor& color)
    {
        if (m_Color == color)
        {
            return;
        }
        m_Color = color;
        NotifySiblingWidgetDirty();
    }

    std::shared_ptr<Texture2D> ImageComponent::ResolveDrawTextureShared() const
    {
        if (m_SourceMode == EUIImageSourceMode::Sprite)
        {
            if (m_Sprite)
            {
                return m_Sprite->GetRuntimeTextureShared();
            }
            return nullptr;
        }

        return m_Texture;
    }

    Vector4 ImageComponent::ResolveDrawUVRect() const
    {
        if (m_SourceMode == EUIImageSourceMode::Sprite)
        {
            if (m_Sprite)
            {
                return m_Sprite->GetUVRectNormalized();
            }
            return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
        }

        return Vector4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    bool ImageComponent::CanDraw() const
    {
        // A style source that is switched off contributes nothing, even with valid data: the
        // widget beside it assembles the picture, so its own switch has to reach this gate.
        if (!IsActive())
        {
            return false;
        }

        const std::shared_ptr<Texture2D> texture = ResolveDrawTextureShared();
        if (texture == nullptr)
        {
            // Raw mode with null texture still draws white via material factory.
            return m_SourceMode == EUIImageSourceMode::Raw;
        }
        return !IsUVRectDegenerate(ResolveDrawUVRect());
    }

    bool ImageComponent::IsUVRectDegenerate(const Vector4& uvRect)
    {
        constexpr float kEps = 1.0e-6f;
        return std::fabs(uvRect.z - uvRect.x) < kEps || std::fabs(uvRect.w - uvRect.y) < kEps;
    }

    bool ImageComponent::CanContribute() const
    {
        return CanDraw();
    }

    bool ImageComponent::BuildDrawData(const Vector2& widgetSizePx, WidgetDrawData& outData)
    {
        // The image fills the widget rect with the shared quad, so the widget size is not needed.
        (void)widgetSizePx;

        if (!CanDraw())
        {
            return false;
        }

        outData.bLocalPixelMesh = false;
        outData.VertexBuffer = nullptr;
        outData.IndexBuffer = nullptr;
        outData.VertexInputLayout = nullptr;
        outData.Texture = ResolveDrawTexture();
        outData.Color = m_Color.ToVector4();
        outData.UVRect = ResolveDrawUVRect();
        return true;
    }

    void ImageComponent::ApplyMaterialParameters(Material& material) const
    {
        if (!RenderSystem::HasInstance())
        {
            return;
        }

        RHI* rhi = RenderSystem::Get().GetRHI();
        if (rhi == nullptr)
        {
            return;
        }

        ScreenUIMaterialFactory::ApplyColorTextureAndUVRect(
            material,
            m_Color,
            ResolveDrawTextureShared(),
            ResolveDrawUVRect(),
            *rhi);
    }

    void ImageComponent::NotifySiblingWidgetDirty()
    {
        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return;
        }

        const std::vector<std::shared_ptr<WidgetComponent>> widgets =
            owner->GetComponentsOfType<WidgetComponent>();
        for (const std::shared_ptr<WidgetComponent>& widget : widgets)
        {
            if (widget)
            {
                widget->MarkRenderStateDirty();
            }
        }
    }
}

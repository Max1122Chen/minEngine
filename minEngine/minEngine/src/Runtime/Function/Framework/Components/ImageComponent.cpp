#include "ImageComponent.h"

#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    ImageComponent::ImageComponent() = default;

    void ImageComponent::SetTexture(const std::shared_ptr<Texture2D>& texture)
    {
        if (m_Texture == texture)
        {
            return;
        }
        m_Texture = texture;
        NotifySiblingWidgetDirty();
    }

    void ImageComponent::SetColor(const Vector4& color)
    {
        if (m_Color == color)
        {
            return;
        }
        m_Color = color;
        NotifySiblingWidgetDirty();
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

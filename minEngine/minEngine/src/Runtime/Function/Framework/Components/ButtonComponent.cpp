#include "ButtonComponent.h"

#include "Runtime/Function/Framework/Components/ImageComponent.h"
#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/UI/UISystem.h"

namespace minEngine
{
    ButtonComponent::ButtonComponent()
    {
        m_bCanEverTick = true;
    }

    void ButtonComponent::Tick(float deltaTime)
    {
        (void)deltaTime;
        if (!IsActive())
        {
            return;
        }

        ApplyVisualState();
    }

    void ButtonComponent::OnActivate()
    {
        Component::OnActivate();
        ApplyVisualState();
    }

    void ButtonComponent::OnDeactivate()
    {
        ApplyTint(m_NormalColor);
        Component::OnDeactivate();
    }

    void ButtonComponent::NotifyClicked()
    {
        if (!CanAcceptClick())
        {
            return;
        }

        m_OnClicked.Broadcast();
    }

    void ButtonComponent::SetInteractable(bool interactable)
    {
        if (m_bInteractable == interactable)
        {
            return;
        }

        m_bInteractable = interactable;
        ApplyVisualState();
    }

    void ButtonComponent::SetTargetGraphic(const std::shared_ptr<ImageComponent>& image)
    {
        m_TargetGraphic = image;
        ApplyVisualState();
    }

    ImageComponent* ButtonComponent::ResolveTargetGraphic() const
    {
        if (m_TargetGraphic)
        {
            return m_TargetGraphic.get();
        }

        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<ImageComponent>> images =
            owner->GetComponentsOfType<ImageComponent>();
        if (images.empty() || !images[0])
        {
            return nullptr;
        }

        return images[0].get();
    }

    WidgetComponent* ButtonComponent::FindWidget() const
    {
        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<WidgetComponent>> widgets =
            owner->GetComponentsOfType<WidgetComponent>();
        if (widgets.empty() || !widgets[0])
        {
            return nullptr;
        }

        return widgets[0].get();
    }

    bool ButtonComponent::CanAcceptClick() const
    {
        if (!IsActive() || !m_bInteractable)
        {
            return false;
        }

        WidgetComponent* widget = FindWidget();
        if (widget == nullptr || !widget->IsActive() || !widget->IsHitTestVisible())
        {
            return false;
        }

        return true;
    }

    void ButtonComponent::SetColorTintTransition(bool enabled)
    {
        m_bColorTintTransition = enabled;
        ApplyVisualState();
    }

    void ButtonComponent::SetNormalColor(const LinearColor& color)
    {
        m_NormalColor = color;
        ApplyVisualState();
    }

    void ButtonComponent::SetHighlightedColor(const LinearColor& color)
    {
        m_HighlightedColor = color;
        ApplyVisualState();
    }

    void ButtonComponent::SetPressedColor(const LinearColor& color)
    {
        m_PressedColor = color;
        ApplyVisualState();
    }

    void ButtonComponent::SetDisabledColor(const LinearColor& color)
    {
        m_DisabledColor = color;
        ApplyVisualState();
    }

    void ButtonComponent::ApplyVisualState()
    {
        if (!m_bColorTintTransition)
        {
            return;
        }

        if (!IsActive() || !m_bInteractable)
        {
            ApplyTint(m_DisabledColor);
            return;
        }

        WidgetComponent* widget = FindWidget();
        if (widget == nullptr)
        {
            ApplyTint(m_NormalColor);
            return;
        }

        if (!UISystem::HasInstance() || !UISystem::Get().IsPointerRoutingEnabled())
        {
            ApplyTint(m_NormalColor);
            return;
        }

        const ScreenUIPointerState& pointer = UISystem::Get().GetPointerState();
        if (pointer.Pressed == widget)
        {
            ApplyTint(m_PressedColor);
            return;
        }

        if (pointer.Hovered == widget)
        {
            ApplyTint(m_HighlightedColor);
            return;
        }

        ApplyTint(m_NormalColor);
    }

    void ButtonComponent::ApplyTint(const LinearColor& color)
    {
        ImageComponent* image = ResolveTargetGraphic();
        if (image == nullptr)
        {
            return;
        }

        image->SetColor(color);
    }
}
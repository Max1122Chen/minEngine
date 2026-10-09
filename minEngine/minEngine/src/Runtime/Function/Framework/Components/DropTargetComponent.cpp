#include "DropTargetComponent.h"

#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    DropTargetComponent::DropTargetComponent() = default;

    void DropTargetComponent::SetInteractable(bool interactable)
    {
        m_bInteractable = interactable;
    }

    WidgetComponent* DropTargetComponent::FindWidget() const
    {
        GameObject* owner = GetOwner();
        if (owner == nullptr)
        {
            return nullptr;
        }

        const std::vector<std::shared_ptr<WidgetComponent>> widgets =
            owner->GetComponentsOfType<WidgetComponent>();
        for (const std::shared_ptr<WidgetComponent>& widget : widgets)
        {
            if (widget)
            {
                return widget.get();
            }
        }
        return nullptr;
    }

    bool DropTargetComponent::CanAcceptDrop(const ScreenUIDragSession& session) const
    {
        (void)session;
        return IsActive() && m_bInteractable && FindWidget() != nullptr;
    }

    void DropTargetComponent::NotifyDrop()
    {
        m_OnDrop.Broadcast();
    }
}

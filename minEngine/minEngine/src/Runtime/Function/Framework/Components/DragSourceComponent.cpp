#include "DragSourceComponent.h"

#include "Runtime/Function/Framework/Components/WidgetComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/UI/UISystem.h"

namespace minEngine
{
    DragSourceComponent::DragSourceComponent() = default;

    void DragSourceComponent::SetInteractable(bool interactable)
    {
        m_bInteractable = interactable;
    }

    void DragSourceComponent::SetDragThresholdPx(float thresholdPx)
    {
        m_DragThresholdPx = thresholdPx;
    }

    void DragSourceComponent::SetUserId(int32_t userId)
    {
        m_UserId = userId;
    }

    WidgetComponent* DragSourceComponent::FindWidget() const
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

    bool DragSourceComponent::CanStartDrag() const
    {
        return IsActive() && m_bInteractable && FindWidget() != nullptr;
    }

    void DragSourceComponent::NotifyDragBegin()
    {
        m_OnDragBegin.Broadcast();
    }

    void DragSourceComponent::NotifyDrag()
    {
        m_OnDrag.Broadcast();
    }

    void DragSourceComponent::NotifyDragEnd()
    {
        m_OnDragEnd.Broadcast();
    }

    void DragSourceComponent::NotifyDragCancel()
    {
        m_OnDragCancel.Broadcast();
    }

    bool DragSourceComponent::ConsumeLastEndSuccess()
    {
        const bool success = m_bLastEndSuccess;
        m_bLastEndSuccess = false;
        return success;
    }

    void DragSourceComponent::SetLastEndSuccess(bool success)
    {
        m_bLastEndSuccess = success;
    }

    void DragSourceComponent::OnDeactivate()
    {
        if (UISystem::HasInstance())
        {
            UISystem::Get().CancelDragIfSource(this);
        }
        Component::OnDeactivate();
    }
}

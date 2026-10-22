#pragma once

#include "Core.h"
#include "Runtime/Core/Delegates/DelegateMacros.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <cstdint>

namespace minEngine
{
    class WidgetComponent;

    DECLARE_MULTICAST_DELEGATE(DOnUIDrag);

    /**
     * Marks a ScreenUI Widget GO as a drag source.
     * Session / threshold / click suppress are owned by UISystem.
     * Listeners use UISystem::GetDragSession() for pointer / UserId context.
     */
    ME_CLASS()
    class MINENGINE_API DragSourceComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        DragSourceComponent();
        virtual ~DragSourceComponent() override = default;

        DOnUIDrag& OnDragBegin() { return m_OnDragBegin; }
        const DOnUIDrag& OnDragBegin() const { return m_OnDragBegin; }
        DOnUIDrag& OnDrag() { return m_OnDrag; }
        const DOnUIDrag& OnDrag() const { return m_OnDrag; }
        DOnUIDrag& OnDragEnd() { return m_OnDragEnd; }
        const DOnUIDrag& OnDragEnd() const { return m_OnDragEnd; }
        DOnUIDrag& OnDragCancel() { return m_OnDragCancel; }
        const DOnUIDrag& OnDragCancel() const { return m_OnDragCancel; }

        bool IsInteractable() const { return m_bInteractable; }
        void SetInteractable(bool interactable);

        /** <= 0 uses UISystem default threshold. */
        float GetDragThresholdPx() const { return m_DragThresholdPx; }
        void SetDragThresholdPx(float thresholdPx);

        int32_t GetUserId() const { return m_UserId; }
        void SetUserId(int32_t userId);

        WidgetComponent* FindWidget() const;
        bool CanStartDrag() const;

        void NotifyDragBegin();
        void NotifyDrag();
        void NotifyDragEnd();
        void NotifyDragCancel();

        bool ConsumeLastEndSuccess();
        void SetLastEndSuccess(bool success);

    protected:
        void OnDeactivate() override;

    private:
        DOnUIDrag m_OnDragBegin;
        DOnUIDrag m_OnDrag;
        DOnUIDrag m_OnDragEnd;
        DOnUIDrag m_OnDragCancel;

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetInteractable", Getter = "IsInteractable"))
        bool m_bInteractable{ true };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetDragThresholdPx", Getter = "GetDragThresholdPx"))
        float m_DragThresholdPx{ 0.0f };

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetUserId", Getter = "GetUserId"))
        int32_t m_UserId{ 0 };

        bool m_bLastEndSuccess = false;
    };
}

#include "Generated/Reflection/DragSourceComponent.gen.h"

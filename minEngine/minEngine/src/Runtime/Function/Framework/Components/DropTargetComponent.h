#pragma once

#include "Core.h"
#include "Runtime/Core/Delegates/DelegateMacros.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/UI/ScreenUIDragTypes.h"

namespace minEngine
{
    class WidgetComponent;

    DECLARE_MULTICAST_DELEGATE(DOnUIDrop);

    /**
     * Marks a ScreenUI Widget GO as a drop target.
     * Listeners use UISystem::GetDragSession() for source / UserId context.
     */
    ME_CLASS()
    class MINENGINE_API DropTargetComponent : public Component
    {
        ME_GENERATED_BODY()
    public:
        DropTargetComponent();
        virtual ~DropTargetComponent() override = default;

        DOnUIDrop& OnDrop() { return m_OnDrop; }
        const DOnUIDrop& OnDrop() const { return m_OnDrop; }

        bool IsInteractable() const { return m_bInteractable; }
        void SetInteractable(bool interactable);

        WidgetComponent* FindWidget() const;

        /** Default true when interactable and a sibling Widget exists. */
        virtual bool CanAcceptDrop(const ScreenUIDragSession& session) const;

        void NotifyDrop();

    private:
        DOnUIDrop m_OnDrop;

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetInteractable", Getter = "IsInteractable"))
        bool m_bInteractable{ true };
    };
}

#include "Generated/Reflection/DropTargetComponent.gen.h"

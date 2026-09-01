#pragma once
#include "Core.h"
#include "Core/Object/MEObject.h"

namespace minEngine
{
    class GameObject;

    enum ComponentMarkedForNeededEndOfFrameUpdate
    {
        Marked,
        Unmarked
    };

    /**
     * @brief
     * Base class for all components that can be attached to GameObjects.
     */
    ME_CLASS(ScriptType)
    class Component : public MEObject
    {
        ME_GENERATED_BODY(Component)
    public:
        Component();
        virtual ~Component();

        virtual void Tick(float deltaTime) {}

        void SetActive(bool active);
        bool IsActive() const { return m_bActive; }

        /** Reconcile runtime activation after direct writes to m_bActive (e.g. undo/redo). */
        void SyncActivationWithActiveFlag();

        /** Called after scene load / owner attach when a pending activation can be fulfilled. */
        void ResolvePendingActivation();

        virtual void SetOwner(GameObject* inOwner);
        ME_FUNCTION(ScriptCallable)
        GameObject* GetOwner() const { return m_Owner; }

        bool CanEverTick() const { return m_bCanEverTick; }

        void MarkForNeededEndOfFrameUpdate();
        uint32_t GetMarkedForNeededEndOfFrameUpdate() const { return m_MarkedForNeededEndOfFrameUpdate; }
        void SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate marked) { m_MarkedForNeededEndOfFrameUpdate = marked; }
        virtual void DoEndOfFrameUpdate() {}

    protected:
        virtual void OnActivate();
        virtual void OnDeactivate();

        /** Register with runtime systems (physics, audio, render, ...). */
        virtual void ApplyActivationToSystems();
        /** Unregister from runtime systems. */
        virtual void RemoveActivationFromSystems();

        ME_PROPERTY(Invisible)
        GameObject* m_Owner{ nullptr };

        ME_PROPERTY(Invisible)
        bool m_bActive{ true };

        bool m_bCanEverTick{ true };
        ComponentMarkedForNeededEndOfFrameUpdate m_MarkedForNeededEndOfFrameUpdate{ Unmarked };

    private:
        bool CanApplyActivation() const;
        void TryActivate();
        void Deactivate();

        bool m_bPendingActivation{ false };
        bool m_bActivationApplied{ false };
    };
}

#include "Generated/Reflection/Component.gen.h"

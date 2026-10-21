#pragma once
#include "Core.h"
#include "Core/Object/MEObject.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    class GameObject;
    class Scene;
    class RenderScene;

    enum ComponentMarkedForNeededEndOfFrameUpdate
    {
        Marked,
        Unmarked
    };

    /**
     * Cached effective-state bit.
     * Unknown = not computed since the last invalidation. Not serialized, never authored.
     */
    constexpr uint8_t kEffectiveStateUnknown = 0;
    constexpr uint8_t kEffectiveStateFalse = 1;
    constexpr uint8_t kEffectiveStateTrue = 2;

    /**
     * @brief
     * Base class for all components that can be attached to GameObjects.
     */
    ME_CLASS(ScriptType, Abstract)
    class Component : public MEObject
    {
        ME_GENERATED_BODY()
    public:
        Component();
        virtual ~Component();

        virtual void Tick(float deltaTime) {}

        /** Opt into FixedTick; aggregates to owning GameObject. */
        void SetFixedTickEnabled(bool enabled);
        bool IsFixedTickEnabled() const { return m_bFixedTickEnabled; }
        virtual bool WantsFixedTick() const { return m_bFixedTickEnabled; }
        virtual void FixedTick(float fixedDeltaSeconds) { (void)fixedDeltaSeconds; }

        /** Writes the AUTHORED flag only; the effective state and its reconcile follow. */
        void SetActive(bool active);

        /**
         * Effective state: self flag AND the owning GameObject chain.
         * O(1): a cache read. The ancestor term is published by the owning GameObject whenever the
         * chain changes, so a query never walks the parent chain and never allocates.
         */
        bool IsActive() const
        {
            if (m_EffectiveActiveCache == kEffectiveStateUnknown)
            {
                m_EffectiveActiveCache = (IsActiveSelf() && m_bOwnerEffectiveActive)
                    ? kEffectiveStateTrue
                    : kEffectiveStateFalse;
            }
            return m_EffectiveActiveCache == kEffectiveStateTrue;
        }

        /** The author's own flag. Only the activation mechanism and the inspector may read it. */
        bool IsActiveSelf() const { return m_bActive; }

        /** Explicit alias of IsActive(), for call sites that mean the hierarchy result. */
        bool IsActiveInHierarchy() const { return IsActive(); }

        /** Reconcile the runtime activation state against the effective state. */
        void ReconcileEffectiveActivation() { SyncActivationWithActiveFlag(); }

        /**
         * Exported spelling of ReconcileEffectiveActivation(). Kept by name because the symbol is
         * frozen in cmake/minEngine_exports.def and the Editor DLL links against it.
         */
        void SyncActivationWithActiveFlag();

        /** Called after scene load / owner attach when a pending activation can be fulfilled. */
        void ResolvePendingActivation();

        virtual void SetOwner(GameObject* inOwner);
        ME_FUNCTION(ScriptCallable)
        GameObject* GetOwner() const { return m_Owner; }

        /** Owning Scene via GameObject Outer; nullptr if not yet in a Scene. */
        Scene* GetOwningScene() const;
        /**
         * RenderScene of the owning Scene (Ensures RenderScene exists).
         * nullptr if component is not in a Scene.
         */
        RenderScene* GetOwningRenderScene() const;
        /** Existing RenderScene only; does not create. Prefer for Remove/dtor paths. */
        RenderScene* GetOwningRenderSceneIfPresent() const;

        void Rename(const std::string& newName) { SetName(newName); }

        bool CanEverTick() const { return m_bCanEverTick; }

        /**
         * Convergence observability: how many times this component registered with / unregistered
         * from runtime systems. A steady-state reconcile must not move either counter.
         */
        uint32_t GetActivationApplyCount() const { return m_ActivationApplyCount; }
        uint32_t GetActivationRemoveCount() const { return m_ActivationRemoveCount; }

        void MarkForNeededEndOfFrameUpdate();
        uint32_t GetMarkedForNeededEndOfFrameUpdate() const { return m_MarkedForNeededEndOfFrameUpdate; }
        void SetMarkedForNeededEndOfFrameUpdate(ComponentMarkedForNeededEndOfFrameUpdate marked) { m_MarkedForNeededEndOfFrameUpdate = marked; }
        virtual void DoEndOfFrameUpdate() {}

        /** Simulation lifecycle. Orthogonal to Activate. */
        void BeginPlay();
        void EndPlay(EEndPlayReason reason);
        bool HasBegunPlay() const { return m_bHasBegunPlay; }

    protected:
        virtual void OnActivate();
        virtual void OnDeactivate();
        virtual void OnBeginPlay() {}
        virtual void OnEndPlay(EEndPlayReason reason) { (void)reason; }

        /** Register with runtime systems (physics, audio, render, ...). */
        virtual void ApplyActivationToSystems();
        /** Unregister from runtime systems. */
        virtual void RemoveActivationFromSystems();

        ME_PROPERTY(Invisible, meta = (Setter = "SetOwner", Getter = "GetOwner"))
        GameObject* m_Owner{ nullptr };

        /** AUTHORED active flag. Effective state = this AND the owning GameObject chain. */
        ME_PROPERTY(Invisible)
        bool m_bActive{ true };

        bool m_bCanEverTick{ true };
        bool m_bFixedTickEnabled{ false };
        ComponentMarkedForNeededEndOfFrameUpdate m_MarkedForNeededEndOfFrameUpdate{ Unmarked };

    private:
        friend class GameObject;

        /**
         * Publishes the ancestor-chain contribution. Called by the owning GameObject during
         * push-down; never writes any authored flag.
         */
        void PublishOwnerEffectiveActive(bool ownerActive)
        {
            m_bOwnerEffectiveActive = ownerActive;
            m_EffectiveActiveCache = kEffectiveStateUnknown;
        }

        bool CanApplyActivation() const;
        void TryActivate();
        void Deactivate();
        void TryBeginPlayIfSimulating();

        bool m_bPendingActivation{ false };
        bool m_bActivationApplied{ false };
        /** Last reconciled system-side state; the delta the reconciler compares against. */
        bool m_bAppliedEffective{ false };
        bool m_bHasBegunPlay{ false };

        /** Ancestor-chain contribution, maintained by the owning GameObject. Not serialized. */
        bool m_bOwnerEffectiveActive{ true };
        mutable uint8_t m_EffectiveActiveCache{ kEffectiveStateUnknown };
        uint32_t m_ActivationApplyCount{ 0 };
        uint32_t m_ActivationRemoveCount{ 0 };
    };
}

#include "Generated/Reflection/Component.gen.h"
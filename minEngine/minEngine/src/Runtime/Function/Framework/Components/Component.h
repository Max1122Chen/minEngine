#pragma once
#include "Core.h"
#include "Core/Object/MEObject.h"

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

        void SetActive(bool active);
        bool IsActive() const { return m_bActive; }

        /** Reconcile runtime activation after direct writes to m_bActive (e.g. undo/redo). */
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

        ME_PROPERTY(Invisible, meta = (Setter = "SetOwner", Getter = "GetOwner"))
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

#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "TypeTraits.h"

#include <algorithm>

namespace minEngine
{
    struct Transform;

    class Component;
    class SceneComponent;

    ME_CLASS(ScriptType)
    class MINENGINE_API GameObject : public MEObject
    {
        ME_GENERATED_BODY()
    public:
        GameObject();
        virtual ~GameObject();

        void Tick(float deltaTime);

        /** Forwards to Components with FixedTick enabled. */
        void FixedTick(float fixedDeltaSeconds);
        bool WantsFixedTick() const { return m_FixedTickComponentCount > 0; }
        void NotifyFixedTickComponentChanged(bool componentEnabled);

        /** Routes to Components then OnBeginPlay. */
        void BeginPlay();
        void EndPlay(EEndPlayReason reason);
        bool HasBegunPlay() const { return m_bHasBegunPlay; }

        bool IsPendingDestroy() const { return m_bPendingDestroy; }
        void MarkPendingDestroy() { m_bPendingDestroy = true; }

        uint64_t GetID() const { return m_ID; }
        void SetID(uint64_t id) { m_ID = id; }

        void Rename(const std::string& newName) { SetName(newName); }

        Transform GetTransform();
        void SetTransform(const Transform& inTransform);
        Transform GetWorldTransform() const;
        void SetWorldTransform(const Transform& worldTransform);

        ME_FUNCTION(ScriptCallable)
        Vector3 GetPosition();
        ME_FUNCTION(ScriptCallable)
        void SetPosition(const Vector3& position);
        ME_FUNCTION(ScriptCallable)
        void Translate(const Vector3& delta);

        ME_FUNCTION(ScriptCallable)
        Component* FindComponentByClassName(const std::string& componentClassName) const;

        Quaternion GetRotation();
        void SetRotation(const Quaternion& rotation);
        Vector3 GetRotationEulerDegrees();
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees);
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local);

        Vector3 GetScale();
        void SetScale(const Vector3& scale);
        void ScaleBy(const Vector3& scaleFactor);

    
        SceneComponent* GetRootComponent() const { return m_RootComponent; }
        /** swapping Root rebinds GO-parent Root↔Root (KeepWorld) or dissolves edges. */
        void SetRootComponent(SceneComponent* rootComponent);
        std::vector<std::shared_ptr<Component>>& GetAllComponents() { return m_Components; }

        // just a simple implementation for demo purposes
        template<typename T>
        std::vector<std::shared_ptr<T>> GetComponentsOfType()
        {
            std::vector<std::shared_ptr<T>> result;
            for (auto& component : m_Components)
            {
                // Entries may be null mid-destruction (SetOwner(nullptr) then reset).
                if (!component || component->GetClass() == nullptr)
                {
                    continue;
                }

                if (component->GetClass()->IsA(T::StaticClass()))
                {
                    result.push_back(std::static_pointer_cast<T>(component));
                }
            }
            return result;
        }
        
        template<typename T>
        std::shared_ptr<T> AddComponent()
        {
            static_assert(std::is_base_of_v<minEngine::Component,T>,"Tried to use AddComponent<T> with a non-component type!!!");
            std::shared_ptr<T> newComponentBase = NewObject<T>("",this);
            std::shared_ptr<Component> newComponent = std::static_pointer_cast<Component>(newComponentBase);
            AddComponent_Internal(newComponent);
            return newComponentBase;
        }

        std::shared_ptr<Component> AddComponent(const std::string& componentTypeName);

        bool RemoveComponent(Component& target);

        /** Reorder m_Components. Root SceneComponent cannot be moved. Returns false on failure. */
        bool MoveComponent(Component& target, size_t newIndex);
        size_t FindComponentIndex(const Component& target) const;

        void InsertRestoredComponent(std::shared_ptr<Component> component, size_t index);

        /**
         * GO parent means child Root attaches under parent Root.
         * Both GOs must have a Root SceneComponent; otherwise returns false.
         */
        bool AttachToParent(GameObject* parent, AttachmentTransformRules rules);
        void DetachFromParent(AttachmentTransformRules rules);
        GameObject* GetParent() const { return m_Parent; }
        const std::vector<GameObject*>& GetChildren() const { return m_Children; }

        /** Clear m_Children only (keeps serialized m_Parent for Resolve). */
        void ClearChildrenLinks();

        /**
         * Drops this node's GameObject-parent link without notifying anything.
         * Teardown paths only: no transform cascade, no render-dirty, no singleton access, so it
         * stays usable while the object graph is being taken apart.
         */
        void ClearGameObjectParentLink()
        {
            if (m_Parent == nullptr)
            {
                return;
            }

            std::vector<GameObject*>& siblings = m_Parent->m_Children;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            m_Parent = nullptr;
        }

        // ---- Authored activity / visibility axes and their effective state ----

        /** Writes the AUTHORED flag of this node only; the subtree is invalidated and reconciled. */
        void SetActive(bool active)
        {
            if (IsActiveSelf() == active)
            {
                return;
            }

            m_bActive = active;
            PropagateEffectiveStateChange();
        }

        /** Writes the AUTHORED flag of this node only; the subtree is invalidated and reconciled. */
        void SetVisible(bool visible)
        {
            if (IsVisibleSelf() == visible)
            {
                return;
            }

            m_bVisible = visible;
            PropagateEffectiveStateChange();
        }

        /** Effective state: this node's flag AND the parent GameObject chain. Cached, O(1). */
        bool IsActive() const
        {
            if (m_EffectiveActiveCache == kEffectiveStateUnknown)
            {
                const bool parentActive = (m_Parent != nullptr) ? m_Parent->IsActive() : true;
                m_EffectiveActiveCache = (m_bActive && parentActive)
                    ? kEffectiveStateTrue
                    : kEffectiveStateFalse;
            }
            return m_EffectiveActiveCache == kEffectiveStateTrue;
        }

        /** Effective state on the presentation axis. Cached, O(1). */
        bool IsVisible() const
        {
            if (m_EffectiveVisibleCache == kEffectiveStateUnknown)
            {
                const bool parentVisible = (m_Parent != nullptr) ? m_Parent->IsVisible() : true;
                m_EffectiveVisibleCache = (m_bVisible && parentVisible)
                    ? kEffectiveStateTrue
                    : kEffectiveStateFalse;
            }
            return m_EffectiveVisibleCache == kEffectiveStateTrue;
        }

        /** The author's own flag. */
        bool IsActiveSelf() const { return m_bActive; }
        /** The author's own flag. */
        bool IsVisibleSelf() const { return m_bVisible; }

        /** Explicit alias of IsActive(), for call sites that mean the hierarchy result. */
        bool IsActiveInHierarchy() const { return IsActive(); }
        /** Explicit alias of IsVisible(), for call sites that mean the hierarchy result. */
        bool IsVisibleInHierarchy() const { return IsVisible(); }

        /**
         * Push the caches of this subtree to Unknown without evaluating, so a later pass can
         * never read an ancestor value computed from a half-resolved hierarchy. Touches no
         * authored flag. Used by the two-phase reconcile after deserialization.
         */
        void InvalidateEffectiveStateSubtree()
        {
            m_EffectiveActiveCache = kEffectiveStateUnknown;
            m_EffectiveVisibleCache = kEffectiveStateUnknown;

            for (GameObject* child : m_Children)
            {
                if (child != nullptr)
                {
                    child->InvalidateEffectiveStateSubtree();
                }
            }
        }

        /** Publish this node's effective state to its own components and reconcile the delta. */
        void ReconcileEffectiveState() { ApplyEffectiveStateToComponents(IsActive(), IsVisible()); }

    protected:
        virtual void OnBeginPlay() {}
        virtual void OnEndPlay(EEndPlayReason reason) { (void)reason; }

    private:
        void AddComponent_Internal(std::shared_ptr<Component> newComponent);
        bool WouldCreateHierarchyCycle(const GameObject* candidateParent) const;
        void UnlinkFromCurrentParent();
        void RebindRootToGameObjectParent(AttachmentTransformRules rules);
        void RebindChildGameObjectRoots(AttachmentTransformRules rules);

        /**
         * Push-down + delta reconcile for this subtree.
         * Recomputes this node from the (already valid) parent cache, publishes the result to the
         * owned components, then recurses. Authored flags are never written; depth-first with no
         * allocation, so a mid-walk reparent cannot invalidate the loop.
         */
        void PropagateEffectiveStateChange()
        {
            m_EffectiveActiveCache = kEffectiveStateUnknown;
            m_EffectiveVisibleCache = kEffectiveStateUnknown;

            ApplyEffectiveStateToComponents(IsActive(), IsVisible());

            for (size_t index = 0; index < m_Children.size(); ++index)
            {
                GameObject* child = m_Children[index];
                if (child != nullptr)
                {
                    child->PropagateEffectiveStateChange();
                }
            }
        }

        void ApplyEffectiveStateToComponents(bool ownActive, bool ownVisible)
        {
            // Snapshot each element: a reconcile may register/unregister with systems that remove
            // or reorder components, and a range-for over m_Components would then dangle.
            for (size_t index = 0; index < m_Components.size(); ++index)
            {
                const std::shared_ptr<Component> component = m_Components[index];
                if (!component || component->GetClass() == nullptr)
                {
                    continue;
                }

                component->PublishOwnerEffectiveActive(ownActive);

                if (component->IsA(SceneComponent::StaticClass()))
                {
                    SceneComponent* sceneComponent = static_cast<SceneComponent*>(component.get());
                    const bool visibleBefore = sceneComponent->IsVisible();
                    sceneComponent->PublishOwnerEffectiveVisible(ownVisible);
                    if (sceneComponent->IsVisible() != visibleBefore)
                    {
                        sceneComponent->MarkRenderStateDirty();
                    }
                }

                component->ReconcileEffectiveActivation();
            }
        }

    private:
        /** Runtime scene-local lookup id; not used for hierarchy serialization. */
        uint64_t m_ID{ 0 };

        /** Serialized parent (GUID); null = scene root. Children rebuilt on load. */
        ME_PROPERTY()
        GameObject* m_Parent{ nullptr };

        std::vector<GameObject*> m_Children;

        ME_PROPERTY()
        SceneComponent* m_RootComponent{ nullptr };

        ME_PROPERTY(Instanced)
        std::vector<std::shared_ptr<Component>> m_Components;

        /** AUTHORED active flag of this node; the ancestor term of the whole subtree. */
        ME_PROPERTY(Invisible)
        bool m_bActive{ true };

        /** AUTHORED visibility flag of this node; the presentation ancestor term. */
        ME_PROPERTY(Invisible)
        bool m_bVisible{ true };

        mutable uint8_t m_EffectiveActiveCache{ kEffectiveStateUnknown };
        mutable uint8_t m_EffectiveVisibleCache{ kEffectiveStateUnknown };

        bool m_bHasBegunPlay{ false };
        bool m_bPendingDestroy{ false };
        int m_FixedTickComponentCount{ 0 };

    };
}

#include "GameObject.gen.h"
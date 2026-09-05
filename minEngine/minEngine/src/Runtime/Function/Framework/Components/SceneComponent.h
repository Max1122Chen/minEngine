#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

namespace minEngine
{
    class Component;

    enum class AttachmentTransformRules
    {
        KeepRelativeTransform,
        KeepWorldTransform
    };
    
    ME_CLASS()
    class SceneComponent : public Component
    {
        ME_GENERATED_BODY(SceneComponent)

    public:
        SceneComponent();
        virtual ~SceneComponent() override;

        /** Marks this component's render proxy dirty (does not cascade). */
        void MarkRenderStateDirty();
        /**
         * Local transform changed: dirty self for physics/render and cascade to attach children
         * so world-dependent systems (physics bodies, proxies) refresh.
         */
        void NotifyLocalTransformChanged(ETeleportType teleport = ETeleportType::ResetPhysics);
        void ApplyEditorTransformEdit(ETeleportType teleport = ETeleportType::ResetPhysics);
        void PostEditChangeProperty(const Reflection::PropertyChangedEvent& event) override;

        bool IsTransformDirty() const { return m_bTransformDirty; }
        ETeleportType GetPendingTeleportType() const { return m_PendingTeleportType; }
        void ClearTransformDirty();

        const Transform& GetTransform() const { return m_Transform; }
        void SetTransform(const Transform& inTransform);
        void SetTransform(const Transform& inTransform, ETeleportType teleport);
        /** Writes local TRS from a world-space pose (used by physics simulation sync). */
        void SetWorldTransformFromSimulation(const Transform& worldTransform);
        void SetWorldTransform(const Transform& worldTransform);
        void SetWorldTransform(const Transform& worldTransform, ETeleportType teleport);
        void SetTransformFromSimulation(const Transform& inTransform);

        const Vector3& GetPosition() const { return m_Transform.Position; }
        void SetPosition(const Vector3& position);
        void SetPosition(const Vector3& position, ETeleportType teleport);
        void Translate(const Vector3& delta);

        const Quaternion& GetRotation() const { return m_Transform.Rotation; }
        void SetRotation(const Quaternion& rotation);
        void SetRotation(const Quaternion& rotation, ETeleportType teleport);
        Vector3 GetRotationEulerDegrees() const { return m_Transform.GetRotationEulerDegrees(); }
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees);
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local);

        const Vector3& GetScale() const { return m_Transform.Scale; }
        void SetScale(const Vector3& scale);
        void SetScale(const Vector3& scale, ETeleportType teleport);
        void ScaleBy(const Vector3& scaleFactor);

        Vector3 GetForwardVector() const;
        Vector3 GetRightVector() const;
        Vector3 GetUpVector() const;

        Matrix4 GetWorldMatrix() const;
        /** World-space TRS (decomposed from GetWorldMatrix when attached). */
        Transform GetWorldTransform() const;
        Vector3 GetWorldPosition() const;
        Quaternion GetWorldRotation() const;
        Vector3 GetWorldForwardVector() const;
        Vector3 GetWorldUpVector() const;
        /** Decompose a TRS matrix (e.g. ImGuizmo world result) into Transform. */
        static Transform MakeTransformFromMatrix(const Matrix4& matrix);

        virtual void SetOwner(GameObject* inOwner) override;

        // We don't implicitly attach to parent in constructor, because at that time.
        // You should explicitly call AttachToComponent to avoid confusion.
        bool AttachToComponent(SceneComponent* inParent, AttachmentTransformRules attachRules);     // return false if failed
        SceneComponent* GetAttachParent() const { return m_AttachParent; }
        void SetAttachParent(SceneComponent* inParent);
        std::vector<SceneComponent*>& GetAttachChildren() { return m_AttachChildren; }
        void DetachFromParent(AttachmentTransformRules detachRules);

    private:
        static Transform DecomposeMatrixToTransform(const Matrix4& matrix);
        Transform ConvertWorldTransformToLocal(const Transform& worldTransform) const;
        /** Unlink attach parent/children without Notify (safe during destruction). */
        void ClearAttachLinksWithoutNotify();

    protected:
    
        ME_PROPERTY(EditAnywhere)
        Transform m_Transform;

        ME_PROPERTY()
        SceneComponent* m_AttachParent{ nullptr };

        std::vector<SceneComponent*> m_AttachChildren;

        bool m_bRenderStateDirty{ false };
        bool m_bTransformDirty{ false };
        ETeleportType m_PendingTeleportType{ ETeleportType::ResetPhysics };
    };
}

#include "Generated/Reflection/SceneComponent.gen.h"
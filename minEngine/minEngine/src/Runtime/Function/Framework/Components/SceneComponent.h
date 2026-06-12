#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Transform/Transform.h"
#include "Runtime/Function/Framework/Components/Component.h"

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
        virtual ~SceneComponent() = default;

        void MarkRenderStateDirty();

        const Transform& GetTransform() const { return m_Transform; }
        void SetTransform(const Transform& inTransform);

        const Vector3& GetPosition() const { return m_Transform.Position; }
        void SetPosition(const Vector3& position);
        void Translate(const Vector3& delta);

        const Quaternion& GetRotation() const { return m_Transform.Rotation; }
        void SetRotation(const Quaternion& rotation);
        Vector3 GetRotationEulerDegrees() const { return m_Transform.GetRotationEulerDegrees(); }
        void SetRotationEulerDegrees(const Vector3& rotationEulerDegrees);
        void Rotate(const glm::quat& delta, Space relativeTo = Space::Local);

        const Vector3& GetScale() const { return m_Transform.Scale; }
        void SetScale(const Vector3& scale);
        void ScaleBy(const Vector3& scaleFactor);

        Vector3 GetForwardVector() const;
        Vector3 GetRightVector() const;
        Vector3 GetUpVector() const;

        virtual void SetOwner(GameObject* inOwner) override;

        // We don't implicitly attach to parent in constructor, because at that time.
        // You should explicitly call AttachToComponent to avoid confusion.
        bool AttachToComponent(SceneComponent* inParent, AttachmentTransformRules attachRules);     // return false if failed
        SceneComponent* GetAttachParent() const { return m_AttachParent; }
        void SetAttachParent(SceneComponent* inParent);
        std::vector<SceneComponent*>& GetAttachChildren() { return m_AttachChildren; }
        void DetachFromParent(AttachmentTransformRules detachRules);

    protected:
    
        ME_PROPERTY(EditAnywhere)
        Transform m_Transform;

        SceneComponent* m_AttachParent{ nullptr };
        std::vector<SceneComponent*> m_AttachChildren;

        bool m_bRenderStateDirty{ false };
        
    };
}

#include "Generated/Reflection/SceneComponent.gen.h"
#include "SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

namespace minEngine
{
    SceneComponent::SceneComponent()
    {
    }

    void SceneComponent::MarkRenderStateDirty()
    {
        m_bRenderStateDirty = true;
        MarkForNeededEndOfFrameUpdate();
    }

    void SceneComponent::ApplyEditorTransformEdit(ETeleportType teleport)
    {
        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();
    }

    void SceneComponent::ClearTransformDirty()
    {
        m_bTransformDirty = false;
        m_PendingTeleportType = ETeleportType::ResetPhysics;
    }

    void SceneComponent::SetTransformFromSimulation(const Transform& inTransform)
    {
        if (m_Transform == inTransform)
        {
            return;
        }

        m_Transform = inTransform;
        MarkRenderStateDirty();
    }

    void SceneComponent::SetTransform(const Transform& inTransform)
    {
        SetTransform(inTransform, ETeleportType::ResetPhysics);
    }

    void SceneComponent::SetTransform(const Transform& inTransform, ETeleportType teleport)
    {
        if (m_Transform == inTransform)
        {
            return;
        }

        m_Transform = inTransform;
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->SetTransform(inTransform, teleport);
            }
        }

        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();
    }

    void SceneComponent::SetPosition(const Vector3& position)
    {
        SetPosition(position, ETeleportType::ResetPhysics);
    }

    void SceneComponent::SetPosition(const Vector3& position, ETeleportType teleport)
    {
        if (m_Transform.Position == position)
        {
            return;
        }

        m_Transform.Position = position;
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->SetPosition(position, teleport);
            }
        }

        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();
    }

    void SceneComponent::Translate(const Vector3& delta)
    {
        Transform tempTransform = m_Transform;
        tempTransform.Translate(delta);
        SetPosition(tempTransform.Position);
    }

    void SceneComponent::SetRotation(const Quaternion& rotation)
    {
        SetRotation(rotation, ETeleportType::ResetPhysics);
    }

    void SceneComponent::SetRotation(const Quaternion& rotation, ETeleportType teleport)
    {
        if (m_Transform.Rotation == rotation)
        {
            return;
        }

        m_Transform.SetRotation(rotation);
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->SetRotation(rotation, teleport);
            }
        }

        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();
    }

    void SceneComponent::SetRotationEulerDegrees(const Vector3& rotationEulerDegrees)
    {
        SetRotation(Quaternion::FromEulerDegreesXYZ(rotationEulerDegrees));
    }

    void SceneComponent::Rotate(const glm::quat& delta, Space relativeTo)
    {
        Transform tempTransform = m_Transform;
        tempTransform.Rotate(delta, relativeTo);
        SetRotation(tempTransform.GetRotation());
    }

    void SceneComponent::SetScale(const Vector3& scale)
    {
        SetScale(scale, ETeleportType::ResetPhysics);
    }

    void SceneComponent::SetScale(const Vector3& scale, ETeleportType teleport)
    {
        if (m_Transform.Scale == scale)
        {
            return;
        }

        m_Transform.Scale = scale;
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->SetScale(scale, teleport);
            }
        }

        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();
    }

    void SceneComponent::ScaleBy(const Vector3& scaleFactor)
    {
        Transform tempTransform = m_Transform;
        tempTransform.ScaleBy(scaleFactor);
        SetScale(tempTransform.Scale);
    }

    Vector3 SceneComponent::GetForwardVector() const
    {
        const glm::quat rotationQuat = m_Transform.Rotation.ToGlm();
        return glm::normalize(rotationQuat * Vector3(1.0f, 0.0f, 0.0f));
    }

    Vector3 SceneComponent::GetRightVector() const
    {
        const glm::quat rotationQuat = m_Transform.Rotation.ToGlm();
        return glm::normalize(rotationQuat * Vector3(0.0f, 0.0f, 1.0f));
    }

    Vector3 SceneComponent::GetUpVector() const
    {
        const glm::quat rotationQuat = m_Transform.Rotation.ToGlm();
        return glm::normalize(rotationQuat * Vector3(0.0f, 1.0f, 0.0f));
    }

    void SceneComponent::SetOwner(GameObject* inOwner)
    {
        Component::SetOwner(inOwner);
    }

    bool SceneComponent::AttachToComponent(SceneComponent* inParent, AttachmentTransformRules attachRules)
    {
        (void)attachRules;

        if (inParent == nullptr)
        {
            return false;
        }

        if (GetAttachParent() != nullptr)
        {
            auto& siblings = m_AttachParent->m_AttachChildren;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        SetAttachParent(inParent);
        inParent->m_AttachChildren.push_back(this);

        MarkRenderStateDirty();
        return true;
    }

    void SceneComponent::SetAttachParent(SceneComponent* inParent)
    {
        m_AttachParent = inParent;
    }

    void SceneComponent::DetachFromParent(AttachmentTransformRules detachRules)
    {
        (void)detachRules;
    }
}

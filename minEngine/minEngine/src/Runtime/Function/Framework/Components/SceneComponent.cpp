#include "SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace minEngine
{
    Transform SceneComponent::DecomposeMatrixToTransform(const Matrix4& matrix)
    {
        glm::vec3 translation{};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 skew{};
        glm::vec4 perspective{};

        glm::decompose(matrix, scale, rotation, translation, skew, perspective);

        Transform result;
        result.Position = Vector3(translation);
        result.Rotation = Quaternion::FromGlm(rotation);
        result.Scale = Vector3(scale);
        return result;
    }

    SceneComponent::SceneComponent()    {
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

    Matrix4 SceneComponent::GetWorldMatrix() const
    {
        const Matrix4 localMatrix = m_Transform.ToMatrix();
        if (m_AttachParent == nullptr)
        {
            return localMatrix;
        }

        return m_AttachParent->GetWorldMatrix() * localMatrix;
    }

    Vector3 SceneComponent::GetWorldPosition() const
    {
        const Matrix4 worldMatrix = GetWorldMatrix();
        return Vector3(worldMatrix[3]);
    }

    Vector3 SceneComponent::GetWorldForwardVector() const
    {
        const Matrix4 worldMatrix = GetWorldMatrix();
        return glm::normalize(Vector3(worldMatrix[0]));
    }

    Vector3 SceneComponent::GetWorldUpVector() const
    {
        const Matrix4 worldMatrix = GetWorldMatrix();
        return glm::normalize(Vector3(worldMatrix[1]));
    }

    void SceneComponent::SetOwner(GameObject* inOwner)
    {
        Component::SetOwner(inOwner);
    }

    bool SceneComponent::AttachToComponent(SceneComponent* inParent, AttachmentTransformRules attachRules)
    {
        if (inParent == nullptr)
        {
            return false;
        }

        const Matrix4 worldMatrixBeforeAttach = GetWorldMatrix();
        SetAttachParent(inParent);

        if (attachRules == AttachmentTransformRules::KeepWorldTransform)
        {
            const Matrix4 parentWorldMatrix = inParent->GetWorldMatrix();
            m_Transform = DecomposeMatrixToTransform(glm::inverse(parentWorldMatrix) * worldMatrixBeforeAttach);
        }

        MarkRenderStateDirty();
        return true;
    }
    void SceneComponent::SetAttachParent(SceneComponent* inParent)
    {
        if (m_AttachParent == inParent)
        {
            return;
        }

        if (m_AttachParent != nullptr)
        {
            auto& siblings = m_AttachParent->m_AttachChildren;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        m_AttachParent = inParent;

        if (m_AttachParent != nullptr)
        {
            auto& siblings = m_AttachParent->m_AttachChildren;
            if (std::find(siblings.begin(), siblings.end(), this) == siblings.end())
            {
                siblings.push_back(this);
            }
        }
    }

    void SceneComponent::DetachFromParent(AttachmentTransformRules detachRules)
    {
        if (m_AttachParent == nullptr)
        {
            return;
        }

        const Matrix4 worldMatrixBeforeDetach = GetWorldMatrix();
        SetAttachParent(nullptr);

        if (detachRules == AttachmentTransformRules::KeepWorldTransform)
        {
            m_Transform = DecomposeMatrixToTransform(worldMatrixBeforeDetach);
        }

        MarkRenderStateDirty();
    }
}
#include "SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"

#include <algorithm>
#include <string_view>

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

    SceneComponent::~SceneComponent()
    {
        ClearAttachLinksWithoutNotify();
    }

    void SceneComponent::ClearAttachLinksWithoutNotify()
    {
        // Children first: clear their parent pointer without Notify (we are being destroyed).
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr && child->m_AttachParent == this)
            {
                child->m_AttachParent = nullptr;
            }
        }
        m_AttachChildren.clear();

        if (m_AttachParent != nullptr)
        {
            auto& siblings = m_AttachParent->m_AttachChildren;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            m_AttachParent = nullptr;
        }
    }

    void SceneComponent::MarkRenderStateDirty()
    {
        m_bRenderStateDirty = true;
        MarkForNeededEndOfFrameUpdate();
    }

    void SceneComponent::NotifyLocalTransformChanged(ETeleportType teleport)
    {
        m_bTransformDirty = true;
        m_PendingTeleportType = teleport;
        MarkRenderStateDirty();

        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->NotifyLocalTransformChanged(teleport);
            }
        }
    }

    Transform SceneComponent::GetWorldTransform() const
    {
        if (m_AttachParent == nullptr)
        {
            return m_Transform;
        }

        return DecomposeMatrixToTransform(GetWorldMatrix());
    }

    Transform SceneComponent::MakeTransformFromMatrix(const Matrix4& matrix)
    {
        return DecomposeMatrixToTransform(matrix);
    }

    Transform SceneComponent::ConvertWorldTransformToLocal(const Transform& worldTransform) const
    {
        if (m_AttachParent == nullptr)
        {
            return worldTransform;
        }

        const Matrix4 parentWorldMatrix = m_AttachParent->GetWorldMatrix();
        return DecomposeMatrixToTransform(glm::inverse(parentWorldMatrix) * worldTransform.ToMatrix());
    }

    void SceneComponent::ApplyEditorTransformEdit(ETeleportType teleport)
    {
        NotifyLocalTransformChanged(teleport);
    }

    void SceneComponent::PostEditChangeProperty(const Reflection::PropertyChangedEvent& event)
    {
        const std::string_view name = event.propertyName;
        const bool isTransformEdit =
            name == "m_Transform"
            || (name.size() >= 12 && name.compare(0, 12, "m_Transform.") == 0);
        if (isTransformEdit)
        {
            ApplyEditorTransformEdit();
        }

        Component::PostEditChangeProperty(event);
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
        for (SceneComponent* child : m_AttachChildren)
        {
            if (child != nullptr)
            {
                child->NotifyLocalTransformChanged(ETeleportType::TeleportPhysics);
            }
        }
    }

    void SceneComponent::SetWorldTransformFromSimulation(const Transform& worldTransform)
    {
        SetTransformFromSimulation(ConvertWorldTransformToLocal(worldTransform));
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
        NotifyLocalTransformChanged(teleport);
    }

    void SceneComponent::SetWorldTransform(const Transform& worldTransform)
    {
        SetWorldTransform(worldTransform, ETeleportType::ResetPhysics);
    }

    void SceneComponent::SetWorldTransform(const Transform& worldTransform, ETeleportType teleport)
    {
        SetTransform(ConvertWorldTransformToLocal(worldTransform), teleport);
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
        NotifyLocalTransformChanged(teleport);
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
        NotifyLocalTransformChanged(teleport);
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
        NotifyLocalTransformChanged(teleport);
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

    Quaternion SceneComponent::GetWorldRotation() const
    {
        return GetWorldTransform().Rotation;
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

        NotifyLocalTransformChanged(ETeleportType::ResetPhysics);
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

        NotifyLocalTransformChanged(ETeleportType::ResetPhysics);
    }
}
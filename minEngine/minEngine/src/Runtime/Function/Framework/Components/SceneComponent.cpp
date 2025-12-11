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

    void SceneComponent::SetTransform(const Transform &inTransform)
    {
        if(!(m_Transform == inTransform))
        {
            m_Transform = inTransform;
            for(auto& child : m_AttachChildren)
            {
                child->SetTransform(inTransform);   // simply propagate to children for now. TODO: Should we use dirty flag instead?
            }

            MarkRenderStateDirty();
        }
    }

    void SceneComponent::SetPosition(const Vector3 &position)
    {
        if(!(m_Transform.Position == position))
        {
            m_Transform.Position = position;
            for(auto& child : m_AttachChildren)
            {
                child->SetPosition(position);   // simply propagate to children for now. TODO: Should we use dirty flag instead?
            }
            MarkRenderStateDirty();
        }
    }

    void SceneComponent::SetRotation(const Vector3 &rotation)
    {
        if(!(m_Transform.Rotation == rotation))
        {
            m_Transform.Rotation = rotation;
            for(auto& child : m_AttachChildren)
            {
                child->SetRotation(rotation);   // simply propagate to children for now. TODO: Should we use dirty flag instead?
            }
            MarkRenderStateDirty();
        }
    }

    void SceneComponent::SetScale(const Vector3 &scale)
    {
        if(!(m_Transform.Scale == scale))
        {
            m_Transform.Scale = scale;
            for(auto& child : m_AttachChildren)
            {
                child->SetScale(scale);   // simply propagate to children for now. TODO: Should we use dirty flag instead?
            }
            MarkRenderStateDirty();
        }
    }

    void SceneComponent::SetOwner(GameObject *inOwner)
    {
        Component::SetOwner(inOwner);
    }

    bool SceneComponent::AttachToComponent(SceneComponent *inParent, AttachmentTransformRules attachRules)
    {
        if (inParent == nullptr)
        {
            return false;
        }

        // Detach from current parent
        if (GetAttachParent() != nullptr)
        {
            auto& siblings = m_AttachParent->m_AttachChildren;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), std::shared_ptr<SceneComponent>(this)), siblings.end());
        }

        // Attach to new parent
        SetAttachParent(inParent);
        inParent->m_AttachChildren.push_back(std::shared_ptr<SceneComponent>(this));    // add self to parent's children list

        // Update transform simply for now. Just use parent's world transform.
        // if (attachRules == AttachmentTransformRules::KeepWorldTransform)
        SetTransform(inParent->GetTransform());

        MarkRenderStateDirty();
        return true;
    }

    void SceneComponent::SetAttachParent(SceneComponent *inParent)
    {
        m_AttachParent = std::shared_ptr<SceneComponent>(inParent);
    }
}

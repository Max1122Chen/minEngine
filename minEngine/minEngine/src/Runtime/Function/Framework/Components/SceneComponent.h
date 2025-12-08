#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Framework/Transform.h"
#include "Runtime/Function/Framework/Components/Component.h"

namespace minEngine
{
    class SceneComponent : public Component
    {
    public:
        SceneComponent() = default;
        SceneComponent(std::shared_ptr<GameObject> owner);
        virtual ~SceneComponent() = default;


        const Transform& GetTransform() const { return m_Transform; }
        void SetTransform(const Transform& inTransform) { m_Transform = inTransform; }

        const Vector3& GetPosition() const { return m_Transform.Position; }
        void SetPosition(const Vector3& position) { m_Transform.Position = position; }

        const Vector3& GetRotation() const { return m_Transform.Rotation; }
        void SetRotation(const Vector3& rotation) { m_Transform.Rotation = rotation; }

        const Vector3& GetScale() const { return m_Transform.Scale; }
        void SetScale(const Vector3& scale) { m_Transform.Scale = scale; }

        Transform m_Transform;
    };
}
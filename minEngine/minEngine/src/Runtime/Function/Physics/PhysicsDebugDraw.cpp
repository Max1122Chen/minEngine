#include "PhysicsDebugDraw.h"

#include "BoxColliderComponent.h"
#include "CapsuleColliderComponent.h"
#include "ColliderComponent.h"
#include "SphereColliderComponent.h"

#include "Runtime/Function/Debug/DebugDraw.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Scene/Scene.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace minEngine::PhysicsDebugDraw
{
    namespace
    {
        const Options& GetDefaultOptions()
        {
            static const Options kDefaultOptions;
            return kDefaultOptions;
        }

        float GetUniformScale(const Vector3& scale)
        {
            return scale.x;
        }

        Matrix4 BuildColliderWorldTransform(const SceneComponent& rootComponent)
        {
            const Transform& transform = rootComponent.GetTransform();
            Matrix4 worldTransform = glm::translate(glm::mat4(1.0f), transform.Position);
            worldTransform *= glm::mat4_cast(transform.Rotation.ToGlm());
            return worldTransform;
        }

        Vector4 GetColorForChannel(const ECollisionChannel channel)
        {
            switch (channel)
            {
            case ECollisionChannel::WorldStatic:
                return Vector4(0.4f, 0.8f, 0.4f, 1.0f);
            case ECollisionChannel::Default:
                return Vector4(0.2f, 0.9f, 1.0f, 1.0f);
            case ECollisionChannel::Trigger:
                return Vector4(1.0f, 0.9f, 0.2f, 1.0f);
            default:
                return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            }
        }

        void SubmitCollider(
            const ColliderComponent& colliderComponent,
            const SceneComponent& rootComponent)
        {
            const Matrix4 worldTransform = BuildColliderWorldTransform(rootComponent);
            const float uniformScale = GetUniformScale(rootComponent.GetScale());
            const Vector4 color = GetColorForChannel(colliderComponent.GetObjectChannel());

            if (colliderComponent.IsA(BoxColliderComponent::StaticClass()))
            {
                const auto& boxCollider = static_cast<const BoxColliderComponent&>(colliderComponent);
                DebugDraw::Box(worldTransform, boxCollider.GetHalfExtent() * uniformScale, color);
                return;
            }

            if (colliderComponent.IsA(SphereColliderComponent::StaticClass()))
            {
                const auto& sphereCollider = static_cast<const SphereColliderComponent&>(colliderComponent);
                DebugDraw::Sphere(worldTransform, sphereCollider.GetRadius() * uniformScale, color);
                return;
            }

            if (colliderComponent.IsA(CapsuleColliderComponent::StaticClass()))
            {
                const auto& capsuleCollider = static_cast<const CapsuleColliderComponent&>(colliderComponent);
                DebugDraw::Capsule(
                    worldTransform,
                    capsuleCollider.GetRadius() * uniformScale,
                    capsuleCollider.GetHalfHeight() * uniformScale,
                    color);
            }
        }
    }

    const Options& GetOptions()
    {
        return GetDefaultOptions();
    }

    void SubmitScene(const Scene& scene, const PhysicsWorld& world, const Options& options)
    {
        (void)world;

        if (!options.bDrawColliders)
        {
            return;
        }

        for (const std::shared_ptr<GameObject>& gameObject : scene.GetAllGameObjects())
        {
            if (!gameObject)
            {
                continue;
            }

            SceneComponent* rootComponent = gameObject->GetRootComponent();
            if (rootComponent == nullptr)
            {
                continue;
            }

            for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
            {
                if (!component || !component->GetClass() || !component->IsA(ColliderComponent::StaticClass()))
                {
                    continue;
                }

                const auto& colliderComponent = static_cast<const ColliderComponent&>(*component);
                SubmitCollider(colliderComponent, *rootComponent);
            }
        }
    }
}

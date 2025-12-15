#include "MovementComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    void MovementComponent::AddMovementInput(const Vector3& worldDirection, float scale)
    {
        SceneComponent* ownerSceneComponent = GetOwner()->GetComponent<SceneComponent>().get();
        if(ownerSceneComponent)
        {
            Vector3 newPosition = ownerSceneComponent->GetPosition() + worldDirection * scale;
            ownerSceneComponent->SetPosition(newPosition);
        }
    }

    void MovementComponent::AddRotationInput(const Vector3& deltaRotation)
    {
        SceneComponent* ownerSceneComponent = dynamic_cast<SceneComponent*>(GetOwner()->GetComponent<SceneComponent>().get());
        if(ownerSceneComponent)
        {
            Vector3 newRotation = ownerSceneComponent->GetRotation() + deltaRotation;
            ownerSceneComponent->SetRotation(newRotation);
        }
    }
}
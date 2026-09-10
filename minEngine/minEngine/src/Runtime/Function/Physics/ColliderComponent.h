#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

namespace minEngine
{
    ME_CLASS()
    class ColliderComponent : public Component
    {
        ME_GENERATED_BODY()

    public:
        ColliderComponent() = default;
        ~ColliderComponent() override;

        ECollisionChannel GetObjectChannel() const { return m_ObjectChannel; }
        void SetObjectChannel(ECollisionChannel objectChannel);

    protected:
        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

        void RefreshOwningRigidBody();

        void PostEditChangeProperty(const Reflection::PropertyChangedEvent& event) override;

        ME_PROPERTY(EditAnywhere, meta = (Setter = "SetObjectChannel", Getter = "GetObjectChannel"))
        ECollisionChannel m_ObjectChannel{ECollisionChannel::Default};
    };
}

#include "Generated/Reflection/ColliderComponent.gen.h"

#pragma once

#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    ME_CLASS()
    class AudioListenerComponent : public SceneComponent
    {
        ME_GENERATED_BODY(AudioListenerComponent)

    public:
        AudioListenerComponent();
        ~AudioListenerComponent() override;

        void SetOwner(GameObject* inOwner) override;

        bool GetUseTransformOrientation() const { return m_bUseTransformOrientation; }
        void SetUseTransformOrientation(bool useTransform) { m_bUseTransformOrientation = useTransform; }

    private:
        ME_PROPERTY(EditAnywhere)
        bool m_bUseTransformOrientation{true};
    };
}

#include "Generated/Reflection/AudioListenerComponent.gen.h"

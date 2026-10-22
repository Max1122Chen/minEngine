#pragma once

#include "Runtime/Core/EngineAPI.h"

#include "Runtime/Function/Framework/Components/SceneComponent.h"

namespace minEngine
{
    ME_CLASS()
    class MINENGINE_API AudioListenerComponent : public SceneComponent
    {
        ME_GENERATED_BODY()

    public:
        AudioListenerComponent();
        ~AudioListenerComponent() override;

        bool GetUseTransformOrientation() const { return m_bUseTransformOrientation; }
        void SetUseTransformOrientation(bool useTransform) { m_bUseTransformOrientation = useTransform; }

    protected:
        void ApplyActivationToSystems() override;
        void RemoveActivationFromSystems() override;

    private:
        ME_PROPERTY(EditAnywhere)
        bool m_bUseTransformOrientation{true};
    };
}

#include "Generated/Reflection/AudioListenerComponent.gen.h"

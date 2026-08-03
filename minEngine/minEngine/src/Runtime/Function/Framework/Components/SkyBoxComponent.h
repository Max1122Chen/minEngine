#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Render/Environment/EnvironmentMap.h"

namespace minEngine
{
    class SkyBoxSceneProxy;

    ME_CLASS()
    class SkyBoxComponent : public SceneComponent
    {
        ME_GENERATED_BODY(SkyBoxComponent)

    public:
        SkyBoxComponent();
        virtual ~SkyBoxComponent() override;

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_Enabled; }

        void SetSkyIntensity(float intensity);
        float GetSkyIntensity() const { return m_SkyIntensity; }

        void SetEnvironmentMap(const std::shared_ptr<EnvironmentMap>& environmentMap);
        EnvironmentMap* GetEnvironmentMap() const { return m_EnvironmentMap.get(); }
        const std::shared_ptr<EnvironmentMap>& GetEnvironmentMapShared() const { return m_EnvironmentMap; }

        virtual void DoEndOfFrameUpdate() override;

        SkyBoxSceneProxy* CreateSceneProxy();
        SkyBoxSceneProxy* GetSceneProxy() const { return m_SkyBoxSceneProxy; }

        ME_PROPERTY(EditAnywhere)
        bool m_Enabled = true;

        ME_PROPERTY(EditAnywhere)
        float m_SkyIntensity = 1.0f;

        /** Project EnvironmentMap asset (not EngineDefault path). */
        ME_PROPERTY(EditAnywhere)
        std::shared_ptr<EnvironmentMap> m_EnvironmentMap;

        SkyBoxSceneProxy* m_SkyBoxSceneProxy = nullptr;
    };
}

#include "Generated/Reflection/SkyBoxComponent.gen.h"

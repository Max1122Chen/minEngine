#pragma once

#include "GameModuleStubAPI.h"
#include "Runtime/Function/Framework/Components/PrimitiveComponent.h"
#include "Runtime/Function/Render/PrimitiveSceneProxies/PrimitiveSceneProxy.h"

namespace GameModuleStub
{
    /**
     * Minimal external-module PrimitiveComponent for multi-module reflection smoke.
     * Proves game SHARED can ME_CLASS + AddComponent<T>() without engine-side scan of game src.
     */
    ME_CLASS()
    class StubGamePrimitiveComponent : public minEngine::PrimitiveComponent
    {
        ME_GENERATED_BODY()

    public:
        StubGamePrimitiveComponent() = default;
        ~StubGamePrimitiveComponent() override = default;

        minEngine::Math::Geometry::AABB GetBoundingBox() const override
        {
            return minEngine::Math::Geometry::AABB{};
        }

        minEngine::PrimitiveSceneProxy* CreateSceneProxy() override
        {
            minEngine::PrimitiveSceneProxy* proxy = new minEngine::PrimitiveSceneProxy();
            SyncSceneProxy(*proxy);
            m_SceneProxy = proxy;
            return proxy;
        }
    };
}

#include "StubGamePrimitiveComponent.gen.h"

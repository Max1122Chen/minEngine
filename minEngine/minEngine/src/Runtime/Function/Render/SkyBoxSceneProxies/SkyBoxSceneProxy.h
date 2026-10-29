#pragma once

#include "Core.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <memory>

namespace minEngine
{
    class SkyBoxComponent;
    class EnvironmentMap;

    class SkyBoxSceneProxy
    {
    public:
        SkyBoxSceneProxy() = default;
        virtual ~SkyBoxSceneProxy() = default;

        /**
         * Identity token for the component this proxy was created from; only compared by the
         * removal command, which never dereferences it.
         */
        SkyBoxComponent* m_SkyBoxComponent = nullptr;

        /**
         * Set by the removal command on the render role once the proxy must stop drawing.
         *
         * Written only there: a game-role clear of the identity token would take the only key the
         * queued removal has, and the sky would keep drawing until the scene it belongs to dies.
         */
        bool m_bPendingRemove = false;

        Transform m_Transform;
        float m_SkyIntensity = 1.0f;
        bool m_Enabled = true;
        std::shared_ptr<EnvironmentMap> m_EnvironmentMap;
    };
}

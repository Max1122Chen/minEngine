#pragma once
#include "Core.h"
#include "Runtime/Core/Math/Math.h"

namespace minEngine
{
    enum class LightType : uint8_t;
    class LightComponent;

    class LightSceneProxy
    {
    public:
        LightSceneProxy() = default;
        virtual ~LightSceneProxy() = default;

    public:
        // Light properties
        LightType m_LightType;

        Vector3 m_Position;
        Vector3 m_LightColor;
        float m_Intensity;
        float m_DiffuseFactor;
        float m_SpecularFactor;
        bool m_CastsShadow;

        /**
         * Whether this light contributes illumination and shadow, i.e. the component is active and
         * visible. Written by the game thread when the proxy is created and synced; the render
         * thread only reads it, so no pass has to dereference the component to learn the state.
         */
        bool m_Enabled = true;

        /**
         * Set by a removal command on the render role once the proxy must stop contributing light.
         *
         * The identity token below used to double as this state: a game-role teardown path cleared
         * it, and a reader treated "no component" as "removed". That made a game-role pointer write
         * the render role's notion of a half-applied removal - and left the removal command that
         * was already queued with nothing to match. The flag carries that state instead and is
         * written only where the removal commands run.
         */
        bool m_bPendingRemove = false;

        LightComponent* m_LightComponent;
    };
}

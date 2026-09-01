#pragma once

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

namespace minEngine
{
    class PhysicsWorld;
    class Scene;
}

namespace minEngine::PhysicsDebugDraw
{
    struct Options
    {
        bool bDrawColliders = true;
        bool bDrawContacts = true;
        bool bDrawActiveTrace = false;
        float ContactNormalLength = 0.15f;
    };

    const Options& GetOptions();

    void SubmitScene(const Scene& scene, const PhysicsWorld& world, const Options& options);
}

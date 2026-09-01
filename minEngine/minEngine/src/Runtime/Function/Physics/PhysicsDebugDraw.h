#pragma once

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Physics/PhysicsTypes.h"

namespace minEngine
{
    class Scene;
}

namespace minEngine::PhysicsDebugDraw
{
    struct Options
    {
        bool bDrawColliders = true;
    };

    const Options& GetOptions();

    void SubmitScene(const Scene& scene, const Options& options);
}

#pragma once

#include "Runtime/Core/Math/Math.h"
#include "Runtime/Function/Debug/DebugDrawTypes.h"
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

    /**
     * What this module read from the game world. The counters are per access, so a caller can
     * bracket an interval and read the difference: a drawing-side interval has to show none.
     */
    struct AccessProbe
    {
        /** Scene walks (enumerating a scene's game objects). */
        uint64_t SceneWalks = 0;
        /** Game objects visited while looking for colliders. */
        uint64_t GameObjectReads = 0;
        /** Colliders whose pose and shape were read. */
        uint64_t ColliderReads = 0;

        uint64_t Total() const { return SceneWalks + GameObjectReads + ColliderReads; }
    };

    MINENGINE_API AccessProbe ProbeGameObjectAccess();

    /**
     * World side. Reads the scene once and returns what it would draw as plain values: no scene,
     * game object or component pointer travels with the result, so it stays valid while the world
     * keeps changing. Null when there is nothing to draw.
     *
     * The options default to the module's own defaults, so a caller outside the engine does not
     * need a second exported accessor just to say "draw everything".
     */
    MINENGINE_API DebugDrawSnapshotRef BuildSnapshot(const Scene& scene, const Options& options = Options{});
}

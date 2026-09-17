#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <memory>

namespace minEngine
{
    class MEObject;
    class Prefab;
    class Scene;

    /** Shared Guid → object lookup for Prefab template / Scene instance trees. */
    class PrefabObjectLookup
    {
    public:
        static std::shared_ptr<MEObject> FindSharedInScene(Scene& scene, const GUID& guid);
        static MEObject* FindInPrefab(Prefab& prefab, const GUID& guid);
        static const MEObject* FindInPrefab(const Prefab& prefab, const GUID& guid);
        static bool PrefabContains(const Prefab& prefab, const GUID& guid);
    };
}

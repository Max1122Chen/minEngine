#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Framework/Prefab/PrefabTypes.h"

#include <unordered_set>

namespace minEngine
{
    class MEObject;
    class Prefab;

    class PrefabReferencePolicy
    {
    public:
        /** Null out non-asset object refs whose Guid is outside templateGuidSet. Recurses Instanced ownership. */
        static void StripExternalNonAssetRefs(
            Prefab& prefab,
            const std::unordered_set<GUID, GUID::Hash>& templateGuidSet,
            PrefabCreateReport* outReport);
    };
}

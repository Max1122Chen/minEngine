#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"

#include <memory>
#include <unordered_map>

namespace minEngine
{
    class MEObject;

    /** Source→clone Guid map used by Prefab Instantiate and PIE Scene duplication. */
    struct ObjectCloneContext
    {
        std::unordered_map<GUID, GUID, GUID::Hash> SourceToClonedGuid;
        std::unordered_map<GUID, std::shared_ptr<MEObject>, GUID::Hash> ClonedBySourceGuid;

        void RecordClone(
            const GUID& sourceGuid,
            const std::shared_ptr<MEObject>& clonedObject,
            const GUID& clonedGuid);

        MEObject* ResolveRef(const GUID& sourceGuid) const;
        std::shared_ptr<MEObject> ResolveRefShared(const GUID& sourceGuid) const;

        void Clear();
    };
}

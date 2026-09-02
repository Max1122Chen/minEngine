#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

#include <memory>
#include <unordered_map>

namespace minEngine
{
    class MEObject;

    struct SceneCloneContext
    {
        int32_t PIEInstanceId = 0;
        ESceneType TargetType = ESceneType::PIE;

        std::unordered_map<GUID, GUID, GUID::Hash> SourceToClonedGuid;
        std::unordered_map<GUID, std::shared_ptr<MEObject>, GUID::Hash> ClonedBySourceGuid;

        void RecordClone(const GUID& sourceGuid, const std::shared_ptr<MEObject>& clonedObject, const GUID& clonedGuid);
        MEObject* ResolveSceneRef(const GUID& sourceGuid) const;
        std::shared_ptr<MEObject> ResolveSceneRefShared(const GUID& sourceGuid) const;
    };
}

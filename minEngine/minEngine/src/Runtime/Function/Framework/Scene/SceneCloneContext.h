#pragma once

#include "Core.h"
#include "Runtime/Core/Object/ObjectCloneContext.h"
#include "Runtime/Function/Framework/Scene/SceneTypes.h"

namespace minEngine
{
    struct SceneCloneContext : ObjectCloneContext
    {
        int32_t PIEInstanceId = 0;
        ESceneType TargetType = ESceneType::PIE;

        MEObject* ResolveSceneRef(const GUID& sourceGuid) const { return ResolveRef(sourceGuid); }
        std::shared_ptr<MEObject> ResolveSceneRefShared(const GUID& sourceGuid) const
        {
            return ResolveRefShared(sourceGuid);
        }
    };
}

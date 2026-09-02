#include "SceneCloneContext.h"

#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    void SceneCloneContext::RecordClone(
        const GUID& sourceGuid,
        const std::shared_ptr<MEObject>& clonedObject,
        const GUID& clonedGuid)
    {
        if (sourceGuid.IsZero() || clonedGuid.IsZero() || clonedObject == nullptr)
        {
            return;
        }

        SourceToClonedGuid[sourceGuid] = clonedGuid;
        ClonedBySourceGuid[sourceGuid] = clonedObject;
    }

    MEObject* SceneCloneContext::ResolveSceneRef(const GUID& sourceGuid) const
    {
        const std::shared_ptr<MEObject> resolved = ResolveSceneRefShared(sourceGuid);
        return resolved ? resolved.get() : nullptr;
    }

    std::shared_ptr<MEObject> SceneCloneContext::ResolveSceneRefShared(const GUID& sourceGuid) const
    {
        if (sourceGuid.IsZero())
        {
            return nullptr;
        }

        const auto iter = ClonedBySourceGuid.find(sourceGuid);
        if (iter == ClonedBySourceGuid.end())
        {
            return nullptr;
        }

        return iter->second;
    }
}

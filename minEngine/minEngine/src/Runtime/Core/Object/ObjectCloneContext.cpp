#include "Runtime/Core/Object/ObjectCloneContext.h"

#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    void ObjectCloneContext::RecordClone(
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

    MEObject* ObjectCloneContext::ResolveRef(const GUID& sourceGuid) const
    {
        const std::shared_ptr<MEObject> resolved = ResolveRefShared(sourceGuid);
        return resolved ? resolved.get() : nullptr;
    }

    std::shared_ptr<MEObject> ObjectCloneContext::ResolveRefShared(const GUID& sourceGuid) const
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

    void ObjectCloneContext::Clear()
    {
        SourceToClonedGuid.clear();
        ClonedBySourceGuid.clear();
    }
}

#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <string>
#include <vector>

namespace minEngine
{
    class GameObject;
    class Scene;
    class Prefab;

    ME_STRUCT()
    struct PrefabObjectMapping
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        GUID TemplateGuid;

        ME_PROPERTY()
        GUID InstanceGuid;
    };

    /** Schema placeholder for CORE-F24; always empty in CORE-F23. */
    ME_STRUCT()
    struct PrefabPropertyOverride
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        GUID TemplateObjectGuid;

        ME_PROPERTY()
        std::string PropertyPath;

        ME_PROPERTY()
        std::string ValueJson;
    };

    ME_STRUCT()
    struct PrefabInstanceRecord
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        GUID PrefabAssetGuid;

        ME_PROPERTY()
        GUID RootInstanceGuid;

        ME_PROPERTY()
        std::vector<PrefabObjectMapping> ObjectMappings;

        ME_PROPERTY()
        std::vector<PrefabPropertyOverride> Overrides;
    };

    struct PrefabBrokenRef
    {
        GUID OwnerTemplateGuid;
        std::string PropertyPath;
        GUID PreviousTargetGuid;
        std::string Reason;
    };

    struct PrefabCreateReport
    {
        std::vector<PrefabBrokenRef> BrokenRefs;
    };

    struct PrefabInstantiateParams
    {
        Transform WorldTransform{};
        GameObject* AttachParent = nullptr;
        bool bRegisterPrefabInstance = true;
    };
}

#include "PrefabTypes.gen.h"

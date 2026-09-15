#pragma once

#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/Transform/Transform.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class GameObject;
    class Scene;
    class Prefab;

    ME_ENUM()
    enum class EPrefabOverrideKind : uint8_t
    {
        PropertyValue = 0,
        AddedComponent,
        RemovedComponent,
    };

    ME_STRUCT()
    struct PrefabObjectMapping
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        GUID TemplateGuid;

        ME_PROPERTY()
        GUID InstanceGuid;
    };

    ME_STRUCT()
    struct PrefabPropertyOverride
    {
        ME_GENERATED_BODY()

        ME_PROPERTY()
        EPrefabOverrideKind Kind{ EPrefabOverrideKind::PropertyValue };

        ME_PROPERTY()
        GUID TemplateObjectGuid;

        ME_PROPERTY()
        std::string PropertyPath;

        /** Opaque payload; MVP uses "bin:" + hex of binary property buffer. */
        ME_PROPERTY()
        std::string ValueJson;

        ME_PROPERTY()
        std::string TypeName;

        ME_PROPERTY()
        GUID AddedInstanceGuid;
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

    ME_ENUM()
    enum class EPrefabEditOpKind : uint8_t
    {
        PropertyEdit = 0,
        Reparent,
        DeleteGameObject,
        AddTopLevelGameObject,
        AddComponent,
        RemoveComponent,
    };

    struct PrefabEditOp
    {
        EPrefabEditOpKind Kind = EPrefabEditOpKind::PropertyEdit;
        GUID TargetInstanceGuid;
        GUID NewParentInstanceGuid;
    };

    struct PrefabEditValidationResult
    {
        bool bAllowed = true;
        std::string Error;
    };
}

#include "PrefabTypes.gen.h"

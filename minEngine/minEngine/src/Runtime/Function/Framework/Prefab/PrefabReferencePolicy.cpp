#include "Runtime/Function/Framework/Prefab/PrefabReferencePolicy.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/MEClass.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Resource/Asset.h"

namespace minEngine
{
    namespace
    {
        using Reflection::MEArrayProperty;
        using Reflection::MEClass;
        using Reflection::MEObjectPtrCategory;
        using Reflection::MEObjectPtrProperty;
        using Reflection::MEObjectProperty;
        using Reflection::MEProperty;
        using Reflection::MEPropertyCategory;
        using Reflection::PropertySpecifier;
        using Reflection::ReflectionSystem;

        bool IsAssetObject(const MEObject* object)
        {
            return object != nullptr
                && object->GetClass() != nullptr
                && object->GetClass()->IsA(Asset::StaticClass());
        }

        void ClearObjectPtr(MEObjectPtrProperty& property, void* ptrToPtr)
        {
            if (ptrToPtr == nullptr)
            {
                return;
            }

            if (property.GetPtrCategory() == MEObjectPtrCategory::Raw)
            {
                *static_cast<MEObject**>(ptrToPtr) = nullptr;
                return;
            }

            if (property.GetPtrCategory() == MEObjectPtrCategory::Shared)
            {
                auto* sharedPtr = static_cast<std::shared_ptr<MEObject>*>(ptrToPtr);
                if (sharedPtr != nullptr)
                {
                    sharedPtr->reset();
                }
            }
        }

        void StripObjectRecursive(
            MEObject& owner,
            const std::unordered_set<GUID, GUID::Hash>& templateGuidSet,
            PrefabCreateReport* outReport,
            const std::string& pathPrefix);

        void StripValue(
            void* valuePtr,
            MEProperty& property,
            MEObject& owner,
            const std::unordered_set<GUID, GUID::Hash>& templateGuidSet,
            PrefabCreateReport* outReport,
            const std::string& path)
        {
            if (valuePtr == nullptr)
            {
                return;
            }

            switch (property.GetCategory())
            {
            case MEPropertyCategory::ObjectPtr:
            {
                auto& objectPtrProperty = static_cast<MEObjectPtrProperty&>(property);
                MEObject* pointed = static_cast<MEObject*>(
                    const_cast<void*>(objectPtrProperty.GetConstPointingData(valuePtr)));

                if (property.HasSpecifier(PropertySpecifier::Instanced))
                {
                    if (pointed != nullptr)
                    {
                        StripObjectRecursive(*pointed, templateGuidSet, outReport, path);
                    }
                    return;
                }

                if (pointed == nullptr)
                {
                    return;
                }

                const GUID targetGuid = pointed->GetGuid();
                if (templateGuidSet.find(targetGuid) != templateGuidSet.end())
                {
                    return;
                }

                if (IsAssetObject(pointed))
                {
                    return;
                }

                if (outReport != nullptr)
                {
                    PrefabBrokenRef broken;
                    broken.OwnerTemplateGuid = owner.GetGuid();
                    broken.PropertyPath = path;
                    broken.PreviousTargetGuid = targetGuid;
                    broken.Reason = "ExternalNonAsset";
                    outReport->BrokenRefs.push_back(broken);
                }

                ClearObjectPtr(objectPtrProperty, valuePtr);
                return;
            }
            case MEPropertyCategory::Array:
            {
                auto& arrayProperty = static_cast<MEArrayProperty&>(property);
                MEProperty* inner = arrayProperty.GetInnerProperty();
                if (inner == nullptr)
                {
                    return;
                }

                const size_t count = arrayProperty.GetSize(valuePtr);
                for (size_t index = 0; index < count; ++index)
                {
                    void* elementPtr = arrayProperty.GetMutableElement(valuePtr, index);
                    StripValue(
                        elementPtr,
                        *inner,
                        owner,
                        templateGuidSet,
                        outReport,
                        path + "[" + std::to_string(index) + "]");
                }
                return;
            }
            case MEPropertyCategory::Object:
            {
                auto& objectProperty = static_cast<MEObjectProperty&>(property);
                const MEClass* valueClass = objectProperty.GetValueClass();
                if (valueClass == nullptr)
                {
                    return;
                }

                ReflectionSystem::Get().ForEachPropertyInHierarchy(
                    valueClass,
                    [&](const MEProperty& nestedProperty) -> bool
                    {
                        void* nestedPtr = nestedProperty.GetMutable(valuePtr);
                        StripValue(
                            nestedPtr,
                            const_cast<MEProperty&>(nestedProperty),
                            owner,
                            templateGuidSet,
                            outReport,
                            path + "." + nestedProperty.GetName());
                        return true;
                    });
                return;
            }
            default:
                return;
            }
        }

        void StripObjectRecursive(
            MEObject& owner,
            const std::unordered_set<GUID, GUID::Hash>& templateGuidSet,
            PrefabCreateReport* outReport,
            const std::string& pathPrefix)
        {
            const MEClass* classInfo = owner.GetClass();
            if (classInfo == nullptr)
            {
                return;
            }

            ReflectionSystem::Get().ForEachPropertyInHierarchy(
                classInfo,
                [&](const MEProperty& property) -> bool
                {
                    void* valuePtr = property.GetMutable(&owner);
                    const std::string path = pathPrefix.empty()
                        ? property.GetName()
                        : (pathPrefix + "." + property.GetName());
                    StripValue(
                        valuePtr,
                        const_cast<MEProperty&>(property),
                        owner,
                        templateGuidSet,
                        outReport,
                        path);
                    return true;
                });
        }
    }

    void PrefabReferencePolicy::StripExternalNonAssetRefs(
        Prefab& prefab,
        const std::unordered_set<GUID, GUID::Hash>& templateGuidSet,
        PrefabCreateReport* outReport)
    {
        for (const std::shared_ptr<GameObject>& gameObject : prefab.GetTemplateObjects())
        {
            if (gameObject)
            {
                StripObjectRecursive(*gameObject, templateGuidSet, outReport, std::string());
            }
        }
    }
}

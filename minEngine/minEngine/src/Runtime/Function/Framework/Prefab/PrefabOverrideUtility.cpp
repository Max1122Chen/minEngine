#include "Runtime/Function/Framework/Prefab/PrefabOverrideUtility.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabEditValidator.h"
#include "Runtime/Function/Framework/Prefab/PrefabObjectLookup.h"
#include "Runtime/Function/Framework/Prefab/PrefabPropertyPath.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    namespace
    {
        constexpr Serialization::SerializerOptions kOverridePropertyOptions{
            .enumAsString = true,
            .strictTypeCheck = true,
            .skipUnknownField = false,
        };

        GameObject* FindOwningGameObject(MEObject& object)
        {
            if (GameObject* gameObject = dynamic_cast<GameObject*>(&object))
            {
                return gameObject;
            }

            if (Component* component = dynamic_cast<Component*>(&object))
            {
                return dynamic_cast<GameObject*>(const_cast<MEObject*>(component->GetOuter()));
            }

            return nullptr;
        }

        bool IsRootTransformPath(const PrefabInstanceRecord& record, MEObject& instanceObject, std::string_view propertyPath)
        {
            GameObject* root = ObjectManager::HasInstance()
                ? ObjectManager::Get().FindObjectAs<GameObject>(record.RootInstanceGuid).get()
                : nullptr;
            if (root == nullptr)
            {
                return false;
            }

            SceneComponent* rootComponent = root->GetRootComponent();
            if (rootComponent == nullptr)
            {
                return false;
            }

            if (&instanceObject == rootComponent)
            {
                return propertyPath.rfind("m_Transform", 0) == 0;
            }

            if (&instanceObject == root)
            {
                if (propertyPath.rfind("m_Components/", 0) != 0)
                {
                    return false;
                }

                const std::string expected =
                    std::string("m_Components/") + rootComponent->GetGuid().ToString() + "/m_Transform";
                return propertyPath.rfind(expected, 0) == 0;
            }

            return false;
        }

        bool SerializePathToPayload(
            MEObject& owner,
            std::string_view propertyPath,
            std::string& outPayload,
            std::string* outError)
        {
            PrefabResolvedProperty resolved;
            if (!PrefabPropertyPath::TryResolve(owner, propertyPath, resolved, outError))
            {
                return false;
            }

            std::vector<uint8_t> buffer;
            const Serialization::SerializeResult result =
                Serialization::Serializer::SerializePropertyByPathToBuffer(
                    resolved.OwnerObject,
                    resolved.OwnerClass,
                    resolved.LeafPropertyName,
                    buffer,
                    kOverridePropertyOptions);
            if (!result.ok)
            {
                if (outError)
                {
                    *outError = result.message;
                }
                return false;
            }

            outPayload = PrefabPropertyPath::EncodeBinaryPayload(buffer);
            return true;
        }

        bool ApplyPayloadToPath(
            MEObject& owner,
            std::string_view propertyPath,
            std::string_view payload,
            std::string* outError)
        {
            PrefabResolvedProperty resolved;
            if (!PrefabPropertyPath::TryResolve(owner, propertyPath, resolved, outError))
            {
                return false;
            }

            std::vector<uint8_t> buffer;
            if (!PrefabPropertyPath::DecodeBinaryPayload(payload, buffer))
            {
                if (outError)
                {
                    *outError = "Invalid override payload encoding.";
                }
                return false;
            }

            std::vector<Serialization::PendingObjectRef> unresolvedRefs;
            const Serialization::SerializeResult result =
                Serialization::Serializer::DeserializePropertyByPathFromBuffer(
                    resolved.OwnerObject,
                    resolved.OwnerClass,
                    resolved.LeafPropertyName,
                    buffer,
                    unresolvedRefs,
                    kOverridePropertyOptions);
            if (!result.ok)
            {
                if (outError)
                {
                    *outError = result.message;
                }
                return false;
            }

            if (!unresolvedRefs.empty())
            {
                Serialization::Serializer::ResolvePendingObjectRefs(unresolvedRefs);
            }

            return true;
        }

        std::shared_ptr<Prefab> ResolvePrefabAsset(const GUID& prefabAssetGuid)
        {
            if (prefabAssetGuid.IsZero())
            {
                return nullptr;
            }

            if (ObjectManager::HasInstance())
            {
                if (std::shared_ptr<Prefab> loaded =
                        ObjectManager::Get().FindObjectAs<Prefab>(prefabAssetGuid))
                {
                    return loaded;
                }
            }

            if (AssetManager::HasInstance())
            {
                std::string error;
                std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByGUID(prefabAssetGuid, error);
                return std::dynamic_pointer_cast<Prefab>(asset);
            }

            return nullptr;
        }

        MEObject* FindTemplateObject(Prefab& prefab, const GUID& templateGuid)
        {
            return PrefabObjectLookup::FindInPrefab(prefab, templateGuid);
        }

        void UpsertOverride(
            PrefabInstanceRecord& record,
            const GUID& templateObjectGuid,
            std::string_view propertyPath,
            std::string_view payload)
        {
            for (PrefabPropertyOverride& overrideEntry : record.Overrides)
            {
                if (overrideEntry.Kind == EPrefabOverrideKind::PropertyValue
                    && overrideEntry.TemplateObjectGuid == templateObjectGuid
                    && overrideEntry.PropertyPath == propertyPath)
                {
                    overrideEntry.ValueJson = std::string(payload);
                    return;
                }
            }

            PrefabPropertyOverride created;
            created.Kind = EPrefabOverrideKind::PropertyValue;
            created.TemplateObjectGuid = templateObjectGuid;
            created.PropertyPath = std::string(propertyPath);
            created.ValueJson = std::string(payload);
            record.Overrides.push_back(std::move(created));
        }

        void RemoveOverride(
            PrefabInstanceRecord& record,
            const GUID& templateObjectGuid,
            std::string_view propertyPath)
        {
            auto& overrides = record.Overrides;
            overrides.erase(
                std::remove_if(
                    overrides.begin(),
                    overrides.end(),
                    [&](const PrefabPropertyOverride& entry)
                    {
                        return entry.Kind == EPrefabOverrideKind::PropertyValue
                            && entry.TemplateObjectGuid == templateObjectGuid
                            && entry.PropertyPath == propertyPath;
                    }),
                overrides.end());
        }

        bool ShouldCopyReflectedProperty(const Reflection::MEProperty& property)
        {
            using Reflection::MEObjectPtrCategory;
            using Reflection::MEPropertyCategory;
            using Reflection::PropertySpecifier;

            if (property.HasSpecifier(PropertySpecifier::Transient)
                || property.HasSpecifier(PropertySpecifier::Instanced))
            {
                return false;
            }

            // m_Name is Invisible for Inspector but must still propagate / revert.
            if (property.HasSpecifier(PropertySpecifier::Invisible) && property.GetName() != "m_Name")
            {
                return false;
            }

            switch (property.GetCategory())
            {
            case MEPropertyCategory::MulticastDelegate:
            case MEPropertyCategory::Array:
                return false;

            case MEPropertyCategory::ObjectPtr:
            {
                const auto& objectPtrProperty =
                    static_cast<const Reflection::MEObjectPtrProperty&>(property);
                if (objectPtrProperty.GetPtrCategory() != MEObjectPtrCategory::Shared)
                {
                    return false;
                }

                const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
                return valueClass != nullptr && valueClass->IsA(Asset::StaticClass());
            }

            case MEPropertyCategory::Primitive:
            case MEPropertyCategory::Object:
                return true;

            default:
                return false;
            }
        }

        bool CopyMappedObjectLeaves(
            PrefabInstanceRecord& record,
            MEObject& templateObject,
            MEObject& instanceObject,
            const GUID& templateObjectGuid,
            bool respectOverrides)
        {
            const Reflection::MEClass* objectClass = templateObject.GetClass();
            if (objectClass == nullptr)
            {
                return false;
            }

            bool anyWritten = false;
            Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
                objectClass,
                [&](const Reflection::MEProperty& property) -> bool
                {
                    if (!ShouldCopyReflectedProperty(property))
                    {
                        return true;
                    }

                    const std::string path = property.GetName();
                    if (IsRootTransformPath(record, instanceObject, path))
                    {
                        return true;
                    }

                    if (respectOverrides
                        && PrefabOverrideUtility::HasOverride(record, templateObjectGuid, path))
                    {
                        return true;
                    }

                    std::string payload;
                    if (!SerializePathToPayload(templateObject, path, payload, nullptr))
                    {
                        ME_LOG(
                            LogCore,
                            Warn,
                            "Prefab propagate: serialize '{}' on template {} failed; skipping.",
                            path,
                            templateObjectGuid.ToString());
                        return true;
                    }

                    if (!ApplyPayloadToPath(instanceObject, path, payload, nullptr))
                    {
                        ME_LOG(
                            LogCore,
                            Warn,
                            "Prefab propagate: apply '{}' to instance failed; skipping.",
                            path);
                        return true;
                    }

                    anyWritten = true;
                    return true;
                });

            return anyWritten;
        }

        bool CopyAllMappedLeavesFromTemplate(
            Prefab& prefab,
            PrefabInstanceRecord& record,
            bool respectOverrides,
            bool clearOverridesAfter)
        {
            bool anyWritten = false;

            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                MEObject* templateObject = FindTemplateObject(prefab, mapping.TemplateGuid);
                std::shared_ptr<MEObject> instanceObject =
                    ObjectManager::Get().FindObject(mapping.InstanceGuid);
                if (templateObject == nullptr || !instanceObject)
                {
                    if (templateObject == nullptr)
                    {
                        ME_LOG(
                            LogCore,
                            Warn,
                            "Prefab propagate: missing template object {}.",
                            mapping.TemplateGuid.ToString());
                    }
                    else
                    {
                        ME_LOG(
                            LogCore,
                            Warn,
                            "Prefab propagate: missing instance object {}.",
                            mapping.InstanceGuid.ToString());
                    }
                    continue;
                }

                if (CopyMappedObjectLeaves(
                        record,
                        *templateObject,
                        *instanceObject,
                        mapping.TemplateGuid,
                        respectOverrides))
                {
                    anyWritten = true;
                }
            }

            if (clearOverridesAfter)
            {
                record.Overrides.clear();
            }

            return anyWritten;
        }
    }

    bool PrefabOverrideUtility::HasOverride(
        const PrefabInstanceRecord& record,
        const GUID& templateObjectGuid,
        std::string_view propertyPath)
    {
        for (const PrefabPropertyOverride& entry : record.Overrides)
        {
            if (entry.Kind == EPrefabOverrideKind::PropertyValue
                && entry.TemplateObjectGuid == templateObjectGuid
                && entry.PropertyPath == propertyPath)
            {
                return true;
            }
        }
        return false;
    }

    bool PrefabOverrideUtility::TryRecordPropertyOverride(
        Scene& scene,
        MEObject& instanceObject,
        std::string_view propertyPath,
        std::string* outError)
    {
        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(scene, instanceObject.GetGuid());
        if (record == nullptr)
        {
            GameObject* owningGo = FindOwningGameObject(instanceObject);
            if (owningGo != nullptr)
            {
                record = PrefabUtility::FindInstanceRecord(scene, owningGo->GetGuid());
            }
        }

        if (record == nullptr)
        {
            return true;
        }

        const GUID templateGuid =
            PrefabPropertyPath::FindTemplateGuidForInstance(*record, instanceObject.GetGuid());
        if (templateGuid.IsZero())
        {
            if (outError)
            {
                *outError = "Instance object is not in PrefabObjectMappings.";
            }
            return false;
        }

        std::shared_ptr<Prefab> prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (!prefab)
        {
            std::string payload;
            if (!SerializePathToPayload(instanceObject, propertyPath, payload, outError))
            {
                return false;
            }
            UpsertOverride(*record, templateGuid, propertyPath, payload);
            return true;
        }

        MEObject* templateObject = FindTemplateObject(*prefab, templateGuid);
        if (templateObject == nullptr)
        {
            if (outError)
            {
                *outError = "Template object missing from Prefab asset.";
            }
            return false;
        }

        std::string instancePayload;
        std::string templatePayload;
        if (!SerializePathToPayload(instanceObject, propertyPath, instancePayload, outError))
        {
            return false;
        }
        if (!SerializePathToPayload(*templateObject, propertyPath, templatePayload, outError))
        {
            return false;
        }

        if (instancePayload == templatePayload)
        {
            RemoveOverride(*record, templateGuid, propertyPath);
        }
        else
        {
            UpsertOverride(*record, templateGuid, propertyPath, instancePayload);
        }

        return true;
    }

    bool PrefabOverrideUtility::RevertProperty(
        Scene& scene,
        MEObject& instanceObject,
        std::string_view propertyPath,
        std::string* outError)
    {
        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(scene, instanceObject.GetGuid());
        if (record == nullptr)
        {
            GameObject* owningGo = FindOwningGameObject(instanceObject);
            if (owningGo != nullptr)
            {
                record = PrefabUtility::FindInstanceRecord(scene, owningGo->GetGuid());
            }
        }

        if (record == nullptr)
        {
            if (outError)
            {
                *outError = "Object is not part of a Prefab instance.";
            }
            return false;
        }

        const GUID templateGuid =
            PrefabPropertyPath::FindTemplateGuidForInstance(*record, instanceObject.GetGuid());
        std::shared_ptr<Prefab> prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (templateGuid.IsZero() || !prefab)
        {
            if (outError)
            {
                *outError = "Cannot resolve Prefab template for revert.";
            }
            return false;
        }

        MEObject* templateObject = FindTemplateObject(*prefab, templateGuid);
        if (templateObject == nullptr)
        {
            if (outError)
            {
                *outError = "Template object missing.";
            }
            return false;
        }

        std::string templatePayload;
        if (!SerializePathToPayload(*templateObject, propertyPath, templatePayload, outError))
        {
            return false;
        }
        if (!ApplyPayloadToPath(instanceObject, propertyPath, templatePayload, outError))
        {
            return false;
        }

        RemoveOverride(*record, templateGuid, propertyPath);
        return true;
    }

    bool PrefabOverrideUtility::RevertInstance(
        Scene& scene,
        const GUID& rootInstanceGuid,
        std::string* outError)
    {
        PrefabInstanceRecord* record = PrefabUtility::FindInstanceRecord(scene, rootInstanceGuid);
        if (record == nullptr)
        {
            if (outError)
            {
                *outError = "Prefab instance record not found.";
            }
            return false;
        }

        std::shared_ptr<Prefab> prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (!prefab)
        {
            if (outError)
            {
                *outError = "Prefab asset unavailable for RevertInstance.";
            }
            return false;
        }

        CopyAllMappedLeavesFromTemplate(*prefab, *record, false, true);
        return true;
    }

    bool PrefabOverrideUtility::PropagateDefaultsToScene(const Prefab& prefab, Scene& scene)
    {
        bool anyWritten = false;

        for (PrefabInstanceRecord& record : scene.GetPrefabInstancesMutable())
        {
            if (record.PrefabAssetGuid != prefab.GetGuid())
            {
                continue;
            }

            if (CopyAllMappedLeavesFromTemplate(const_cast<Prefab&>(prefab), record, true, false))
            {
                anyWritten = true;
            }
        }

        return anyWritten;
    }

    bool PrefabOverrideUtility::PropagateDefaultsToEditorScene(const Prefab& prefab)
    {
        if (!SceneManager::HasInstance())
        {
            return false;
        }

        if (Scene* editorScene = SceneManager::Get().GetEditorScene())
        {
            return PropagateDefaultsToScene(prefab, *editorScene);
        }

        return false;
    }

    PrefabEditValidationResult PrefabOverrideUtility::ValidateEdit(
        Scene& scene,
        const PrefabInstanceRecord& record,
        const PrefabEditOp& op)
    {
        return PrefabEditValidator::ValidateEdit(scene, record, op);
    }
}

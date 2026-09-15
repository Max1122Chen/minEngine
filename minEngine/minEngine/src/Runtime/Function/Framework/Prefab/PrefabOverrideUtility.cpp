#include "Runtime/Function/Framework/Prefab/PrefabOverrideUtility.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Serialization/Serializer.h"
#include "Runtime/Function/Framework/Components/Component.h"
#include "Runtime/Function/Framework/Components/SceneComponent.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Prefab/Prefab.h"
#include "Runtime/Function/Framework/Prefab/PrefabEditValidator.h"
#include "Runtime/Function/Framework/Prefab/PrefabPropertyPath.h"
#include "Runtime/Function/Framework/Prefab/PrefabUtility.h"
#include "Runtime/Function/Framework/Scene/Scene.h"
#include "Runtime/Function/Framework/Scene/SceneManager.h"
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

        Prefab* ResolvePrefabAsset(const GUID& prefabAssetGuid)
        {
            if (prefabAssetGuid.IsZero())
            {
                return nullptr;
            }

            if (ObjectManager::HasInstance())
            {
                if (std::shared_ptr<Prefab> loaded = ObjectManager::Get().FindObjectAs<Prefab>(prefabAssetGuid))
                {
                    return loaded.get();
                }
            }

            if (AssetManager::HasInstance())
            {
                std::string error;
                std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByGUID(prefabAssetGuid, error);
                return dynamic_cast<Prefab*>(asset.get());
            }

            return nullptr;
        }

        MEObject* FindTemplateObject(Prefab& prefab, const GUID& templateGuid)
        {
            for (const std::shared_ptr<GameObject>& gameObject : prefab.GetTemplateObjects())
            {
                if (!gameObject)
                {
                    continue;
                }

                if (gameObject->GetGuid() == templateGuid)
                {
                    return gameObject.get();
                }

                for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
                {
                    if (component && component->GetGuid() == templateGuid)
                    {
                        return component.get();
                    }
                }
            }

            return nullptr;
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

        void CopyNonRootPropertiesFromTemplate(
            Prefab& prefab,
            PrefabInstanceRecord& record,
            bool clearOverrides)
        {
            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                MEObject* templateObject = FindTemplateObject(prefab, mapping.TemplateGuid);
                std::shared_ptr<MEObject> instanceObject =
                    ObjectManager::Get().FindObject(mapping.InstanceGuid);
                if (templateObject == nullptr || !instanceObject)
                {
                    continue;
                }

                GameObject* templateGo = dynamic_cast<GameObject*>(templateObject);
                GameObject* instanceGo = dynamic_cast<GameObject*>(instanceObject.get());
                if (templateGo != nullptr && instanceGo != nullptr)
                {
                    // Copy GO name always (not transform).
                    std::string namePayload;
                    if (SerializePathToPayload(*templateGo, "m_Name", namePayload, nullptr))
                    {
                        ApplyPayloadToPath(*instanceGo, "m_Name", namePayload, nullptr);
                    }

                    for (const std::shared_ptr<Component>& templateComponent : templateGo->GetAllComponents())
                    {
                        if (!templateComponent)
                        {
                            continue;
                        }

                        const GUID instanceComponentGuid =
                            PrefabPropertyPath::FindInstanceGuidForTemplate(record, templateComponent->GetGuid());
                        std::shared_ptr<MEObject> instanceComponentObject =
                            ObjectManager::Get().FindObject(instanceComponentGuid);
                        Component* instanceComponent = dynamic_cast<Component*>(instanceComponentObject.get());
                        if (instanceComponent == nullptr)
                        {
                            continue;
                        }

                        const bool isRootTransform =
                            instanceGo->GetGuid() == record.RootInstanceGuid
                            && instanceGo->GetRootComponent() == instanceComponent;

                        if (SceneComponent* templateSceneComponent =
                                dynamic_cast<SceneComponent*>(templateComponent.get()))
                        {
                            if (isRootTransform)
                            {
                                continue;
                            }

                            std::string transformPayload;
                            if (SerializePathToPayload(
                                    *templateSceneComponent, "m_Transform", transformPayload, nullptr))
                            {
                                ApplyPayloadToPath(
                                    *instanceComponent, "m_Transform", transformPayload, nullptr);
                            }
                        }
                    }
                }
            }

            if (clearOverrides)
            {
                record.Overrides.clear();
            }
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
            // Plain GO — nothing to record.
            return true;
        }

        const GUID templateGuid =
            PrefabPropertyPath::FindTemplateGuidForInstance(*record, instanceObject.GetGuid());
        if (templateGuid.IsZero())
        {
            // Component may map via its own Guid.
            if (outError)
            {
                *outError = "Instance object is not in PrefabObjectMappings.";
            }
            return false;
        }

        Prefab* prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (prefab == nullptr)
        {
            // Still record override without default comparison when asset unavailable.
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

        // Map component path: if editing component with plain path, compare same path on template component.
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
        Prefab* prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (templateGuid.IsZero() || prefab == nullptr)
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
        if (record == nullptr || record->RootInstanceGuid != rootInstanceGuid)
        {
            // Allow lookup by any mapped object, then require root match for full revert.
            if (record == nullptr)
            {
                if (outError)
                {
                    *outError = "Prefab instance record not found.";
                }
                return false;
            }
        }

        Prefab* prefab = ResolvePrefabAsset(record->PrefabAssetGuid);
        if (prefab == nullptr)
        {
            if (outError)
            {
                *outError = "Prefab asset unavailable for RevertInstance.";
            }
            return false;
        }

        CopyNonRootPropertiesFromTemplate(*prefab, *record, true);
        return true;
    }

    void PrefabOverrideUtility::PropagateDefaultsToScene(const Prefab& prefab, Scene& scene)
    {
        for (PrefabInstanceRecord& record : scene.GetPrefabInstancesMutable())
        {
            if (record.PrefabAssetGuid != prefab.GetGuid())
            {
                continue;
            }

            for (const PrefabObjectMapping& mapping : record.ObjectMappings)
            {
                MEObject* templateObject = FindTemplateObject(const_cast<Prefab&>(prefab), mapping.TemplateGuid);
                std::shared_ptr<MEObject> instanceObject = ObjectManager::Get().FindObject(mapping.InstanceGuid);
                if (templateObject == nullptr || !instanceObject)
                {
                    continue;
                }

                auto maybeCopy = [&](MEObject& templateOwner, MEObject& instanceOwner, std::string_view path)
                {
                    if (HasOverride(record, mapping.TemplateGuid, path))
                    {
                        return;
                    }

                    if (IsRootTransformPath(record, instanceOwner, path))
                    {
                        return;
                    }

                    std::string payload;
                    if (!SerializePathToPayload(templateOwner, path, payload, nullptr))
                    {
                        return;
                    }
                    ApplyPayloadToPath(instanceOwner, path, payload, nullptr);
                };

                if (GameObject* templateGo = dynamic_cast<GameObject*>(templateObject))
                {
                    GameObject* instanceGo = dynamic_cast<GameObject*>(instanceObject.get());
                    if (instanceGo == nullptr)
                    {
                        continue;
                    }

                    maybeCopy(*templateGo, *instanceGo, "m_Name");

                    for (const std::shared_ptr<Component>& templateComponent : templateGo->GetAllComponents())
                    {
                        if (!templateComponent)
                        {
                            continue;
                        }

                        const GUID instanceComponentGuid =
                            PrefabPropertyPath::FindInstanceGuidForTemplate(record, templateComponent->GetGuid());
                        auto instanceComponentObject = ObjectManager::Get().FindObject(instanceComponentGuid);
                        Component* instanceComponent = dynamic_cast<Component*>(instanceComponentObject.get());
                        if (instanceComponent == nullptr)
                        {
                            continue;
                        }

                        if (dynamic_cast<SceneComponent*>(templateComponent.get()) != nullptr)
                        {
                            const std::string path =
                                std::string("m_Components/") + templateComponent->GetGuid().ToString() + "/m_Transform";
                            // Compare/apply using component-local path on both sides.
                            if (HasOverride(record, templateComponent->GetGuid(), "m_Transform")
                                || HasOverride(record, mapping.TemplateGuid, path))
                            {
                                continue;
                            }

                            if (instanceGo->GetGuid() == record.RootInstanceGuid
                                && instanceGo->GetRootComponent() == instanceComponent)
                            {
                                continue;
                            }

                            std::string payload;
                            if (SerializePathToPayload(*templateComponent, "m_Transform", payload, nullptr))
                            {
                                ApplyPayloadToPath(*instanceComponent, "m_Transform", payload, nullptr);
                            }
                        }
                    }
                }
                else if (Component* templateComponent = dynamic_cast<Component*>(templateObject))
                {
                    Component* instanceComponent = dynamic_cast<Component*>(instanceObject.get());
                    if (instanceComponent == nullptr)
                    {
                        continue;
                    }

                    if (dynamic_cast<SceneComponent*>(templateComponent) != nullptr)
                    {
                        maybeCopy(*templateComponent, *instanceComponent, "m_Transform");
                    }
                }
            }
        }
    }

    void PrefabOverrideUtility::PropagateDefaultsToOpenScenes(const Prefab& prefab)
    {
        if (!SceneManager::HasInstance())
        {
            return;
        }

        if (Scene* editorScene = SceneManager::Get().GetEditorScene())
        {
            PropagateDefaultsToScene(prefab, *editorScene);
        }
    }

    PrefabEditValidationResult PrefabOverrideUtility::ValidateEdit(
        Scene& scene,
        const PrefabInstanceRecord& record,
        const PrefabEditOp& op)
    {
        return PrefabEditValidator::ValidateEdit(scene, record, op);
    }
}

#include "UI/Property/ObjectPtrWidget.h"

#include "UI/Property/PropertyRefPicker.h"

#include "imgui.h"

#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/MEProperties.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>

namespace minEngine
{
    AllowedClasses ObjectPtrWidget::ResolveDefaultAllowedClasses(const Reflection::MEClass* valueClass)
    {
        AllowedClasses allowed;
        if (valueClass != nullptr)
        {
            allowed.push_back(valueClass);
        }

        return allowed;
    }

    void ObjectPtrWidget::AppendCandidatesDedupe(std::vector<PropertyRefCandidate>& candidates,
                                                 const std::vector<PropertyRefCandidate>& toAppend)
    {
        for (const PropertyRefCandidate& candidate : toAppend)
        {
            const auto existing = std::find_if(
                candidates.begin(),
                candidates.end(),
                [&](const PropertyRefCandidate& entry) { return entry.Guid == candidate.Guid; });
            if (existing == candidates.end())
            {
                candidates.push_back(candidate);
            }
        }
    }

    void ObjectPtrWidget::SortCandidatesByDisplayName(std::vector<PropertyRefCandidate>& candidates)
    {
        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const PropertyRefCandidate& left, const PropertyRefCandidate& right)
            { return left.DisplayName < right.DisplayName; });
    }

    void ObjectPtrWidget::CollectAssetCandidates(const Reflection::MEClass* allowedClass,
                                                 std::vector<PropertyRefCandidate>& outCandidates)
    {
        if (allowedClass == nullptr || !allowedClass->IsA(Asset::StaticClass()))
        {
            return;
        }

        const std::vector<const AssetMeta*> assetMetas =
            AssetManager::Get().FindAssetMetasByClass(allowedClass);
        std::vector<PropertyRefCandidate> batch;
        batch.reserve(assetMetas.size());
        for (const AssetMeta* meta : assetMetas)
        {
            if (meta == nullptr)
            {
                continue;
            }

            PropertyRefCandidate candidate;
            candidate.Kind = PropertyRefCandidateKind::AssetMeta;
            candidate.Guid = meta->Guid;
            candidate.DisplayName = meta->AssetName;
            candidate.Meta = meta;
            batch.push_back(std::move(candidate));
        }

        AppendCandidatesDedupe(outCandidates, batch);
    }

    std::string ObjectPtrWidget::MakeObjectDisplayLabel(const MEObject& object)
    {
        if (!object.GetName().empty())
        {
            return object.GetName();
        }

        std::string label = "Object";
        if (object.GetClass() != nullptr)
        {
            const std::string& fullName = object.GetClass()->GetName();
            const size_t scopePos = fullName.rfind("::");
            label = scopePos != std::string::npos ? fullName.substr(scopePos + 2) : fullName;
        }

        return label + " " + object.GetGuid().ToString();
    }

    void ObjectPtrWidget::CollectObjectCandidates(const Reflection::MEClass* allowedClass,
                                                  std::vector<PropertyRefCandidate>& outCandidates)
    {
        if (allowedClass == nullptr || allowedClass->IsA(Asset::StaticClass()))
        {
            return;
        }

        if (!ObjectManager::HasInstance())
        {
            return;
        }

        std::vector<PropertyRefCandidate> batch;
        ObjectManager::Get().ForEachLiveObject(
            [&](const std::shared_ptr<MEObject>& object)
            {
                if (!object || !object->GetClass() || !object->GetClass()->IsA(allowedClass))
                {
                    return;
                }

                if (object->GetClass()->IsA(Asset::StaticClass()))
                {
                    return;
                }

                PropertyRefCandidate candidate;
                candidate.Kind = PropertyRefCandidateKind::LiveObject;
                candidate.Guid = object->GetGuid();
                candidate.DisplayName = MakeObjectDisplayLabel(*object);
                candidate.Object = object;
                batch.push_back(std::move(candidate));
            });

        AppendCandidatesDedupe(outCandidates, batch);
    }

    GUID ObjectPtrWidget::ReadCurrentGuid(const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                          void* propertyPtr)
    {
        const MEObject* object =
            static_cast<const MEObject*>(objectPtrProperty.GetConstPointingData(propertyPtr));
        if (object == nullptr)
        {
            return GUID::Zero();
        }

        return object->GetGuid();
    }

    bool ObjectPtrWidget::ApplySelection(const Reflection::MEObjectPtrProperty& objectPtrProperty,
                                         void* propertyPtr,
                                         const Reflection::MEClass* valueClass,
                                         const PropertyRefCandidate* selected,
                                         const ObjectPtrWidgetHooks& hooks)
    {
        if (valueClass == nullptr || propertyPtr == nullptr)
        {
            return false;
        }

        if (selected != nullptr && hooks.TryApplySelection && hooks.TryApplySelection(*selected))
        {
            return true;
        }

        const Reflection::MEObjectPtrCategory ptrCategory = objectPtrProperty.GetPtrCategory();
        if (ptrCategory != Reflection::MEObjectPtrCategory::Shared)
        {
            if (selected != nullptr)
            {
                ME_CORE_ERROR(
                    "ObjectPtrWidget: unsupported pointer category for property '{}'.",
                    objectPtrProperty.GetName());
            }

            return false;
        }

        if (selected == nullptr)
        {
            return valueClass->SetSharedPtr(std::shared_ptr<MEObject>(), propertyPtr);
        }

        if (selected->Kind == PropertyRefCandidateKind::LiveObject)
        {
            if (selected->Object == nullptr)
            {
                return false;
            }

            return valueClass->SetSharedPtr(selected->Object, propertyPtr);
        }

        if (selected->Meta == nullptr)
        {
            return false;
        }

        std::string errorMessage;
        const std::shared_ptr<Asset> asset =
            AssetManager::Get().LoadAssetByPath(selected->Meta->AssetPath, errorMessage);
        if (!asset)
        {
            ME_CORE_ERROR(
                "ObjectPtrWidget: failed to load asset '{}' for property '{}': {}",
                selected->Meta->AssetPath,
                objectPtrProperty.GetName(),
                errorMessage);
            return false;
        }

        const std::shared_ptr<void> assetAsVoid = asset;
        if (!valueClass->SetSharedPtr(assetAsVoid, propertyPtr))
        {
            ME_CORE_ERROR(
                "ObjectPtrWidget: SetSharedPtr failed for property '{}' on class '{}'.",
                objectPtrProperty.GetName(),
                valueClass->GetName());
            return false;
        }

        return true;
    }

    bool ObjectPtrWidget::Draw(const Reflection::MEProperty& property,
                               void* propertyPtr,
                               const PropertyEditSession& session,
                               const ObjectPtrWidgetHooks& hooks,
                               float itemWidth)
    {
        if (property.GetCategory() != Reflection::MEPropertyCategory::ObjectPtr)
        {
            return false;
        }

        const Reflection::MEObjectPtrProperty& objectPtrProperty =
            static_cast<const Reflection::MEObjectPtrProperty&>(property);
        const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
        if (valueClass == nullptr || propertyPtr == nullptr)
        {
            ImGui::TextUnformatted("Object reference type unresolved.");
            return false;
        }

        const AllowedClasses allowed = ResolveDefaultAllowedClasses(valueClass);
        std::vector<PropertyRefCandidate> candidates;
        for (const Reflection::MEClass* allowedClass : allowed)
        {
            if (allowedClass == nullptr)
            {
                continue;
            }

            if (allowedClass->IsA(Asset::StaticClass()))
            {
                CollectAssetCandidates(allowedClass, candidates);
            }
            else
            {
                CollectObjectCandidates(allowedClass, candidates);
            }
        }

        SortCandidatesByDisplayName(candidates);

        const GUID currentGuid = ReadCurrentGuid(objectPtrProperty, propertyPtr);
        const PropertyRefPickerResult pickerResult = PropertyRefPicker::DrawCombo(
            candidates,
            currentGuid,
            itemWidth,
            hooks.OnComboActivated);

        if (hooks.OnComboDeactivatedAfterEdit && ImGui::IsItemDeactivatedAfterEdit())
        {
            hooks.OnComboDeactivatedAfterEdit();
        }

        if (!pickerResult.ValueChanged)
        {
            return false;
        }

        const PropertyRefCandidate* selected = pickerResult.Selected;
        if (!ApplySelection(objectPtrProperty, propertyPtr, valueClass, selected, hooks))
        {
            return false;
        }

        if (hooks.OnMarkDirty)
        {
            hooks.OnMarkDirty();
        }
        else
        {
            session.MarkDirty();
        }

        if (hooks.OnRenderStateDirty)
        {
            hooks.OnRenderStateDirty();
        }

        if (hooks.OnSelectionCommitted)
        {
            hooks.OnSelectionCommitted(pickerResult.SelectionChanged);
        }

        return true;
    }
}

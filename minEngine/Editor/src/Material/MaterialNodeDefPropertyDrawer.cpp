#include "MaterialNodeDefPropertyDrawer.h"

#include "MaterialEditor.h"
#include "UI/Property/PropertyEditPolicy.h"
#include "UI/Property/PropertyEditSession.h"
#include "UI/Property/PropertyEditTypes.h"
#include "UI/Property/PropertyValueWidget.h"

#include "imgui.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>
#include <string>

namespace minEngine
{
    namespace
    {
        constexpr float kFieldWidth = MaterialNodeDefPropertyDrawer::kFieldWidth;

        bool ShouldSkipNodeDefProperty(const Reflection::MEProperty& property)
        {
            if (!PropertyEditPolicy::ShouldShow(property, EditorPropertyEditContextKind::AssetDefaults))
            {
                return true;
            }

            const std::string& name = property.GetName();
            if (name == "m_Inputs" || name == "m_Outputs")
            {
                return true;
            }

            if (property.GetCategory() == Reflection::MEPropertyCategory::Array)
            {
                return true;
            }

            if (property.GetCategory() == Reflection::MEPropertyCategory::Object)
            {
                const Reflection::MEObjectProperty& objectProperty =
                    static_cast<const Reflection::MEObjectProperty&>(property);
                Reflection::MEClass* valueClass = objectProperty.GetValueClass();
                if (PropertyValueWidget::IsLinearColorStruct(valueClass))
                {
                    return false;
                }

                return true;
            }

            return false;
        }

        bool DrawAssetRef(
            const Reflection::MEObjectPtrProperty& objectPtrProperty,
            void* propertyPtr,
            MaterialEditor& /*materialEditor*/)
        {
            const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
            if (!valueClass || !valueClass->IsA(Asset::StaticClass()))
            {
                ImGui::TextUnformatted("Unsupported object reference.");
                return false;
            }

            const std::string& typeName = valueClass->GetName();
            const std::vector<AssetMeta*> assetMetas = AssetManager::Get().FindAssetMetasByType(typeName);

            const Asset* currentAsset =
                static_cast<const Asset*>(objectPtrProperty.GetConstPointingData(propertyPtr));
            const AssetMeta* currentMeta = currentAsset ? currentAsset->GetMeta() : nullptr;
            const char* previewLabel = currentMeta ? currentMeta->AssetName.c_str() : "None";
            const GUID selectedGuid = currentMeta ? currentMeta->Guid : GUID::Zero();

            bool changed = false;
            ImGui::SetNextItemWidth(kFieldWidth);
            if (ImGui::BeginCombo("##AssetRefCombo", previewLabel))
            {
                if (ImGui::Selectable("None", currentMeta == nullptr))
                {
                    if (objectPtrProperty.GetPtrCategory() == Reflection::MEObjectPtrCategory::Shared)
                    {
                        changed = valueClass->SetSharedPtr(std::shared_ptr<MEObject>(), propertyPtr);
                    }
                }

                for (const AssetMeta* meta : assetMetas)
                {
                    const bool selected = currentMeta && meta->Guid == selectedGuid;
                    ImGui::PushID(meta->Guid.ToString().c_str());
                    if (ImGui::Selectable(meta->AssetName.c_str(), selected))
                    {
                        std::string errorMessage;
                        std::shared_ptr<Asset> asset =
                            AssetManager::Get().LoadAssetByPath(meta->AssetPath, errorMessage);
                        if (asset && objectPtrProperty.GetPtrCategory() == Reflection::MEObjectPtrCategory::Shared)
                        {
                            changed = valueClass->SetSharedPtr(asset, propertyPtr);
                        }
                    }
                    ImGui::PopID();
                }

                ImGui::EndCombo();
            }

            return changed;
        }

        bool DrawProperty(const Reflection::MEProperty& property, void* propertyPtr, MaterialEditor& materialEditor)
        {
            if (ShouldSkipNodeDefProperty(property))
            {
                return false;
            }

            ImGui::PushID(property.GetName().c_str());

            if (ImGui::BeginTable("##NodeDefProperty", 2, ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(PropertyEditPolicy::GetDisplayName(property));
                ImGui::TableSetColumnIndex(1);

                bool changed = false;
                switch (property.GetCategory())
                {
                    case Reflection::MEPropertyCategory::Primitive:
                    case Reflection::MEPropertyCategory::Object:
                        changed = PropertyValueWidget::Draw(property, propertyPtr, kFieldWidth);
                        break;
                    case Reflection::MEPropertyCategory::ObjectPtr:
                        changed = DrawAssetRef(
                            static_cast<const Reflection::MEObjectPtrProperty&>(property),
                            propertyPtr,
                            materialEditor);
                        break;
                    default:
                        ImGui::TextDisabled("-");
                        break;
                }

                ImGui::EndTable();
                ImGui::PopID();
                return changed;
            }

            ImGui::PopID();
            return false;
        }
    }

    bool MaterialNodeDefPropertyDrawer::DrawProperties(
        MaterialGraphNodeDef* nodeDef,
        MaterialEditor& materialEditor,
        const PropertyEditSession& editSession)
    {
        if (!nodeDef || !nodeDef->GetClass())
        {
            return false;
        }

        bool changed = false;
        Reflection::ReflectionSystem::Get().ForEachPropertyInHierarchy(
            nodeDef->GetClass()->GetName(),
            [&](const Reflection::MEProperty& property) -> bool
            {
                void* valuePtr = property.GetMutable(static_cast<void*>(nodeDef));
                if (valuePtr == nullptr)
                {
                    return true;
                }

                changed |= DrawProperty(property, valuePtr, materialEditor);
                return true;
            });

        if (changed)
        {
            editSession.MarkDirty();
        }

        return changed;
    }
}

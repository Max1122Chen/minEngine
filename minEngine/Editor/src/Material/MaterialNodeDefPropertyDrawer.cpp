#include "MaterialNodeDefPropertyDrawer.h"

#include "MaterialEditor.h"

#include "imgui.h"

#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "Runtime/Resource/Asset.h"
#include "Runtime/Resource/AssetManager.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace minEngine
{
    namespace
    {
        constexpr float kFieldWidth = MaterialNodeDefPropertyDrawer::kFieldWidth;

        std::string GetShortTypeName(const std::string& fullTypeName)
        {
            const size_t colon = fullTypeName.rfind(':');
            if (colon != std::string::npos && colon + 1 < fullTypeName.size())
            {
                return fullTypeName.substr(colon + 1);
            }

            return fullTypeName;
        }

        bool ShouldSkipNodeDefProperty(const Reflection::MEProperty& property)
        {
            if (property.HasSpecifier(Reflection::PropertySpecifier::Invisible))
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

        bool IsSignedIntegerTypeName(const std::string& shortTypeName)
        {
            return shortTypeName == "int"
                || shortTypeName == "int32"
                || shortTypeName == "int16"
                || shortTypeName == "long"
                || shortTypeName == "int64";
        }

        bool DrawPrimitiveProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
        {
            const std::string shortTypeName = GetShortTypeName(primitiveProperty.primitiveTypeName);
            ImGui::SetNextItemWidth(kFieldWidth);

            if (IsSignedIntegerTypeName(shortTypeName))
            {
                return ImGui::DragInt("##Value", static_cast<int*>(propertyPtr), 1);
            }

            if (shortTypeName == "uint32")
            {
                return ImGui::DragScalar(
                    "##Value",
                    ImGuiDataType_U32,
                    propertyPtr,
                    1.0f,
                    nullptr,
                    nullptr,
                    "%u");
            }

            if (shortTypeName == "float")
            {
                return ImGui::DragFloat("##Value", static_cast<float*>(propertyPtr), 0.01f, 0.0f, 0.0f, "%.3f");
            }

            if (shortTypeName == "double")
            {
                return ImGui::DragScalar("##Value", ImGuiDataType_Double, propertyPtr, 0.01);
            }

            if (shortTypeName == "bool")
            {
                return ImGui::Checkbox("##Value", static_cast<bool*>(propertyPtr));
            }

            if (shortTypeName == "string" || shortTypeName == "std::string")
            {
                std::string* stringValue = static_cast<std::string*>(propertyPtr);
                char textBuffer[256] = {};
                std::strncpy(textBuffer, stringValue->c_str(), sizeof(textBuffer) - 1);
                if (ImGui::InputText("##Value", textBuffer, sizeof(textBuffer)))
                {
                    *stringValue = textBuffer;
                    return true;
                }

                return false;
            }

            ImGui::TextDisabled("Unsupported: %s", shortTypeName.c_str());
            return false;
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
                ImGui::TextUnformatted(property.GetName().c_str());
                ImGui::TableSetColumnIndex(1);

                bool changed = false;
                switch (property.GetCategory())
                {
                    case Reflection::MEPropertyCategory::Primitive:
                        changed = DrawPrimitiveProperty(
                            static_cast<const Reflection::MEPrimitiveProperty&>(property),
                            propertyPtr);
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
        MaterialEditor& materialEditor)
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
            materialEditor.NotifyGraphChanged();
        }

        return changed;
    }
}

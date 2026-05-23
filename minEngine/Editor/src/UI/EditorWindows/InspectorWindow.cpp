#include "InspectorWindow.h"
#include "Runtime/Resource/AssetManager.h"

namespace minEngine
{
    void InspectorWindow::OnDraw()
    {
        DrawGameObjectDetails(m_Editor.GetSelectedGameObject());
    }

    void InspectorWindow::DrawGameObjectDetails(GameObject* gameObject)
    {
        ImGui::Begin(m_Title.c_str());

        if (!gameObject)
        {
            ImGui::TextUnformatted("No selected GameObject.");
            ImGui::End();
            return;
        }

        const bool requestRenameByHotkey = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && ImGui::IsKeyPressed(ImGuiKey_F2, false);
        if (requestRenameByHotkey)
        {
            BeginRenameSelectedGameObject(*gameObject);
        }

        if (m_RenameTargetGameObjectId != gameObject->GetID())
        {
            m_IsRenamingSelectedGameObject = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 7.0f));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.21f, 0.31f, 0.45f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.37f, 0.53f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.23f, 0.34f, 0.49f, 1.0f));

        const std::string selectedName = m_Editor.GetSelectedGameObjectName();
        if (m_IsRenamingSelectedGameObject && m_RenameTargetGameObjectId == gameObject->GetID())
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.27f, 0.40f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.23f, 0.34f, 0.49f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.21f, 0.31f, 0.45f, 1.0f));

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (m_RequestRenameFocus)
            {
                ImGui::SetKeyboardFocusHere();
                m_RequestRenameFocus = false;
            }

            const bool committed = ImGui::InputText("##SelectedGameObjectRename",
                m_RenameBuffer,
                sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

            if (committed || ImGui::IsItemDeactivatedAfterEdit())
            {
                m_Editor.RenameGameObject(gameObject->GetID(), m_RenameBuffer);
                m_IsRenamingSelectedGameObject = false;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                m_IsRenamingSelectedGameObject = false;
            }

            ImGui::PopStyleColor(3);
        }
        else
        {
            const std::string headerLabel = "  " + selectedName + "##SelectedGameObjectHeader";
            ImGui::Selectable(headerLabel.c_str(), true, ImGuiSelectableFlags_SpanAllColumns);
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                BeginRenameSelectedGameObject(*gameObject);
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->GetID()));

        ImGui::Spacing();

        // Add Component Section
        const std::vector<std::string> componentTypeNames = m_Editor.GetAllComponentTypeNames();
        ImGui::SeparatorText("Add Component");
        if (!componentTypeNames.empty())
        {
            if (std::find(componentTypeNames.begin(), componentTypeNames.end(), m_SelectedAddComponentTypeName) == componentTypeNames.end())
            {
                m_SelectedAddComponentTypeName = componentTypeNames.front();
            }

            ImGui::PushItemWidth(260.0f);
            const std::string selectedDisplayName = GetShortTypeName(m_SelectedAddComponentTypeName);
            if (ImGui::BeginCombo("##AddComponentCombo", selectedDisplayName.c_str()))
            {
                for (const std::string& typeName : componentTypeNames)
                {
                    const bool isSelected = (typeName == m_SelectedAddComponentTypeName);
                    const std::string displayName = GetShortTypeName(typeName);
                    if (ImGui::Selectable(displayName.c_str(), isSelected))
                    {
                        m_SelectedAddComponentTypeName = typeName;
                    }

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            if (ImGui::Button("Add Component"))
            {
                m_Editor.AddComponentToSelectedGameObject(m_SelectedAddComponentTypeName);
            }
        }
        else
        {
            ImGui::TextUnformatted("No reflected Component derived types found.");
        }

        Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();

        // GameObject's RootComponent Section
        
        if (gameObject->GetRootComponent())
        {
            ImGui::Spacing();
            ImGui::SeparatorText("Root Transform");
            std::string tableId = "RootComponentTable##" + std::to_string(gameObject->GetID());
            if (ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
                
                SceneComponent* rootComponent = gameObject->GetRootComponent();
                const Reflection::MEClass* classInfo = rootComponent->GetClass();
                bool valueChanged = false;
                if (classInfo)
                {
                    reflectionSystem.ForEachPropertyInHierarchy(classInfo->GetName(),
                    [&](const Reflection::MEProperty& property) -> bool
                    {
                        if(property.GetName() == "m_Transform")
                        {
                            void* valuePtr = property.GetMutable(static_cast<void*>(rootComponent));
                            valueChanged |= DrawProperty(property, valuePtr);
                        }
                        return true;
                    });
                    if(valueChanged)
                    {
                        rootComponent->MarkRenderStateDirty(); // Mark render state dirty to ensure changes are reflected in the editor viewport
                    }
                }
                else
                {
                    ImGui::TextUnformatted("Root component type info missing.");
                }
                ImGui::EndTable();
            }
            
        }

        // Components Section
        ImGui::Spacing();
        ImGui::SeparatorText("Components");

        for (const std::shared_ptr<Component>& component : gameObject->GetAllComponents())
        {
            if (!component)
            {
                continue;
            }

            const Reflection::MEClass* classInfo = component->GetClass();
            if (classInfo == nullptr)
            {
                ImGui::TextUnformatted("Component type info missing.");
                continue;
            }

            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.26f, 0.36f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.23f, 0.33f, 0.46f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.20f, 0.30f, 0.42f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 3.0f));
            const std::string headerLabel = GetShortTypeName(classInfo->GetName()) + "##component_" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            const bool componentOpen = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);
            if (!componentOpen)
            {
                continue;
            }

            const std::string tableId = "ComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
            if (!ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
            {
                continue;
            }

            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
            bool hasAnyReflectedField = false;
            MEObject* componentObject = static_cast<MEObject*>(component.get());
            if (componentObject == nullptr)
            {
                ImGui::EndTable();
                continue;
            }
            const Reflection::MEClass* compClass = componentObject->GetClass();
            if (compClass == nullptr)
            {
                ImGui::TextUnformatted("Component class info missing.");
                ImGui::EndTable();
                continue;
            }
            const std::string& compClassName = compClass->GetName();
            bool valueChanged = false;
            reflectionSystem.ForEachPropertyInHierarchy(compClassName,
            [&](const Reflection::MEProperty& property) -> bool
            {
                hasAnyReflectedField = true;
                void* valuePtr = property.GetMutable(componentObject);
                valueChanged |= DrawProperty(property, valuePtr);
                return true;
            });
            if(valueChanged)
            {
                if(compClass->IsA(SceneComponent::StaticClass()))
                {
                    SceneComponent* sceneComponent = static_cast<SceneComponent*>(componentObject);
                    sceneComponent->MarkRenderStateDirty(); // Mark render state dirty to ensure changes are reflected in the editor viewport
                }
            }

            TryDrawComponentContextMenu(*component);
            
            if (!hasAnyReflectedField)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Info");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted("No reflected fields.");
            }

            ImGui::EndTable();
        }

        ImGui::End();
    }


    bool InspectorWindow::DrawProperty(const Reflection::MEProperty &property, void *propertyPtr)
    {
        // Skip invisible properties
        if(property.HasSpecifier(Reflection::PropertySpecifier::Invisible))
        {
            return false;
        }

        ImGui::PushID(property.GetName().c_str());
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(property.GetName().c_str());
        ImGui::TableSetColumnIndex(1);

        bool valueChanged = false;
        switch(property.GetCategory())
        {
            case Reflection::MEPropertyCategory::Primitive:
                valueChanged = DrawPrimitiveProperty(static_cast<const Reflection::MEPrimitiveProperty&>(property), propertyPtr);
                break;
            case Reflection::MEPropertyCategory::Object:
                valueChanged = DrawObjectProperty(static_cast<const Reflection::MEObjectProperty&>(property), propertyPtr);
                break;
            case Reflection::MEPropertyCategory::ObjectPtr:
                valueChanged = DrawObjectPtrProperty(static_cast<const Reflection::MEObjectPtrProperty&>(property), propertyPtr);
                break;
            case Reflection::MEPropertyCategory::Array:
                valueChanged = DrawArrayProperty(static_cast<const Reflection::MEArrayProperty&>(property), propertyPtr);
                break;
        }
        ImGui::PopID();
        return valueChanged;
    }

    bool InspectorWindow::DrawPrimitiveProperty(const Reflection::MEPrimitiveProperty &primitiveProperty, void *propertyPtr)
    {
        std::string shortTypeName = GetShortTypeName(primitiveProperty.primitiveTypeName);
        if (shortTypeName == "int"
            || shortTypeName == "int32"
            || shortTypeName == "int16"
            || shortTypeName == "long"
            || shortTypeName == "int64")
        {
            return DrawIntProperty(primitiveProperty, propertyPtr);
        }
        else if (shortTypeName == "uint32")
        {
            if (ImGui::DragScalar("##Value", ImGuiDataType_U32, propertyPtr, 1.0f, nullptr, nullptr, "%u"))
            {
                m_Editor.MarkSceneDirty();
                return true;
            }
            return false;
        }
        else if(shortTypeName == "float")
        {
            return DrawFloatProperty(primitiveProperty, propertyPtr);
        }
        else if(shortTypeName == "double")
        {
            return DrawDoubleProperty(primitiveProperty, propertyPtr);
        }
        else if(shortTypeName == "bool")
        {
            return DrawBoolProperty(primitiveProperty, propertyPtr);
        }
        else if (shortTypeName == "string" || shortTypeName == "std::string")
        {
            return DrawStringProperty(primitiveProperty, propertyPtr);
        }
        else if(shortTypeName == "Vector2")
        {
            return DrawVector2Property(primitiveProperty, propertyPtr);
        }
        else if(shortTypeName == "Vector3")
        {
            return DrawVector3Property(primitiveProperty, propertyPtr);
        }
        else if(shortTypeName == "Vector4")
        {
            return DrawVector4Property(primitiveProperty, propertyPtr);
        }
        else
        {
            ImGui::TextUnformatted(("Unsupported primitive type: " + shortTypeName).c_str());
        }
        return false;
    }

    bool InspectorWindow::DrawIntProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        if (ImGui::DragInt("##Value", static_cast<int*>(propertyPtr), 1))
        {
            m_Editor.MarkSceneDirty();
            return true;
        }
        return false;
    }

    bool InspectorWindow::DrawFloatProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        if (ImGui::DragFloat("##Value", static_cast<float*>(propertyPtr), 0.1f))
        {
            m_Editor.MarkSceneDirty();
            return true;
        }
        return false;
    }

    bool InspectorWindow::DrawDoubleProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        if (ImGui::DragScalar("##Value", ImGuiDataType_Double, propertyPtr, 0.1f))
        {
            m_Editor.MarkSceneDirty();
            return true;
        }
        return false;
    }

    bool InspectorWindow::DrawBoolProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        if (ImGui::Checkbox("##Value", static_cast<bool*>(propertyPtr)))
        {
            m_Editor.MarkSceneDirty();
            return true;
        }
        return false;
    }

    bool InspectorWindow::DrawStringProperty(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        std::string* stringValue = static_cast<std::string*>(propertyPtr);
        char textBuffer[256] = {};
        std::strncpy(textBuffer, stringValue->c_str(), sizeof(textBuffer) - 1);
        if (ImGui::InputText("##Value", textBuffer, sizeof(textBuffer)))
        {
            *stringValue = textBuffer;
            m_Editor.MarkSceneDirty();
            return true;
        }
        return false;
    }

    bool InspectorWindow::DrawVector2Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        Vector2* value = static_cast<Vector2*>(propertyPtr);
        float data[2] = {value->x, value->y};
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool valueChanged = false;
        if (ImGui::DragFloat2("##Value", data, 0.1f))
        {
            value->x = data[0];
            value->y = data[1];
            m_Editor.MarkSceneDirty();
            valueChanged = true;
        }
        ImGui::PopStyleVar(2);
        return valueChanged;
    }

    bool InspectorWindow::DrawVector3Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        Vector3* value = static_cast<Vector3*>(propertyPtr);
        float data[3] = {value->x, value->y, value->z};
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool valueChanged = false;
        if (ImGui::DragFloat3("##Value", data, 0.1f))
        {
            value->x = data[0];
            value->y = data[1];
            value->z = data[2];
            m_Editor.MarkSceneDirty();
            valueChanged = true;
        }
        ImGui::PopStyleVar(2);
        return valueChanged;
    }

    bool InspectorWindow::DrawVector4Property(const Reflection::MEPrimitiveProperty& primitiveProperty, void* propertyPtr)
    {
        Vector4* value = static_cast<Vector4*>(propertyPtr);
        float data[4] = {value->x, value->y, value->z, value->w};
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool valueChanged = false;
        if (ImGui::DragFloat4("##Value", data, 0.1f))
        {
            value->x = data[0];
            value->y = data[1];
            value->z = data[2];
            value->w = data[3];
            m_Editor.MarkSceneDirty();
            valueChanged = true;
        }
        ImGui::PopStyleVar(2);
        return valueChanged;
    }

    bool InspectorWindow::DrawObjectProperty(const Reflection::MEObjectProperty& objectProperty, void* propertyPtr)
    {
        Reflection::MEClass* valueClass = objectProperty.GetValueClass();
        if (valueClass)
        {
            bool valueChanged = false;
            Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
            reflectionSystem.ForEachPropertyInHierarchy(valueClass, 
                [&](const Reflection::MEProperty& property) -> bool
            {
                void* valuePtr = property.GetMutable(propertyPtr);
                valueChanged |= DrawProperty(property, valuePtr);
                return true;
            });
            return valueChanged;
        }
        return false;
    }

    bool InspectorWindow::DrawObjectPtrProperty(const Reflection::MEObjectPtrProperty& objectPtrProperty, void* propertyPtr)
    {
        const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
        if (valueClass)
        {
            if (valueClass->IsA(Asset::StaticClass()))
            {
                return DrawAssetRef(objectPtrProperty, propertyPtr);
            }
            else
            {
                // TODO: handle non-asset object references (e.g. reference to another component or game object). This requires a way to select the target object in the editor, which is more complex than selecting an asset. For now we will just display a placeholder text.
                ImGui::TextUnformatted("Object references are not supported in this version.");
            }
        }
        return false;
    }

    bool InspectorWindow::DrawAssetRef(const Reflection::MEObjectPtrProperty &objectPtrProperty, void *propertyPtr)
    {
        // TODO: infer the "Asset Type" from the typeName
        const Reflection::MEClass* valueClass = objectPtrProperty.GetValueClass();
        const std::string& typeName = valueClass->GetName();
        const Asset* currentAsset = static_cast<const Asset*>(objectPtrProperty.GetConstPointingData(propertyPtr));
        const AssetMeta* currentAssetMeta = currentAsset ? currentAsset->GetMeta() : nullptr;
        std::string selectedAssetName = currentAssetMeta ? currentAssetMeta->AssetName : "None";
        GUID selectedGuid = currentAssetMeta ? currentAssetMeta->Guid : GUID::Zero();
        
        const std::vector<AssetMeta*> assetMetas = AssetManager::Get().FindAssetMetasByType(typeName);
        bool valueChanged = false;
        if (!assetMetas.empty())
        {
            // Use guid as the unique identifier for the asset reference, and display the asset name in the combo box. If the current selected guid is not in the list of available assets (e.g. the asset has been deleted), display "None".
            if(std::find_if(assetMetas.begin(), assetMetas.end(), [&](const AssetMeta* meta) { return meta->Guid == selectedGuid; }) == assetMetas.end())
            {
                selectedAssetName = "None";
            }

            ImGui::PushItemWidth(260.0f);
            if (ImGui::BeginCombo("##AssetRefCombo", selectedAssetName.c_str()))
            {
                for (const AssetMeta* meta : assetMetas)
                {
                    const bool isSelected = (meta->Guid == selectedGuid);
                    ImGui::PushID(meta->Guid.ToString().c_str());
                    if (ImGui::Selectable(meta->AssetName.c_str(), isSelected))
                    {
                        selectedAssetName = meta->AssetName;
                        std::string errorMessage;
                        std::shared_ptr<Asset> asset = AssetManager::Get().LoadAssetByPath(meta->AssetPath, errorMessage);
                        if(asset)
                        {
                            // TODO: Implement this
                            if (objectPtrProperty.GetPtrCategory() == Reflection::MEObjectPtrCategory::Raw)
                            {
                                *static_cast<void**>(propertyPtr) = asset.get();
                                valueChanged = true;
                            }
                            else if (objectPtrProperty.GetPtrCategory() == Reflection::MEObjectPtrCategory::Shared)
                            {
                                valueChanged = valueClass->SetSharedPtr(asset, propertyPtr);
                                if (valueChanged != true)
                                {
                                    ME_ERROR("Failed to set shared pointer for property '%s'", objectPtrProperty.GetName().c_str());
                                }
                            }
                        }
                    }
                    ImGui::PopID();

                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
        }   
        return valueChanged;
    }

    bool InspectorWindow::DrawArrayProperty(const Reflection::MEArrayProperty& arrayProperty, void* propertyPtr)
    {
        ImGui::TextUnformatted("Array properties are not supported in this version.");
        return false;
    }

    bool InspectorWindow::TryDrawComponentContextMenu(Component &component)
    {
        if (ImGui::BeginPopupContextItem(("ComponentContextMenu##" + std::to_string(reinterpret_cast<uintptr_t>(&component))).c_str()))
        {
            if (ImGui::MenuItem("Remove"))
            {
                if(m_Editor.RemoveComponentFromGO(*component.GetOwner(), component))
                {
                    ME_CORE_INFO("Component '{}' removed from GameObject '{}'.", component.GetClass()->GetName(), component.GetOwner()->GetName());
                }
            }
            ImGui::EndPopup();
            return true;
         }
         return false;
    }

    std::string InspectorWindow::GetShortTypeName(const std::string& fullTypeName)
    {
        const size_t scopePos = fullTypeName.rfind("::");
        if (scopePos == std::string::npos)
        {
            return fullTypeName;
        }

        return fullTypeName.substr(scopePos + 2);
    }
}
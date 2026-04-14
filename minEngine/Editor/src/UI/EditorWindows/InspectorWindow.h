#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <limits>

namespace minEngine
{
    inline std::string GetShortTypeName(const std::string& fullTypeName)
    {
        const size_t scopePos = fullTypeName.rfind("::");
        if (scopePos == std::string::npos)
        {
            return fullTypeName;
        }

        return fullTypeName.substr(scopePos + 2);
    }

    class InspectorWindow final : public EditorWindow
    {
    public:
        explicit InspectorWindow(Editor& editor)
            : EditorWindow(editor)
        {
        }

        const std::string& GetId() const override
        {
            return m_Id;
        }

        const std::string& GetTitle() const override
        {
            return m_Title;
        }

        void OnDraw() override
        {
            ImGui::Begin(m_Title.c_str());

            const std::shared_ptr<GameObject> gameObject = m_Editor.GetSelectedGameObject();
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

            if (m_RenameTargetGameObjectId != gameObject->m_ID)
            {
                m_IsRenamingSelectedGameObject = false;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 7.0f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.21f, 0.31f, 0.45f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.37f, 0.53f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.23f, 0.34f, 0.49f, 1.0f));

            const std::string selectedName = m_Editor.GetSelectedGameObjectName();
            if (m_IsRenamingSelectedGameObject && m_RenameTargetGameObjectId == gameObject->m_ID)
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
                    m_Editor.RenameGameObject(gameObject->m_ID, m_RenameBuffer);
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

            ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->m_ID));

            ImGui::Spacing();

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

            ImGui::Spacing();

            if (const std::shared_ptr<SceneComponent> rootComponent = gameObject->GetRootComponent())
            {
                ImGui::SeparatorText("Transform");
                Transform transform = rootComponent->GetTransform();

                float position[3] = {transform.Position.x, transform.Position.y, transform.Position.z};
                float rotation[3] = {transform.Rotation.x, transform.Rotation.y, transform.Rotation.z};
                float scale[3] = {transform.Scale.x, transform.Scale.y, transform.Scale.z};

                bool transformDirty = false;
                if (ImGui::BeginTable("TransformFields", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Position");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    transformDirty |= ImGui::DragFloat3("##TransformPosition", position, 0.05f);
                    ImGui::PopStyleVar(2);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Rotation");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    transformDirty |= ImGui::DragFloat3("##TransformRotation", rotation, 0.5f);
                    ImGui::PopStyleVar(2);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Scale");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    transformDirty |= ImGui::DragFloat3("##TransformScale", scale, 0.05f, 0.01f, 100.0f);
                    ImGui::PopStyleVar(2);

                    ImGui::EndTable();
                }

                if (transformDirty)
                {
                    transform.Position = Vector3(position[0], position[1], position[2]);
                    transform.Rotation = Vector3(rotation[0], rotation[1], rotation[2]);
                    transform.Scale = Vector3(scale[0], scale[1], scale[2]);
                    gameObject->SetTransform(transform);
                    m_Editor.MarkSceneDirty();
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Components");

            for (const std::shared_ptr<Component>& component : gameObject->GetComponents())
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
                Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
                reflectionSystem.ForEachPropertyInHierarchy(compClassName,
                [&](const Reflection::MEProperty& property) -> bool
                {
                    hasAnyReflectedField = true;
                    void* valuePtr = property.GetMutable(componentObject);
                    DrawProperty(property, valuePtr);
                    return true;
                });

                
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

    private:
        static constexpr uint64_t kInvalidGameObjectId = std::numeric_limits<uint64_t>::max();

        void DrawProperty(const Reflection::MEProperty& property, void* propertyPtr)
        {
            Reflection::MEPrimitiveProperty* primitiveProperty = dynamic_cast<Reflection::MEPrimitiveProperty*>(const_cast<Reflection::MEProperty*>(&property));
            std::string shortPropertyTypeName;
            if (primitiveProperty)            
            {
                shortPropertyTypeName = GetShortTypeName(primitiveProperty->primitiveTypeName);
            }
            else
            {
                shortPropertyTypeName = "Unsupported Type";
            }

            ImGui::PushID(property.GetName().c_str());
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(property.GetName().c_str());
            ImGui::TableSetColumnIndex(1);

            if (shortPropertyTypeName == "int")
            {
                if (ImGui::DragInt("##Value", static_cast<int*>(propertyPtr), 1))
                {
                    m_Editor.MarkSceneDirty();
                }
            }
            else if (shortPropertyTypeName == "float")
            {
                if (ImGui::DragFloat("##Value", static_cast<float*>(propertyPtr), 0.1f))
                {
                    m_Editor.MarkSceneDirty();
                }
            }
            else if (shortPropertyTypeName == "double")
            {
                if (ImGui::DragScalar("##Value", ImGuiDataType_Double, propertyPtr, 0.1f))
                {
                    m_Editor.MarkSceneDirty();
                }
            }
            else if (shortPropertyTypeName == "bool")
            {
                if (ImGui::Checkbox("##Value", static_cast<bool*>(propertyPtr)))
                {
                    m_Editor.MarkSceneDirty();
                }
            }
            else if (shortPropertyTypeName == "string")
            {
                std::string* stringValue = static_cast<std::string*>(propertyPtr);
                char textBuffer[256] = {};
                std::strncpy(textBuffer, stringValue->c_str(), sizeof(textBuffer) - 1);
                if (ImGui::InputText("##Value", textBuffer, sizeof(textBuffer)))
                {
                    *stringValue = textBuffer;
                    m_Editor.MarkSceneDirty();
                }
            }
            else if (shortPropertyTypeName == "Vector2")
            {
                Vector2* value = static_cast<Vector2*>(propertyPtr);
                float data[2] = {value->x, value->y};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::DragFloat2("##Value", data, 0.1f))
                {
                    value->x = data[0];
                    value->y = data[1];
                    m_Editor.MarkSceneDirty();
                }
                ImGui::PopStyleVar(2);
            }
            else if (shortPropertyTypeName == "Vector3")
            {
                Vector3* value = static_cast<Vector3*>(propertyPtr);
                float data[3] = {value->x, value->y, value->z};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::DragFloat3("##Value", data, 0.1f))
                {
                    value->x = data[0];
                    value->y = data[1];
                    value->z = data[2];
                    m_Editor.MarkSceneDirty();
                }
                ImGui::PopStyleVar(2);
            }
            else if (shortPropertyTypeName == "Vector4")
            {
                Vector4* value = static_cast<Vector4*>(propertyPtr);
                float data[4] = {value->x, value->y, value->z, value->w};
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(8.0f, 6.0f));
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::DragFloat4("##Value", data, 0.1f))
                {
                    value->x = data[0];
                    value->y = data[1];
                    value->z = data[2];
                    value->w = data[3];
                    m_Editor.MarkSceneDirty();
                }
                ImGui::PopStyleVar(2);
            }
            else
            {
                const std::string unsupportedLabel = "unsupported: " + property.GetName();
                ImGui::TextUnformatted(unsupportedLabel.c_str());
            }

            ImGui::PopID();
        }

        void BeginRenameSelectedGameObject(const GameObject& gameObject)
        {
            m_IsRenamingSelectedGameObject = true;
            m_RenameTargetGameObjectId = gameObject.m_ID;
            std::memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
            std::strncpy(m_RenameBuffer, gameObject.GetName().c_str(), sizeof(m_RenameBuffer) - 1);
            m_RequestRenameFocus = true;
        }

        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
        std::string m_SelectedAddComponentTypeName;
        bool m_IsRenamingSelectedGameObject = false;
        bool m_RequestRenameFocus = false;
        uint64_t m_RenameTargetGameObjectId = kInvalidGameObjectId;
        char m_RenameBuffer[256] = {};
    };
}

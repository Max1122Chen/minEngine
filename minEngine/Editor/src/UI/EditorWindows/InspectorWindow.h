#pragma once

#include "Core.h"

#include "imgui.h"

#include "Editor.h"
#include "EditorWindow.h"

#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Framework/GameObject/GameObject.h"
#include "Runtime/Function/Framework/Components/Component.h"

#include <algorithm>
#include <cstring>

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

            char nameBuffer[256] = {};
            const std::string selectedName = m_Editor.GetSelectedGameObjectName();
            std::strncpy(nameBuffer, selectedName.c_str(), sizeof(nameBuffer) - 1);

            ImGui::Text("Selected ID: %llu", static_cast<unsigned long long>(gameObject->m_ID));

            if (ImGui::BeginTable("InspectorMainFields", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted("Name");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::InputText("##GameObjectName", nameBuffer, sizeof(nameBuffer)))
                {
                    m_Editor.RenameSelectedGameObject(nameBuffer);
                }

                ImGui::EndTable();
            }

            ImGui::Separator();

            const std::vector<std::string> componentTypeNames = m_Editor.GetAllComponentTypeNames();
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

            ImGui::Separator();

            if (const std::shared_ptr<SceneComponent> rootComponent = gameObject->GetRootComponent())
            {
                Transform transform = rootComponent->GetTransform();

                float position[3] = {transform.Position.x, transform.Position.y, transform.Position.z};
                float rotation[3] = {transform.Rotation.x, transform.Rotation.y, transform.Rotation.z};
                float scale[3] = {transform.Scale.x, transform.Scale.y, transform.Scale.z};

                bool transformDirty = false;
                if (ImGui::BeginTable("TransformFields", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
                {
                    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Position");
                    ImGui::TableSetColumnIndex(1);
                    transformDirty |= ImGui::DragFloat3("##TransformPosition", position, 0.05f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Rotation");
                    ImGui::TableSetColumnIndex(1);
                    transformDirty |= ImGui::DragFloat3("##TransformRotation", rotation, 0.5f);

                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted("Scale");
                    ImGui::TableSetColumnIndex(1);
                    transformDirty |= ImGui::DragFloat3("##TransformScale", scale, 0.05f, 0.01f, 100.0f);

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

            for (const std::shared_ptr<Component>& component : gameObject->GetComponents())
            {
                if (!component)
                {
                    continue;
                }

                const Reflection::TypeInfo* typeInfo = Reflection::ReflectionSystem::Get().GetTypeInfoByTypeId(typeid(*component).name());
                if (typeInfo == nullptr)
                {
                    ImGui::TextUnformatted("Component type info missing.");
                    continue;
                }

                const std::string headerLabel = GetShortTypeName(typeInfo->name) + "##component_" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
                if (!ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    continue;
                }

                const std::string tableId = "ComponentTable##" + std::to_string(reinterpret_cast<uintptr_t>(component.get()));
                if (!ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
                {
                    continue;
                }

                ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, 0.35f);
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.65f);
                bool hasAnyReflectedField = false;
                const Reflection::ReflectionSystem& reflectionSystem = Reflection::ReflectionSystem::Get();
                reflectionSystem.ForEachFieldInHierarchy(typeInfo->name,
                    [&](const Reflection::TypeInfo& declaringType, const Reflection::FieldInfo& field)
                    {
                        void* declaredObjectPtr = reflectionSystem.CastObjectToType(component.get(), typeInfo->name, declaringType.name);
                        if (declaredObjectPtr == nullptr)
                        {
                            return true;
                        }

                        void* fieldPtr = Reflection::ReflectionSystem::GetFieldPtr(declaredObjectPtr, field);
                        if (fieldPtr == nullptr)
                        {
                            return true;
                        }

                        hasAnyReflectedField = true;
                        const std::string shortFieldTypeName = GetShortTypeName(field.typeName);
                        const bool isInheritedField = (declaringType.name != typeInfo->name);
                        const std::string displayFieldName = isInheritedField
                            ? (GetShortTypeName(declaringType.name) + "::" + field.name)
                            : field.name;

                        ImGui::PushID((declaringType.name + "::" + field.name).c_str());
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(displayFieldName.c_str());
                        ImGui::TableSetColumnIndex(1);

                        if (field.typeName == "int" || shortFieldTypeName == "int")
                        {
                            if (ImGui::DragInt("##Value", static_cast<int*>(fieldPtr)))
                            {
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "float" || shortFieldTypeName == "float")
                        {
                            if (ImGui::DragFloat("##Value", static_cast<float*>(fieldPtr), 0.1f))
                            {
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "double" || shortFieldTypeName == "double")
                        {
                            if (ImGui::DragScalar("##Value", ImGuiDataType_Double, fieldPtr, 0.1f))
                            {
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "bool" || shortFieldTypeName == "bool")
                        {
                            if (ImGui::Checkbox("##Value", static_cast<bool*>(fieldPtr)))
                            {
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "std::string" || shortFieldTypeName == "string")
                        {
                            std::string* stringValue = static_cast<std::string*>(fieldPtr);
                            char textBuffer[256] = {};
                            std::strncpy(textBuffer, stringValue->c_str(), sizeof(textBuffer) - 1);
                            if (ImGui::InputText("##Value", textBuffer, sizeof(textBuffer)))
                            {
                                *stringValue = textBuffer;
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "Vector2" || shortFieldTypeName == "Vector2")
                        {
                            Vector2* value = static_cast<Vector2*>(fieldPtr);
                            float data[2] = {value->x, value->y};
                            if (ImGui::DragFloat2("##Value", data, 0.1f))
                            {
                                value->x = data[0];
                                value->y = data[1];
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "Vector3" || shortFieldTypeName == "Vector3")
                        {
                            Vector3* value = static_cast<Vector3*>(fieldPtr);
                            float data[3] = {value->x, value->y, value->z};
                            if (ImGui::DragFloat3("##Value", data, 0.1f))
                            {
                                value->x = data[0];
                                value->y = data[1];
                                value->z = data[2];
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else if (field.typeName == "Vector4" || shortFieldTypeName == "Vector4")
                        {
                            Vector4* value = static_cast<Vector4*>(fieldPtr);
                            float data[4] = {value->x, value->y, value->z, value->w};
                            if (ImGui::DragFloat4("##Value", data, 0.1f))
                            {
                                value->x = data[0];
                                value->y = data[1];
                                value->z = data[2];
                                value->w = data[3];
                                m_Editor.MarkSceneDirty();
                            }
                        }
                        else
                        {
                            const std::string unsupportedLabel = "unsupported: " + field.typeName;
                            ImGui::TextUnformatted(unsupportedLabel.c_str());
                        }

                        ImGui::PopID();
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
        const std::string m_Id = "inspector";
        const std::string m_Title = "Inspector";
        std::string m_SelectedAddComponentTypeName;
    };
}

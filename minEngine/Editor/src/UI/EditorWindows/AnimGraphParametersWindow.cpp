#include "AnimGraphParametersWindow.h"

#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"
#include "Runtime/Core/Log/LogSystem.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace minEngine
{
    AnimGraphParametersWindow::AnimGraphParametersWindow(IEditorContext& context)
        : EditorWindow(context)
    {
        SetOpen(false);
    }

    std::string_view AnimGraphParametersWindow::GetOwnerModuleId() const
    {
        return AnimationGraphEditor::kModuleId;
    }

    void AnimGraphParametersWindow::OnDraw()
    {
        if (!EditorWindowTypography::BeginPanel(m_Context, m_Title.c_str()))
        {
            return;
        }

        EditorTypographyScope bodyTypography(m_Context.GetEditorAppearance(), EditorTypographyRole::Body);

        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor || !animGraphEditor->GetSession().HasOpenGraph())
        {
            ImGui::TextUnformatted("Open an AnimationGraph to edit parameters.");
            ImGui::End();
            return;
        }

        DrawSchemaTable();
        ImGui::End();
    }

    void AnimGraphParametersWindow::DrawSchemaTable()
    {
        AnimationGraphEditor* animGraphEditor = dynamic_cast<AnimationGraphEditor*>(
            m_Context.FindSubModule(AnimationGraphEditor::kModuleId));
        if (!animGraphEditor)
        {
            return;
        }

        AnimationGraph& graph = *animGraphEditor->GetSession().GraphAsset;
        ParameterSchema& schema = graph.GetSchema();
        std::vector<ParameterSchemaEntry>& entries = schema.GetEntriesMutable();

        auto packDefaultBytes = [](ParameterValueType type,
                                   bool boolValue,
                                   int32_t intValue,
                                   float floatValue) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> bytes;
            if (type == ParameterValueType::Bool)
            {
                bytes.push_back(static_cast<uint8_t>(boolValue ? 1 : 0));
            }
            else if (type == ParameterValueType::Int32)
            {
                bytes.resize(sizeof(int32_t));
                std::memcpy(bytes.data(), &intValue, sizeof(int32_t));
            }
            else
            {
                bytes.resize(sizeof(float));
                std::memcpy(bytes.data(), &floatValue, sizeof(float));
            }
            return bytes;
        };

        auto unpackDefault = [](const ParameterSchemaEntry& entry,
                                bool& outBool,
                                int32_t& outInt,
                                float& outFloat)
        {
            outBool = false;
            outInt = 0;
            outFloat = 0.0f;
            if (entry.DefaultBytes.empty())
            {
                return;
            }

            if (entry.Type == ParameterValueType::Bool && entry.DefaultBytes.size() >= 1)
            {
                outBool = entry.DefaultBytes[0] != 0;
            }
            else if (entry.Type == ParameterValueType::Int32
                     && entry.DefaultBytes.size() >= sizeof(int32_t))
            {
                std::memcpy(&outInt, entry.DefaultBytes.data(), sizeof(int32_t));
            }
            else if (entry.Type == ParameterValueType::Float
                     && entry.DefaultBytes.size() >= sizeof(float))
            {
                std::memcpy(&outFloat, entry.DefaultBytes.data(), sizeof(float));
            }
        };

        if (ImGui::Button("Add Entry"))
        {
            std::string baseName = "Param";
            std::string uniqueName = baseName;
            int suffix = 1;
            while (schema.FindEntryIndex(uniqueName) != SIZE_MAX)
            {
                uniqueName = baseName + "_" + std::to_string(suffix++);
            }

            ParameterSchemaEntry entry;
            entry.Name = uniqueName;
            entry.Type = ParameterValueType::Float;
            entry.DefaultBytes = packDefaultBytes(ParameterValueType::Float, false, 0, 0.0f);
            std::string error;
            if (schema.AddEntry(std::move(entry), &error))
            {
                animGraphEditor->NotifyGraphChanged();
            }
            else
            {
                ME_CORE_WARN("AnimGraphParameters: AddEntry failed: {}", error);
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Schema edits require Instance rebind at runtime.");

        if (!ImGui::BeginTable(
                "AnimGraphSchema",
                4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp
                    | ImGuiTableFlags_ScrollY))
        {
            return;
        }

        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn("Default", ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableHeadersRow();

        constexpr const char* kTypeLabels[] = {"Bool", "Int32", "Float"};

        for (int entryIndex = 0; entryIndex < static_cast<int>(entries.size()); ++entryIndex)
        {
            ParameterSchemaEntry& entry = entries[static_cast<size_t>(entryIndex)];
            ImGui::PushID(entryIndex);
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            char nameBuffer[128];
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", entry.Name.c_str());
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText(
                    "##Name",
                    nameBuffer,
                    sizeof(nameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue))
            {
                const std::string newName = nameBuffer;
                const size_t existing = schema.FindEntryIndex(newName);
                if (newName.empty())
                {
                    ME_CORE_WARN("AnimGraphParameters: name must be non-empty.");
                }
                else if (existing != SIZE_MAX && existing != static_cast<size_t>(entryIndex))
                {
                    ME_CORE_WARN("AnimGraphParameters: duplicate name '{}'.", newName);
                }
                else if (entry.Name != newName)
                {
                    entry.Name = newName;
                    animGraphEditor->NotifyGraphChanged();
                }
            }

            ImGui::TableSetColumnIndex(1);
            int typeIndex = static_cast<int>(entry.Type);
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::Combo("##Type", &typeIndex, kTypeLabels, IM_ARRAYSIZE(kTypeLabels)))
            {
                entry.Type = static_cast<ParameterValueType>(typeIndex);
                bool boolValue = false;
                int32_t intValue = 0;
                float floatValue = 0.0f;
                unpackDefault(entry, boolValue, intValue, floatValue);
                entry.DefaultBytes =
                    packDefaultBytes(entry.Type, boolValue, intValue, floatValue);
                animGraphEditor->NotifyGraphChanged();
            }

            ImGui::TableSetColumnIndex(2);
            bool boolValue = false;
            int32_t intValue = 0;
            float floatValue = 0.0f;
            unpackDefault(entry, boolValue, intValue, floatValue);
            ImGui::SetNextItemWidth(-1.0f);
            if (entry.Type == ParameterValueType::Bool)
            {
                if (ImGui::Checkbox("##DefaultBool", &boolValue))
                {
                    entry.DefaultBytes =
                        packDefaultBytes(entry.Type, boolValue, intValue, floatValue);
                    animGraphEditor->NotifyGraphChanged();
                }
            }
            else if (entry.Type == ParameterValueType::Int32)
            {
                if (ImGui::DragInt("##DefaultInt", &intValue))
                {
                    entry.DefaultBytes =
                        packDefaultBytes(entry.Type, boolValue, intValue, floatValue);
                    animGraphEditor->NotifyGraphChanged();
                }
            }
            else
            {
                if (ImGui::DragFloat("##DefaultFloat", &floatValue, 0.01f))
                {
                    entry.DefaultBytes =
                        packDefaultBytes(entry.Type, boolValue, intValue, floatValue);
                    animGraphEditor->NotifyGraphChanged();
                }
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("X"))
            {
                schema.RemoveEntryAt(static_cast<size_t>(entryIndex));
                animGraphEditor->NotifyGraphChanged();
                ImGui::PopID();
                break;
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }
}

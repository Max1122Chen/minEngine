#include "SubEditor/AnimationGraph/AnimGraphInspectorSource.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"
#include "Shell/IEditorContext.h"
#include "UI/Appearance/EditorTypographyScope.h"
#include "UI/Appearance/EditorWindowTypography.h"

#include "imgui.h"

#include "Runtime/Function/Framework/Project/EditorTypographyRole.h"
#include "Runtime/Function/Animation/AnimationClip.h"
#include "Runtime/Function/Framework/Parameters/ParameterSchema.h"
#include "Runtime/Function/Framework/Parameters/ParameterValueType.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace minEngine
{
    AnimGraphInspectorSource::AnimGraphInspectorSource(AnimationGraphEditor& animGraphEditor)
        : m_AnimGraphEditor(animGraphEditor)
    {
    }

    bool AnimGraphInspectorSource::HasInspectableSelection() const
    {
        // Empty selection still shows DefaultState; InspectorWindow only draws when true.
        return m_AnimGraphEditor.GetSession().HasOpenGraph();
    }

    void AnimGraphInspectorSource::DrawInspector()
    {
        IEditorContext* editorContext = m_AnimGraphEditor.GetEditorContext();
        if (editorContext == nullptr)
        {
            ImGui::Begin(kWindowTitle);
            ImGui::TextUnformatted("No animation graph editor context.");
            ImGui::End();
            return;
        }

        if (!EditorWindowTypography::BeginPanel(*editorContext, kWindowTitle))
        {
            return;
        }

        EditorTypographyScope bodyTypography(
            editorContext->GetEditorAppearance(),
            EditorTypographyRole::Body);

        if (!m_AnimGraphEditor.GetSession().HasOpenGraph())
        {
            ImGui::TextUnformatted("Open an AnimationGraph to edit details.");
            ImGui::End();
            return;
        }

        switch (m_AnimGraphEditor.GetSession().Selection.Kind)
        {
        case AnimGraphSelectionKind::None:
            DrawNoSelection();
            break;
        case AnimGraphSelectionKind::State:
            DrawStateDetails();
            break;
        case AnimGraphSelectionKind::Transition:
            DrawTransitionDetails(false);
            break;
        case AnimGraphSelectionKind::AnyStateTransition:
            DrawTransitionDetails(true);
            break;
        }

        ImGui::End();
    }

    void AnimGraphInspectorSource::DrawNoSelection()
    {
        AnimationGraph& graph = *m_AnimGraphEditor.GetSession().GraphAsset;
        AnimStateMachine& stateMachine = graph.GetStateMachine();

        ImGui::TextUnformatted("No selection.");
        ImGui::TextDisabled("Select a State or Transition on the canvas.");
        ImGui::Separator();

        const char* defaultPreview = stateMachine.DefaultStateName.empty()
            ? "(none)"
            : stateMachine.DefaultStateName.c_str();

        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("Default State", defaultPreview))
        {
            if (ImGui::Selectable("(none)", stateMachine.DefaultStateName.empty()))
            {
                if (!stateMachine.DefaultStateName.empty())
                {
                    stateMachine.DefaultStateName.clear();
                    m_AnimGraphEditor.NotifyGraphChanged();
                }
            }

            for (const AnimState& state : stateMachine.States)
            {
                const bool selected = (state.Name == stateMachine.DefaultStateName);
                if (ImGui::Selectable(state.Name.c_str(), selected))
                {
                    if (stateMachine.DefaultStateName != state.Name)
                    {
                        stateMachine.DefaultStateName = state.Name;
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    void AnimGraphInspectorSource::DrawStateDetails()
    {
        AnimationGraphEditorSession& session = m_AnimGraphEditor.GetSession();
        AnimState* state = session.GraphAsset->FindStateMutable(session.Selection.StateName);
        if (!state)
        {
            ImGui::TextDisabled("Selected state no longer exists.");
            return;
        }

        char nameBuffer[128];
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", state->Name.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string error;
            if (!m_AnimGraphEditor.RenameState(state->Name, nameBuffer, &error))
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", error.c_str());
            }
            // Rename may invalidate local pointer; refresh.
            state = session.GraphAsset->FindStateMutable(session.Selection.StateName);
            if (!state)
            {
                return;
            }
        }

        const std::vector<const AssetMeta*> clipMetas =
            AssetManager::Get().FindAssetMetasByType("AnimationClip");

        const char* clipPreview = "(none)";
        if (state->Clip && state->Clip->GetMeta())
        {
            clipPreview = state->Clip->GetMeta()->AssetName.c_str();
        }

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo("Clip", clipPreview))
        {
            if (ImGui::Selectable("(none)", state->Clip == nullptr))
            {
                if (state->Clip)
                {
                    state->Clip.reset();
                    m_AnimGraphEditor.NotifyGraphChanged();
                }
            }

            for (const AssetMeta* meta : clipMetas)
            {
                if (!meta)
                {
                    continue;
                }
                const bool selected =
                    state->Clip
                    && state->Clip->GetMeta()
                    && state->Clip->GetMeta()->AssetPath == meta->AssetPath;
                if (ImGui::Selectable(meta->AssetName.c_str(), selected))
                {
                    std::shared_ptr<AnimationClip> clip =
                        AssetManager::Get().LoadAsset<AnimationClip>(meta->AssetPath);
                    state->Clip = clip;
                    m_AnimGraphEditor.NotifyGraphChanged();
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::Checkbox("Loop", &state->bLoop))
        {
            m_AnimGraphEditor.NotifyGraphChanged();
        }

        if (ImGui::DragFloat("Speed", &state->Speed, 0.01f, 0.0f, 10.0f, "%.2f"))
        {
            m_AnimGraphEditor.NotifyGraphChanged();
        }
    }

    void AnimGraphInspectorSource::DrawTransitionDetails(bool anyState)
    {
        AnimationGraph& graph = *m_AnimGraphEditor.GetSession().GraphAsset;
        AnimStateMachine& stateMachine = graph.GetStateMachine();
        const int transitionIndex = m_AnimGraphEditor.GetSession().Selection.TransitionIndex;

        std::vector<AnimTransition>& transitions =
            anyState ? stateMachine.AnyStateTransitions : stateMachine.Transitions;
        if (transitionIndex < 0 || static_cast<size_t>(transitionIndex) >= transitions.size())
        {
            ImGui::TextDisabled("Selected transition no longer exists.");
            return;
        }

        AnimTransition& transition = transitions[static_cast<size_t>(transitionIndex)];

        if (anyState)
        {
            ImGui::TextUnformatted("From: (Any State)");
        }
        else
        {
            ImGui::Text("From: %s", transition.FromStateName.c_str());
            const bool canReverse =
                !transition.FromStateName.empty()
                && !transition.ToStateName.empty()
                && transition.FromStateName != transition.ToStateName;
            if (!canReverse)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Reverse"))
            {
                m_AnimGraphEditor.ReverseTransition();
            }
            if (!canReverse)
            {
                ImGui::EndDisabled();
            }
        }

        const char* toPreview =
            transition.ToStateName.empty() ? "(none)" : transition.ToStateName.c_str();
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("To", toPreview))
        {
            for (const AnimState& state : stateMachine.States)
            {
                const bool selected = (state.Name == transition.ToStateName);
                if (ImGui::Selectable(state.Name.c_str(), selected))
                {
                    if (transition.ToStateName != state.Name)
                    {
                        transition.ToStateName = state.Name;
                        m_AnimGraphEditor.NotifyGraphChanged();
                        m_AnimGraphEditor.InvalidateGraphCanvas(false);
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        if (ImGui::DragFloat(
                "Blend Duration",
                &transition.BlendDurationSeconds,
                0.01f,
                0.0f,
                5.0f,
                "%.2f s"))
        {
            m_AnimGraphEditor.NotifyGraphChanged();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Conditions");

        const std::vector<ParameterSchemaEntry>& schemaEntries = graph.GetSchema().GetEntries();

        if (ImGui::Button("Add Condition"))
        {
            AnimCondition condition;
            if (!schemaEntries.empty())
            {
                condition.ParamName = schemaEntries.front().Name;
            }
            transition.Conditions.push_back(std::move(condition));
            m_AnimGraphEditor.NotifyGraphChanged();
        }

        if (ImGui::BeginTable(
                "AnimConditions",
                4,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Param");
            ImGui::TableSetupColumn("Op");
            ImGui::TableSetupColumn("Operand");
            ImGui::TableSetupColumn("##remove", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableHeadersRow();

            constexpr const char* kOpLabels[] = {
                "Greater",
                "GreaterEqual",
                "Less",
                "LessEqual",
                "Equal",
                "NotEqual",
                "IsSet",
            };

            for (int conditionIndex = 0;
                 conditionIndex < static_cast<int>(transition.Conditions.size());
                 ++conditionIndex)
            {
                AnimCondition& condition = transition.Conditions[static_cast<size_t>(conditionIndex)];
                ImGui::PushID(conditionIndex);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                const char* paramPreview =
                    condition.ParamName.empty() ? "(none)" : condition.ParamName.c_str();
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##Param", paramPreview))
                {
                    for (const ParameterSchemaEntry& entry : schemaEntries)
                    {
                        const bool selected = (entry.Name == condition.ParamName);
                        if (ImGui::Selectable(entry.Name.c_str(), selected))
                        {
                            condition.ParamName = entry.Name;
                            m_AnimGraphEditor.NotifyGraphChanged();
                        }
                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::TableSetColumnIndex(1);
                int opIndex = static_cast<int>(condition.Op);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::Combo("##Op", &opIndex, kOpLabels, IM_ARRAYSIZE(kOpLabels)))
                {
                    condition.Op = static_cast<AnimConditionOp>(opIndex);
                    m_AnimGraphEditor.NotifyGraphChanged();
                }

                ImGui::TableSetColumnIndex(2);
                ParameterValueType paramType = ParameterValueType::Float;
                for (const ParameterSchemaEntry& entry : schemaEntries)
                {
                    if (entry.Name == condition.ParamName)
                    {
                        paramType = entry.Type;
                        break;
                    }
                }

                ImGui::SetNextItemWidth(-1.0f);
                if (paramType == ParameterValueType::Bool)
                {
                    if (ImGui::Checkbox("##OperandBool", &condition.OperandBool))
                    {
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                }
                else if (paramType == ParameterValueType::Int32)
                {
                    if (ImGui::DragInt("##OperandInt", &condition.OperandInt))
                    {
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                }
                else
                {
                    if (ImGui::DragFloat("##OperandFloat", &condition.OperandFloat, 0.01f))
                    {
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X"))
                {
                    transition.Conditions.erase(
                        transition.Conditions.begin() + static_cast<std::ptrdiff_t>(conditionIndex));
                    m_AnimGraphEditor.NotifyGraphChanged();
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
}

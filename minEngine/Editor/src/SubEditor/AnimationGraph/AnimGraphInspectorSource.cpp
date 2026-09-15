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
                    m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() {
                            graph.GetStateMachine().DefaultStateName.clear();
                            m_AnimGraphEditor.NotifyGraphChanged();
                            return true;
                        });
                }
            }

            for (const AnimState& state : stateMachine.States)
            {
                const bool selected = (state.Name == stateMachine.DefaultStateName);
                if (ImGui::Selectable(state.Name.c_str(), selected))
                {
                    if (stateMachine.DefaultStateName != state.Name)
                    {
                        const std::string newDefault = state.Name;
                        m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                            "m_StateMachine",
                            [&]() {
                                return m_AnimGraphEditor.SetDefaultStateName(newDefault);
                            });
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
            const std::string oldName = state->Name;
            const std::string newName = nameBuffer;
            std::string error;
            if (!m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                    "m_StateMachine",
                    [&]() { return m_AnimGraphEditor.RenameState(oldName, newName, &error); }))
            {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", error.c_str());
            }
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
                    const std::string stateName = state->Name;
                    m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() {
                            AnimState* mutableState = session.GraphAsset->FindStateMutable(stateName);
                            if (mutableState == nullptr)
                            {
                                return false;
                            }

                            mutableState->Clip.reset();
                            m_AnimGraphEditor.NotifyGraphChanged();
                            return true;
                        });
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
                    const std::string stateName = state->Name;
                    const std::string assetPath = meta->AssetPath;
                    m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() {
                            AnimState* mutableState = session.GraphAsset->FindStateMutable(stateName);
                            if (mutableState == nullptr)
                            {
                                return false;
                            }

                            mutableState->Clip = AssetManager::Get().LoadAsset<AnimationClip>(assetPath);
                            m_AnimGraphEditor.NotifyGraphChanged();
                            return true;
                        });
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        state = session.GraphAsset->FindStateMutable(session.Selection.StateName);
        if (!state)
        {
            return;
        }

        bool loop = state->bLoop;
        if (ImGui::Checkbox("Loop", &loop))
        {
            const std::string stateName = state->Name;
            m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() {
                    AnimState* mutableState = session.GraphAsset->FindStateMutable(stateName);
                    if (mutableState == nullptr)
                    {
                        return false;
                    }

                    mutableState->bLoop = loop;
                    m_AnimGraphEditor.NotifyGraphChanged();
                    return true;
                });
        }

        float speed = state->Speed;
        const bool speedChanged = ImGui::DragFloat("Speed", &speed, 0.01f, 0.0f, 10.0f, "%.2f");
        if (ImGui::IsItemActivated())
        {
            m_AnimGraphEditor.StoreOwnedPropertyUndoBefore("m_StateMachine");
        }
        if (speedChanged)
        {
            state->Speed = speed;
            m_AnimGraphEditor.NotifyGraphChanged();
        }
        m_AnimGraphEditor.TryCommitOwnedPropertyUndoAfterEdit("m_StateMachine");
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
                m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                    "m_StateMachine",
                    [&]() { return m_AnimGraphEditor.ReverseTransition(); });
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
                        const std::string newTo = state.Name;
                        const bool isAny = anyState;
                        const int index = transitionIndex;
                        m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                            "m_StateMachine",
                            [&]() {
                                AnimStateMachine& sm = graph.GetStateMachine();
                                std::vector<AnimTransition>& list =
                                    isAny ? sm.AnyStateTransitions : sm.Transitions;
                                if (index < 0 || static_cast<size_t>(index) >= list.size())
                                {
                                    return false;
                                }

                                list[static_cast<size_t>(index)].ToStateName = newTo;
                                m_AnimGraphEditor.NotifyGraphChanged();
                                m_AnimGraphEditor.InvalidateGraphCanvas(false);
                                return true;
                            });
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        float blendDuration = transition.BlendDurationSeconds;
        const bool blendChanged =
            ImGui::DragFloat("Blend Duration", &blendDuration, 0.01f, 0.0f, 5.0f, "%.2f s");
        if (ImGui::IsItemActivated())
        {
            m_AnimGraphEditor.StoreOwnedPropertyUndoBefore("m_StateMachine");
        }
        if (blendChanged)
        {
            transition.BlendDurationSeconds = blendDuration;
            m_AnimGraphEditor.NotifyGraphChanged();
        }
        m_AnimGraphEditor.TryCommitOwnedPropertyUndoAfterEdit("m_StateMachine");

        ImGui::Separator();
        ImGui::TextUnformatted("Conditions");

        const std::vector<ParameterSchemaEntry>& schemaEntries = graph.GetSchema().GetEntries();

        if (ImGui::Button("Add Condition"))
        {
            const bool isAny = anyState;
            const int index = transitionIndex;
            m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                "m_StateMachine",
                [&]() {
                    AnimStateMachine& sm = graph.GetStateMachine();
                    std::vector<AnimTransition>& list =
                        isAny ? sm.AnyStateTransitions : sm.Transitions;
                    if (index < 0 || static_cast<size_t>(index) >= list.size())
                    {
                        return false;
                    }

                    AnimCondition condition;
                    if (!schemaEntries.empty())
                    {
                        condition.ParamName = schemaEntries.front().Name;
                    }
                    list[static_cast<size_t>(index)].Conditions.push_back(std::move(condition));
                    m_AnimGraphEditor.NotifyGraphChanged();
                    return true;
                });
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
                            const std::string paramName = entry.Name;
                            const bool isAny = anyState;
                            const int tIndex = transitionIndex;
                            const int cIndex = conditionIndex;
                            m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                                "m_StateMachine",
                                [&]() {
                                    AnimStateMachine& sm = graph.GetStateMachine();
                                    std::vector<AnimTransition>& list =
                                        isAny ? sm.AnyStateTransitions : sm.Transitions;
                                    if (tIndex < 0 || static_cast<size_t>(tIndex) >= list.size()
                                        || cIndex < 0
                                        || static_cast<size_t>(cIndex) >= list[static_cast<size_t>(tIndex)].Conditions.size())
                                    {
                                        return false;
                                    }

                                    list[static_cast<size_t>(tIndex)].Conditions[static_cast<size_t>(cIndex)].ParamName =
                                        paramName;
                                    m_AnimGraphEditor.NotifyGraphChanged();
                                    return true;
                                });
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
                    const AnimConditionOp newOp = static_cast<AnimConditionOp>(opIndex);
                    const bool isAny = anyState;
                    const int tIndex = transitionIndex;
                    const int cIndex = conditionIndex;
                    m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() {
                            AnimStateMachine& sm = graph.GetStateMachine();
                            std::vector<AnimTransition>& list =
                                isAny ? sm.AnyStateTransitions : sm.Transitions;
                            if (tIndex < 0 || static_cast<size_t>(tIndex) >= list.size()
                                || cIndex < 0
                                || static_cast<size_t>(cIndex) >= list[static_cast<size_t>(tIndex)].Conditions.size())
                            {
                                return false;
                            }

                            list[static_cast<size_t>(tIndex)].Conditions[static_cast<size_t>(cIndex)].Op = newOp;
                            m_AnimGraphEditor.NotifyGraphChanged();
                            return true;
                        });
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
                    bool operand = condition.OperandBool;
                    if (ImGui::Checkbox("##OperandBool", &operand))
                    {
                        const bool isAny = anyState;
                        const int tIndex = transitionIndex;
                        const int cIndex = conditionIndex;
                        m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                            "m_StateMachine",
                            [&]() {
                                AnimStateMachine& sm = graph.GetStateMachine();
                                std::vector<AnimTransition>& list =
                                    isAny ? sm.AnyStateTransitions : sm.Transitions;
                                if (tIndex < 0 || static_cast<size_t>(tIndex) >= list.size()
                                    || cIndex < 0
                                    || static_cast<size_t>(cIndex)
                                        >= list[static_cast<size_t>(tIndex)].Conditions.size())
                                {
                                    return false;
                                }

                                list[static_cast<size_t>(tIndex)].Conditions[static_cast<size_t>(cIndex)].OperandBool =
                                    operand;
                                m_AnimGraphEditor.NotifyGraphChanged();
                                return true;
                            });
                    }
                }
                else if (paramType == ParameterValueType::Int32)
                {
                    int operand = condition.OperandInt;
                    const bool changed = ImGui::DragInt("##OperandInt", &operand);
                    if (ImGui::IsItemActivated())
                    {
                        m_AnimGraphEditor.StoreOwnedPropertyUndoBefore("m_StateMachine");
                    }
                    if (changed)
                    {
                        condition.OperandInt = operand;
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                    m_AnimGraphEditor.TryCommitOwnedPropertyUndoAfterEdit("m_StateMachine");
                }
                else
                {
                    float operand = condition.OperandFloat;
                    const bool changed = ImGui::DragFloat("##OperandFloat", &operand, 0.01f);
                    if (ImGui::IsItemActivated())
                    {
                        m_AnimGraphEditor.StoreOwnedPropertyUndoBefore("m_StateMachine");
                    }
                    if (changed)
                    {
                        condition.OperandFloat = operand;
                        m_AnimGraphEditor.NotifyGraphChanged();
                    }
                    m_AnimGraphEditor.TryCommitOwnedPropertyUndoAfterEdit("m_StateMachine");
                }

                ImGui::TableSetColumnIndex(3);
                if (ImGui::SmallButton("X"))
                {
                    const bool isAny = anyState;
                    const int tIndex = transitionIndex;
                    const int cIndex = conditionIndex;
                    m_AnimGraphEditor.SubmitOwnedPropertyMutation(
                        "m_StateMachine",
                        [&]() {
                            AnimStateMachine& sm = graph.GetStateMachine();
                            std::vector<AnimTransition>& list =
                                isAny ? sm.AnyStateTransitions : sm.Transitions;
                            if (tIndex < 0 || static_cast<size_t>(tIndex) >= list.size()
                                || cIndex < 0
                                || static_cast<size_t>(cIndex)
                                    >= list[static_cast<size_t>(tIndex)].Conditions.size())
                            {
                                return false;
                            }

                            list[static_cast<size_t>(tIndex)].Conditions.erase(
                                list[static_cast<size_t>(tIndex)].Conditions.begin()
                                + static_cast<std::ptrdiff_t>(cIndex));
                            m_AnimGraphEditor.NotifyGraphChanged();
                            return true;
                        });
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }
}

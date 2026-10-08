#include "AnimationGraphEditor.h"

#include "Commands/EditorObjectPropertyApply.h"
#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "EditorGUIManager.h"
#include "Services/Animation/AnimationGraphEditorServices.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"

#include "UI/EditorWindows/AnimGraphWindow.h"
#include "UI/EditorWindows/AnimGraphParametersWindow.h"

#include "imgui.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Core/Object/MEObject.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "Runtime/Function/Animation/AnimationGraph.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace minEngine
{
    AnimationGraphEditor::AnimationGraphEditor()
        : m_InspectorSource(*this)
    {
    }

    void AnimationGraphEditor::Register(IEditorContext& context)
    {
        m_Context = &context;
        EditorGUIManager& gui = context.GetGUIManager();
        gui.RegisterWindow(std::make_unique<AnimGraphWindow>(context));
        gui.RegisterWindow(std::make_unique<AnimGraphParametersWindow>(context));
    }

    void AnimationGraphEditor::OnActivate(IEditorContext& context)
    {
        (void)context;
        OnEnterMode();
    }

    void AnimationGraphEditor::OnDeactivate(IEditorContext& context)
    {
        (void)context;
        OnExitMode();
    }

    void AnimationGraphEditor::RegisterCommands(IEditorContext& context)
    {
        EditorCommandBinding saveGraphCommand;
        saveGraphCommand.Name = "Save Animation Graph";
        saveGraphCommand.Chord = { ImGuiKey_S, true, false, false };
        saveGraphCommand.CanExecute = [this]() { return m_Session.HasOpenGraph(); };
        saveGraphCommand.Execute = [this]() { SaveActiveGraph(); };
        context.GetInputHub().RegisterActiveSubModuleCommand(std::move(saveGraphCommand));
    }

    void AnimationGraphEditor::UnregisterCommands(IEditorContext& context)
    {
        context.GetInputHub().ClearActiveSubModuleCommands();
    }

    void AnimationGraphEditor::ApplyDefaultLayout(IEditorContext& context, ImGuiID dockspaceId)
    {
        (void)context;
        EditorDockLayout::BuildAnimationGraphEditingLayout(dockspaceId);
    }

    bool AnimationGraphEditor::CanOpenAsset(const AssetMeta& meta) const
    {
        return meta.AssetType == "AnimationGraph";
    }

    bool AnimationGraphEditor::OpenAsset(const AssetMeta& meta)
    {
        OpenSession(&meta);
        return m_Session.HasOpenGraph();
    }

    void AnimationGraphEditor::OnEnterMode()
    {
        RefreshGraphList();
        // DocumentHost owns which graphs are open; do not auto-open the first asset.
    }

    void AnimationGraphEditor::OnExitMode()
    {
        // Keep graph sessions alive for multi-document host (ED-F11).
        StashActiveSession();
        ClearSelection();
    }

    void AnimationGraphEditor::StashActiveSession()
    {
        if (!m_Session.HasOpenGraph())
        {
            return;
        }
        m_SessionsByKey[m_Session.AssetPath] = m_Session;
    }

    bool AnimationGraphEditor::ActivateStoredSession(const std::string& assetKey)
    {
        if (m_Session.HasOpenGraph() && m_Session.AssetPath == assetKey)
        {
            return true;
        }

        StashActiveSession();
        const auto it = m_SessionsByKey.find(assetKey);
        if (it == m_SessionsByKey.end())
        {
            return false;
        }
        m_Session = it->second;
        InvalidateGraphCanvas();
        return m_Session.HasOpenGraph();
    }

    void AnimationGraphEditor::DiscardStoredSession(const std::string& assetKey)
    {
        m_SessionsByKey.erase(assetKey);
        if (m_Session.AssetPath == assetKey)
        {
            m_Session.Clear();
            InvalidateGraphCanvas();
        }
    }

    bool AnimationGraphEditor::IsStoredSessionDirty(const std::string& assetKey) const
    {
        if (m_Session.AssetPath == assetKey)
        {
            return m_Session.Dirty;
        }
        const auto it = m_SessionsByKey.find(assetKey);
        return it != m_SessionsByKey.end() && it->second.Dirty;
    }

    void AnimationGraphEditor::Shutdown()
    {
        m_Session.Clear();
        m_GraphMetas.clear();
        m_SelectedGraphIndex = -1;
        m_Context = nullptr;
    }

    void AnimationGraphEditor::InvalidateGraphCanvas(bool rebindGraph)
    {
        m_GraphCanvasInvalidated = true;
        if (rebindGraph)
        {
            m_GraphCanvasRebindPending = true;
        }
    }

    void AnimationGraphEditor::NotifyGraphChanged()
    {
        if (!m_Session.HasOpenGraph())
        {
            return;
        }
        m_Session.Dirty = true;
        if (!m_Session.AssetPath.empty())
        {
            m_SessionsByKey[m_Session.AssetPath] = m_Session;
        }
    }

    void AnimationGraphEditor::RefreshGraphList()
    {
        const std::vector<const AssetMeta*> graphs =
            AssetManager::Get().FindAssetMetasByType("AnimationGraph");
        m_GraphMetas.assign(graphs.begin(), graphs.end());
        std::sort(
            m_GraphMetas.begin(),
            m_GraphMetas.end(),
            [](const AssetMeta* lhs, const AssetMeta* rhs)
            {
                return lhs->AssetPath < rhs->AssetPath;
            });

        m_SelectedGraphIndex = -1;
        if (m_Session.HasOpenGraph())
        {
            for (size_t i = 0; i < m_GraphMetas.size(); ++i)
            {
                if (m_GraphMetas[i]->AssetPath == m_Session.AssetPath)
                {
                    m_SelectedGraphIndex = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    void AnimationGraphEditor::OpenSession(const AssetMeta* meta)
    {
        StashActiveSession();

        if (!meta)
        {
            m_Session.Clear();
            m_SelectedGraphIndex = -1;
            InvalidateGraphCanvas();
            return;
        }

        if (ActivateStoredSession(meta->AssetPath))
        {
            for (size_t i = 0; i < m_GraphMetas.size(); ++i)
            {
                if (m_GraphMetas[i] == meta ||
                    (m_GraphMetas[i] != nullptr && m_GraphMetas[i]->AssetPath == meta->AssetPath))
                {
                    m_SelectedGraphIndex = static_cast<int>(i);
                    break;
                }
            }
            return;
        }

        std::shared_ptr<AnimationGraph> graph =
            ([&]() -> std::shared_ptr<AnimationGraph> { std::string _loadAssetError; return std::dynamic_pointer_cast<AnimationGraph>(AssetManager::Get().LoadAssetByPath(meta->AssetPath, _loadAssetError)); })();
        if (!graph)
        {
            ME_LOG(LogEditor, Error, "AnimationGraphEditor: failed to load '{}'.", meta->AssetPath);
            return;
        }

        m_Session.GraphAsset = graph;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
        m_Session.Selection.Clear();
        m_SessionsByKey[m_Session.AssetPath] = m_Session;

        for (size_t i = 0; i < m_GraphMetas.size(); ++i)
        {
            if (m_GraphMetas[i] == meta)
            {
                m_SelectedGraphIndex = static_cast<int>(i);
                break;
            }
        }

        InvalidateGraphCanvas();
    }

    void AnimationGraphEditor::EnsureDefaultSession()
    {
        if (m_Session.HasOpenGraph())
        {
            return;
        }
        if (m_GraphMetas.empty())
        {
            return;
        }
        OpenSession(m_GraphMetas.front());
    }

    bool AnimationGraphEditor::SaveActiveGraph()
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        std::string validateError;
        if (!m_Session.GraphAsset->Validate(&validateError))
        {
            ME_LOG(LogEditor, Warn, 
                "AnimationGraphEditor: saving '{}' with validation warning: {}",
                m_Session.AssetPath,
                validateError);
        }

        const bool saved = AssetManager::Get().SaveAsset<AnimationGraph>(
            m_Session.AssetPath,
            *m_Session.GraphAsset);
        if (saved)
        {
            m_Session.Dirty = false;
            if (!m_Session.AssetPath.empty())
            {
                m_SessionsByKey[m_Session.AssetPath] = m_Session;
            }
        }
        else
        {
            ME_LOG(LogEditor, Error, "AnimationGraphEditor: Save failed for '{}'.", m_Session.AssetPath);
        }
        return saved;
    }

    bool AnimationGraphEditor::ValidateActiveGraph(std::string* outError) const
    {
        if (!m_Session.HasOpenGraph())
        {
            if (outError)
            {
                *outError = "No AnimationGraph is open.";
            }
            return false;
        }
        return m_Session.GraphAsset->Validate(outError);
    }

    std::string AnimationGraphEditor::MakeUniqueStateName(std::string_view baseName) const
    {
        std::string base = baseName.empty() ? "State" : std::string(baseName);
        if (!m_Session.HasOpenGraph())
        {
            return base;
        }

        const AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        if (m_Session.GraphAsset->FindState(base) == nullptr)
        {
            return base;
        }

        for (int suffix = 1; suffix < 10000; ++suffix)
        {
            const std::string candidate = base + "_" + std::to_string(suffix);
            if (m_Session.GraphAsset->FindState(candidate) == nullptr)
            {
                return candidate;
            }
        }
        return base + "_X";
    }

    bool AnimationGraphEditor::AddStateAt(float editorPosX, float editorPosY, std::string* outName)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        AnimState state;
        state.Name = MakeUniqueStateName("State");
        state.EditorPosX = editorPosX;
        state.EditorPosY = editorPosY;
        state.bLoop = true;
        state.Speed = 1.0f;

        if (sm.DefaultStateName.empty())
        {
            sm.DefaultStateName = state.Name;
        }

        if (outName)
        {
            *outName = state.Name;
        }
        sm.States.push_back(std::move(state));
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::RemoveStateByName(std::string_view stateName)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        auto stateIt = std::find_if(
            sm.States.begin(),
            sm.States.end(),
            [&](const AnimState& state) { return state.Name == stateName; });
        if (stateIt == sm.States.end())
        {
            return false;
        }

        sm.States.erase(stateIt);

        auto eraseMatching = [&](std::vector<AnimTransition>& transitions)
        {
            transitions.erase(
                std::remove_if(
                    transitions.begin(),
                    transitions.end(),
                    [&](const AnimTransition& transition)
                    {
                        return transition.FromStateName == stateName
                            || transition.ToStateName == stateName;
                    }),
                transitions.end());
        };
        eraseMatching(sm.Transitions);
        eraseMatching(sm.AnyStateTransitions);

        if (sm.DefaultStateName == stateName)
        {
            sm.DefaultStateName = sm.States.empty() ? std::string() : sm.States.front().Name;
        }

        if (m_Session.Selection.Kind == AnimGraphSelectionKind::State
            && m_Session.Selection.StateName == stateName)
        {
            ClearSelection();
        }

        NotifyGraphChanged();
        InvalidateGraphCanvas();
        return true;
    }

    bool AnimationGraphEditor::RenameState(
        std::string_view oldName,
        std::string_view newName,
        std::string* outError)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (newName.empty())
        {
            if (outError)
            {
                *outError = "State name must be non-empty.";
            }
            return false;
        }
        if (oldName == newName)
        {
            return true;
        }
        if (m_Session.GraphAsset->FindState(newName) != nullptr)
        {
            if (outError)
            {
                *outError = "State name already exists.";
            }
            return false;
        }

        AnimState* state = m_Session.GraphAsset->FindStateMutable(oldName);
        if (!state)
        {
            if (outError)
            {
                *outError = "State not found.";
            }
            return false;
        }

        state->Name = std::string(newName);
        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        for (AnimTransition& transition : sm.Transitions)
        {
            if (transition.FromStateName == oldName)
            {
                transition.FromStateName = std::string(newName);
            }
            if (transition.ToStateName == oldName)
            {
                transition.ToStateName = std::string(newName);
            }
        }
        for (AnimTransition& transition : sm.AnyStateTransitions)
        {
            if (transition.ToStateName == oldName)
            {
                transition.ToStateName = std::string(newName);
            }
        }
        if (sm.DefaultStateName == oldName)
        {
            sm.DefaultStateName = std::string(newName);
        }
        if (m_Session.Selection.Kind == AnimGraphSelectionKind::State
            && m_Session.Selection.StateName == oldName)
        {
            m_Session.Selection.StateName = std::string(newName);
        }

        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::AddTransition(
        std::string_view fromState,
        std::string_view toState,
        std::string* outError)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (fromState.empty() || toState.empty())
        {
            if (outError)
            {
                *outError = "Transition requires From and To states.";
            }
            return false;
        }
        if (fromState == toState)
        {
            if (outError)
            {
                *outError = "Self-transitions are not allowed in MVP.";
            }
            return false;
        }
        if (m_Session.GraphAsset->FindState(fromState) == nullptr
            || m_Session.GraphAsset->FindState(toState) == nullptr)
        {
            if (outError)
            {
                *outError = "Transition states must exist.";
            }
            return false;
        }

        AnimTransition transition;
        transition.FromStateName = std::string(fromState);
        transition.ToStateName = std::string(toState);
        transition.BlendDurationSeconds = 0.15f;
        m_Session.GraphAsset->GetStateMachine().Transitions.push_back(std::move(transition));
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::AddAnyStateTransition(std::string_view toState, std::string* outError)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (toState.empty())
        {
            if (outError)
            {
                *outError = "AnyState transition requires a To state.";
            }
            return false;
        }
        if (m_Session.GraphAsset->FindState(toState) == nullptr)
        {
            if (outError)
            {
                *outError = "AnyState transition To state must exist.";
            }
            return false;
        }

        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        for (const AnimTransition& existing : sm.AnyStateTransitions)
        {
            if (existing.ToStateName == toState)
            {
                if (outError)
                {
                    *outError = "AnyState transition to this state already exists.";
                }
                return false;
            }
        }

        AnimTransition transition;
        transition.FromStateName.clear();
        transition.ToStateName = std::string(toState);
        transition.BlendDurationSeconds = 0.15f;
        sm.AnyStateTransitions.push_back(std::move(transition));
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::RemoveTransitionAt(size_t transitionIndex)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        if (transitionIndex >= sm.Transitions.size())
        {
            return false;
        }
        sm.Transitions.erase(sm.Transitions.begin() + static_cast<std::ptrdiff_t>(transitionIndex));
        if (m_Session.Selection.Kind == AnimGraphSelectionKind::Transition)
        {
            if (m_Session.Selection.TransitionIndex == static_cast<int>(transitionIndex))
            {
                ClearSelection();
            }
            else if (m_Session.Selection.TransitionIndex > static_cast<int>(transitionIndex))
            {
                --m_Session.Selection.TransitionIndex;
            }
        }
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::RemoveAnyStateTransitionAt(size_t anyTransitionIndex)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        if (anyTransitionIndex >= sm.AnyStateTransitions.size())
        {
            return false;
        }
        sm.AnyStateTransitions.erase(
            sm.AnyStateTransitions.begin() + static_cast<std::ptrdiff_t>(anyTransitionIndex));
        if (m_Session.Selection.Kind == AnimGraphSelectionKind::AnyStateTransition)
        {
            if (m_Session.Selection.TransitionIndex == static_cast<int>(anyTransitionIndex))
            {
                ClearSelection();
            }
            else if (m_Session.Selection.TransitionIndex > static_cast<int>(anyTransitionIndex))
            {
                --m_Session.Selection.TransitionIndex;
            }
        }
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::ReverseTransition()
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        // AnyState transitions keep From empty; reverse is only for normal Transitions.
        if (m_Session.Selection.Kind != AnimGraphSelectionKind::Transition)
        {
            return false;
        }

        AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        const int transitionIndex = m_Session.Selection.TransitionIndex;
        if (transitionIndex < 0
            || static_cast<size_t>(transitionIndex) >= stateMachine.Transitions.size())
        {
            return false;
        }

        AnimTransition& transition = stateMachine.Transitions[static_cast<size_t>(transitionIndex)];
        if (transition.FromStateName.empty()
            || transition.ToStateName.empty()
            || transition.FromStateName == transition.ToStateName)
        {
            return false;
        }

        std::swap(transition.FromStateName, transition.ToStateName);
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::SetDefaultStateName(std::string_view stateName, std::string* outError)
    {
        if (stateName.empty())
        {
            if (outError)
            {
                *outError = "DefaultStateName cannot be empty.";
            }
            return false;
        }
        return ApplyDefaultStateName(stateName, outError);
    }

    bool AnimationGraphEditor::ApplyDefaultStateName(std::string_view stateName, std::string* outError)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (!stateName.empty() && m_Session.GraphAsset->FindState(stateName) == nullptr)
        {
            if (outError)
            {
                *outError = "DefaultStateName must match an existing state.";
            }
            return false;
        }

        AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        if (stateMachine.DefaultStateName == stateName)
        {
            return true;
        }

        stateMachine.DefaultStateName = std::string(stateName);
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::SerializeOwnedProperty(const std::string& propertyPath, std::vector<uint8_t>& outBlob) const
    {
        if (!m_Session.HasOpenGraph() || propertyPath.empty())
        {
            return false;
        }

        AnimationGraph& graph = *m_Session.GraphAsset;
        const Reflection::MEClass* graphClass = graph.GetClass();
        if (graphClass == nullptr)
        {
            return false;
        }

        return EditorObjectPropertyApply::SerializeBlob(
            graph.GetGuid(),
            graphClass->GetName(),
            propertyPath,
            outBlob,
            EditorObjectPropertyApply::MakeDefaultOptions());
    }

    bool AnimationGraphEditor::ApplySetObjectProperty(const GUID& ownerGuid,
                                                      const std::string& ownerClassName,
                                                      const std::string& propertyPath,
                                                      const std::vector<uint8_t>& valueBlob)
    {
        if (!EditorObjectPropertyApply::ApplyBlob(
                ownerGuid,
                ownerClassName,
                propertyPath,
                valueBlob,
                EditorObjectPropertyApply::MakeDefaultOptions()))
        {
            return false;
        }

        NotifyGraphChanged();
        InvalidateGraphCanvas(true);
        return true;
    }

    void AnimationGraphEditor::SubmitSetObjectProperty(IEditorContext& context,
                                                       const GUID& ownerGuid,
                                                       const std::string& ownerClassName,
                                                       const std::string& propertyPath,
                                                       std::vector<uint8_t> beforeValue,
                                                       std::vector<uint8_t> afterValue,
                                                       bool applyOnFirstExecute,
                                                       EditorSetObjectPropertySideEffects sideEffects)
    {
        if (beforeValue == afterValue)
        {
            return;
        }

        EditorDocumentSession* session = GetAnimationGraphDocumentSession(context);
        if (session == nullptr)
        {
            return;
        }

        context.GetAnimationGraphEditorServices().CommitPropertyBlobs(
            context,
            *session,
            ownerGuid,
            ownerClassName,
            propertyPath,
            std::move(beforeValue),
            std::move(afterValue),
            applyOnFirstExecute,
            std::move(sideEffects));
    }

    bool AnimationGraphEditor::SubmitOwnedPropertyMutation(const std::string& propertyPath,
                                                           const std::function<bool()>& mutate)
    {
        if (!mutate || !m_Session.HasOpenGraph())
        {
            return false;
        }

        if (m_Context == nullptr)
        {
            return mutate();
        }

        AnimationGraph& graph = *m_Session.GraphAsset;
        const Reflection::MEClass* graphClass = graph.GetClass();
        if (graphClass == nullptr)
        {
            return mutate();
        }

        std::vector<uint8_t> beforeValue;
        if (!SerializeOwnedProperty(propertyPath, beforeValue))
        {
            return mutate();
        }

        if (!mutate())
        {
            return false;
        }

        std::vector<uint8_t> afterValue;
        if (!SerializeOwnedProperty(propertyPath, afterValue))
        {
            return true;
        }

        SubmitSetObjectProperty(
            *m_Context,
            graph.GetGuid(),
            graphClass->GetName(),
            propertyPath,
            std::move(beforeValue),
            std::move(afterValue),
            false);
        return true;
    }

    void AnimationGraphEditor::StoreOwnedPropertyUndoBefore(const std::string& propertyPath)
    {
        if (ImGui::GetItemID() == 0)
        {
            return;
        }

        PendingOwnedPropertyUndo pending;
        pending.PropertyPath = propertyPath;
        if (!SerializeOwnedProperty(propertyPath, pending.BeforeValue))
        {
            return;
        }

        m_PropertyUndoBeforeByEditId[static_cast<uint32_t>(ImGui::GetItemID())] = std::move(pending);
    }

    void AnimationGraphEditor::TryCaptureOwnedPropertyUndoActivated(const std::string& propertyPath)
    {
        if (!ImGui::IsItemActivated())
        {
            return;
        }

        StoreOwnedPropertyUndoBefore(propertyPath);
    }

    void AnimationGraphEditor::TryCommitOwnedPropertyUndoAfterEdit(const std::string& propertyPath)
    {
        if (!ImGui::IsItemDeactivatedAfterEdit() || ImGui::GetItemID() == 0 || m_Context == nullptr
            || !m_Session.HasOpenGraph())
        {
            return;
        }

        const uint32_t editId = static_cast<uint32_t>(ImGui::GetItemID());
        const auto pendingIter = m_PropertyUndoBeforeByEditId.find(editId);
        if (pendingIter == m_PropertyUndoBeforeByEditId.end()
            || pendingIter->second.PropertyPath != propertyPath)
        {
            return;
        }

        PendingOwnedPropertyUndo pending = std::move(pendingIter->second);
        m_PropertyUndoBeforeByEditId.erase(pendingIter);

        AnimationGraph& graph = *m_Session.GraphAsset;
        const Reflection::MEClass* graphClass = graph.GetClass();
        if (graphClass == nullptr)
        {
            return;
        }

        std::vector<uint8_t> afterValue;
        if (!SerializeOwnedProperty(propertyPath, afterValue))
        {
            return;
        }

        SubmitSetObjectProperty(
            *m_Context,
            graph.GetGuid(),
            graphClass->GetName(),
            propertyPath,
            std::move(pending.BeforeValue),
            std::move(afterValue),
            false);
    }

    void AnimationGraphEditor::CapturePositionDragBefore()
    {
        m_HasPositionDragBefore = SerializeOwnedProperty("m_StateMachine", m_PositionDragBeforeBlob);
    }

    void AnimationGraphEditor::CommitPositionDragIfNeeded()
    {
        if (!m_HasPositionDragBefore || m_Context == nullptr || !m_Session.HasOpenGraph()
            || !ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            return;
        }

        AnimationGraph& graph = *m_Session.GraphAsset;
        const Reflection::MEClass* graphClass = graph.GetClass();
        std::vector<uint8_t> afterValue;
        if (graphClass != nullptr && SerializeOwnedProperty("m_StateMachine", afterValue))
        {
            SubmitSetObjectProperty(
                *m_Context,
                graph.GetGuid(),
                graphClass->GetName(),
                "m_StateMachine",
                std::move(m_PositionDragBeforeBlob),
                std::move(afterValue),
                false);
        }

        m_HasPositionDragBefore = false;
        m_PositionDragBeforeBlob.clear();
    }

    bool AnimationGraphEditor::ApplyInsertState(
        const AnimState& state,
        size_t index,
        const std::string& defaultStateName)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (m_Session.GraphAsset->FindState(state.Name) != nullptr)
        {
            return false;
        }

        AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        if (index > stateMachine.States.size())
        {
            index = stateMachine.States.size();
        }
        stateMachine.States.insert(stateMachine.States.begin() + static_cast<std::ptrdiff_t>(index), state);
        stateMachine.DefaultStateName = defaultStateName;
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::ApplyInsertTransition(size_t index, const AnimTransition& transition)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        if (index > stateMachine.Transitions.size())
        {
            index = stateMachine.Transitions.size();
        }
        stateMachine.Transitions.insert(
            stateMachine.Transitions.begin() + static_cast<std::ptrdiff_t>(index),
            transition);
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::ApplyInsertAnyStateTransition(size_t index, const AnimTransition& transition)
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        if (index > stateMachine.AnyStateTransitions.size())
        {
            index = stateMachine.AnyStateTransitions.size();
        }
        stateMachine.AnyStateTransitions.insert(
            stateMachine.AnyStateTransitions.begin() + static_cast<std::ptrdiff_t>(index),
            transition);
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }

    bool AnimationGraphEditor::CaptureStateRemoval(
        std::string_view stateName,
        AnimStateRemovalSnapshot& outSnapshot) const
    {
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }

        const AnimState* state = m_Session.GraphAsset->FindState(stateName);
        if (state == nullptr)
        {
            return false;
        }

        const AnimStateMachine& stateMachine = m_Session.GraphAsset->GetStateMachine();
        outSnapshot.State = *state;
        outSnapshot.DefaultStateName = stateMachine.DefaultStateName;
        outSnapshot.StateIndex = 0;
        for (size_t stateIndex = 0; stateIndex < stateMachine.States.size(); ++stateIndex)
        {
            if (stateMachine.States[stateIndex].Name == stateName)
            {
                outSnapshot.StateIndex = stateIndex;
                break;
            }
        }

        outSnapshot.Transitions.clear();
        for (size_t transitionIndex = 0; transitionIndex < stateMachine.Transitions.size(); ++transitionIndex)
        {
            const AnimTransition& transition = stateMachine.Transitions[transitionIndex];
            if (transition.FromStateName == stateName || transition.ToStateName == stateName)
            {
                outSnapshot.Transitions.push_back(AnimIndexedTransition{transitionIndex, transition});
            }
        }

        outSnapshot.AnyStateTransitions.clear();
        for (size_t transitionIndex = 0; transitionIndex < stateMachine.AnyStateTransitions.size(); ++transitionIndex)
        {
            const AnimTransition& transition = stateMachine.AnyStateTransitions[transitionIndex];
            if (transition.ToStateName == stateName)
            {
                outSnapshot.AnyStateTransitions.push_back(AnimIndexedTransition{transitionIndex, transition});
            }
        }
        return true;
    }

    bool AnimationGraphEditor::ApplyRestoreStateRemoval(const AnimStateRemovalSnapshot& snapshot)
    {
        if (!ApplyInsertState(snapshot.State, snapshot.StateIndex, snapshot.DefaultStateName))
        {
            return false;
        }

        for (const AnimIndexedTransition& item : snapshot.Transitions)
        {
            ApplyInsertTransition(item.Index, item.Transition);
        }
        for (const AnimIndexedTransition& item : snapshot.AnyStateTransitions)
        {
            ApplyInsertAnyStateTransition(item.Index, item.Transition);
        }
        return true;
    }

    AnimationGraph* AnimationGraphEditor::FindOpenGraph(std::string_view assetKey) const
    {
        if (assetKey.empty())
        {
            return nullptr;
        }
        if (m_Session.AssetPath == assetKey && m_Session.GraphAsset)
        {
            return m_Session.GraphAsset.get();
        }

        const auto it = m_SessionsByKey.find(std::string(assetKey));
        if (it == m_SessionsByKey.end() || !it->second.GraphAsset)
        {
            return nullptr;
        }
        return it->second.GraphAsset.get();
    }

    bool AnimationGraphEditor::SubmitAddState(float editorPosX, float editorPosY, std::string* outName)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }
        return m_Context->GetAnimationGraphEditorServices()
            .AddState(*m_Context, *session, editorPosX, editorPosY, outName)
            .Ok;
    }

    bool AnimationGraphEditor::SubmitRemoveState(std::string_view stateName)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }
        return m_Context->GetAnimationGraphEditorServices().RemoveState(*m_Context, *session, stateName).Ok;
    }

    bool AnimationGraphEditor::SubmitRenameState(
        std::string_view oldName,
        std::string_view newName,
        std::string* outError)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }

        const GraphEditorResult result = m_Context->GetAnimationGraphEditorServices().RenameState(
            *m_Context,
            *session,
            oldName,
            newName);
        if (!result.Ok && outError != nullptr)
        {
            *outError = result.Message;
        }
        return result.Ok;
    }

    bool AnimationGraphEditor::SubmitAddTransition(
        std::string_view fromState,
        std::string_view toState,
        size_t* outIndex,
        std::string* outError)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }

        const GraphEditorResult result = m_Context->GetAnimationGraphEditorServices().AddTransition(
            *m_Context,
            *session,
            fromState,
            toState,
            outIndex);
        if (!result.Ok && outError != nullptr)
        {
            *outError = result.Message;
        }
        return result.Ok;
    }

    bool AnimationGraphEditor::SubmitAddAnyStateTransition(
        std::string_view toState,
        size_t* outIndex,
        std::string* outError)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }

        const GraphEditorResult result = m_Context->GetAnimationGraphEditorServices().AddAnyStateTransition(
            *m_Context,
            *session,
            toState,
            outIndex);
        if (!result.Ok && outError != nullptr)
        {
            *outError = result.Message;
        }
        return result.Ok;
    }

    bool AnimationGraphEditor::SubmitRemoveTransition(size_t transitionIndex)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }
        return m_Context->GetAnimationGraphEditorServices()
            .RemoveTransition(*m_Context, *session, transitionIndex)
            .Ok;
    }

    bool AnimationGraphEditor::SubmitRemoveAnyStateTransition(size_t anyTransitionIndex)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }
        return m_Context->GetAnimationGraphEditorServices()
            .RemoveAnyStateTransition(*m_Context, *session, anyTransitionIndex)
            .Ok;
    }

    bool AnimationGraphEditor::SubmitSetDefaultState(std::string_view stateName, std::string* outError)
    {
        if (m_Context == nullptr)
        {
            return false;
        }
        EditorDocumentSession* session = GetAnimationGraphDocumentSession(*m_Context);
        if (session == nullptr)
        {
            return false;
        }

        const GraphEditorResult result =
            m_Context->GetAnimationGraphEditorServices().SetDefaultState(*m_Context, *session, stateName);
        if (!result.Ok && outError != nullptr)
        {
            *outError = result.Message;
        }
        return result.Ok;
    }
}

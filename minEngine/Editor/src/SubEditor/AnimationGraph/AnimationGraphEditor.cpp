#include "AnimationGraphEditor.h"

#include "EditorGUIManager.h"
#include "Shell/EditorDockLayout.h"
#include "Shell/EditorInputHub.h"
#include "Shell/IEditorContext.h"

#include "UI/EditorWindows/AnimGraphWindow.h"
#include "UI/EditorWindows/AnimGraphParametersWindow.h"

#include "Runtime/Core/Log/LogSystem.h"
#include "Runtime/Resource/AssetManager.h"
#include "Runtime/Resource/AssetMeta.h"

#include <algorithm>
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
        EnsureDefaultSession();
    }

    void AnimationGraphEditor::OnExitMode()
    {
        ClearSelection();
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
        if (!meta)
        {
            m_Session.Clear();
            m_SelectedGraphIndex = -1;
            InvalidateGraphCanvas();
            return;
        }

        std::shared_ptr<AnimationGraph> graph =
            AssetManager::Get().LoadAsset<AnimationGraph>(meta->AssetPath);
        if (!graph)
        {
            ME_CORE_ERROR("AnimationGraphEditor: failed to load '{}'.", meta->AssetPath);
            return;
        }

        m_Session.GraphAsset = graph;
        m_Session.AssetPath = meta->AssetPath;
        m_Session.Dirty = false;
        m_Session.Selection.Clear();

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
            ME_CORE_WARN(
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
        }
        else
        {
            ME_CORE_ERROR("AnimationGraphEditor: Save failed for '{}'.", m_Session.AssetPath);
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
        if (!m_Session.HasOpenGraph())
        {
            return false;
        }
        if (stateName.empty())
        {
            if (outError)
            {
                *outError = "DefaultStateName cannot be empty.";
            }
            return false;
        }
        if (m_Session.GraphAsset->FindState(stateName) == nullptr)
        {
            if (outError)
            {
                *outError = "DefaultStateName must match an existing state.";
            }
            return false;
        }

        AnimStateMachine& sm = m_Session.GraphAsset->GetStateMachine();
        if (sm.DefaultStateName == stateName)
        {
            return true;
        }

        sm.DefaultStateName = std::string(stateName);
        NotifyGraphChanged();
        InvalidateGraphCanvas(false);
        return true;
    }
}

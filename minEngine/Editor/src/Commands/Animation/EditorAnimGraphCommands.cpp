#include "Commands/Animation/EditorAnimGraphCommands.h"

#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

namespace minEngine
{
    EditorAddAnimStateCommand::EditorAddAnimStateCommand(
        AnimationGraphEditor& editor,
        float editorPosX,
        float editorPosY)
        : m_Editor(editor)
        , m_EditorPosX(editorPosX)
        , m_EditorPosY(editorPosY)
    {
        m_Description = "Add Animation State";
    }

    void EditorAddAnimStateCommand::Execute()
    {
        if (!m_HasState)
        {
            std::string createdName;
            if (!m_Editor.AddStateAt(m_EditorPosX, m_EditorPosY, &createdName))
            {
                return;
            }

            AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
            if (graph == nullptr)
            {
                return;
            }

            const AnimState* created = graph->FindState(createdName);
            if (created == nullptr)
            {
                return;
            }

            m_State = *created;
            m_StateIndex = graph->GetStateMachine().States.size() - 1;
            m_DefaultStateName = graph->GetStateMachine().DefaultStateName;
            m_HasState = true;
            return;
        }

        m_Editor.ApplyInsertState(m_State, m_StateIndex, m_DefaultStateName);
    }

    void EditorAddAnimStateCommand::Undo()
    {
        if (!m_HasState)
        {
            return;
        }
        m_Editor.RemoveStateByName(m_State.Name);
    }

    const char* EditorAddAnimStateCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorRemoveAnimStateCommand::EditorRemoveAnimStateCommand(AnimationGraphEditor& editor, std::string stateName)
        : m_Editor(editor)
        , m_StateName(std::move(stateName))
    {
        m_Description = "Remove Animation State";
    }

    void EditorRemoveAnimStateCommand::Execute()
    {
        if (!m_Captured)
        {
            if (!m_Editor.CaptureStateRemoval(m_StateName, m_Snapshot))
            {
                return;
            }
            m_Captured = true;
        }

        m_Editor.RemoveStateByName(m_StateName);
    }

    void EditorRemoveAnimStateCommand::Undo()
    {
        if (!m_Captured)
        {
            return;
        }
        m_Editor.ApplyRestoreStateRemoval(m_Snapshot);
    }

    const char* EditorRemoveAnimStateCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorRenameAnimStateCommand::EditorRenameAnimStateCommand(
        AnimationGraphEditor& editor,
        std::string oldName,
        std::string newName)
        : m_Editor(editor)
        , m_OldName(std::move(oldName))
        , m_NewName(std::move(newName))
    {
        m_Description = "Rename Animation State";
    }

    void EditorRenameAnimStateCommand::Execute()
    {
        m_Editor.RenameState(m_OldName, m_NewName, nullptr);
    }

    void EditorRenameAnimStateCommand::Undo()
    {
        m_Editor.RenameState(m_NewName, m_OldName, nullptr);
    }

    const char* EditorRenameAnimStateCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorAddAnimTransitionCommand::EditorAddAnimTransitionCommand(
        AnimationGraphEditor& editor,
        std::string fromState,
        std::string toState)
        : m_Editor(editor)
        , m_FromState(std::move(fromState))
        , m_ToState(std::move(toState))
    {
        m_Description = "Add Animation Transition";
    }

    void EditorAddAnimTransitionCommand::Execute()
    {
        if (!m_HasTransition)
        {
            AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
            if (graph == nullptr)
            {
                return;
            }

            const size_t before = graph->GetStateMachine().Transitions.size();
            if (!m_Editor.AddTransition(m_FromState, m_ToState, nullptr))
            {
                return;
            }
            if (graph->GetStateMachine().Transitions.size() != before + 1)
            {
                return;
            }

            m_Index = before;
            m_Transition = graph->GetStateMachine().Transitions[m_Index];
            m_HasTransition = true;
            return;
        }

        m_Editor.ApplyInsertTransition(m_Index, m_Transition);
    }

    void EditorAddAnimTransitionCommand::Undo()
    {
        if (!m_HasTransition)
        {
            return;
        }
        m_Editor.RemoveTransitionAt(m_Index);
    }

    const char* EditorAddAnimTransitionCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorAddAnyStateTransitionCommand::EditorAddAnyStateTransitionCommand(
        AnimationGraphEditor& editor,
        std::string toState)
        : m_Editor(editor)
        , m_ToState(std::move(toState))
    {
        m_Description = "Add Any-State Transition";
    }

    void EditorAddAnyStateTransitionCommand::Execute()
    {
        if (!m_HasTransition)
        {
            AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
            if (graph == nullptr)
            {
                return;
            }

            const size_t before = graph->GetStateMachine().AnyStateTransitions.size();
            if (!m_Editor.AddAnyStateTransition(m_ToState, nullptr))
            {
                return;
            }
            if (graph->GetStateMachine().AnyStateTransitions.size() != before + 1)
            {
                return;
            }

            m_Index = before;
            m_Transition = graph->GetStateMachine().AnyStateTransitions[m_Index];
            m_HasTransition = true;
            return;
        }

        m_Editor.ApplyInsertAnyStateTransition(m_Index, m_Transition);
    }

    void EditorAddAnyStateTransitionCommand::Undo()
    {
        if (!m_HasTransition)
        {
            return;
        }
        m_Editor.RemoveAnyStateTransitionAt(m_Index);
    }

    const char* EditorAddAnyStateTransitionCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorRemoveAnimTransitionCommand::EditorRemoveAnimTransitionCommand(
        AnimationGraphEditor& editor,
        size_t transitionIndex)
        : m_Editor(editor)
        , m_Index(transitionIndex)
    {
        m_Description = "Remove Animation Transition";
    }

    void EditorRemoveAnimTransitionCommand::Execute()
    {
        if (!m_Captured)
        {
            AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
            if (graph == nullptr || m_Index >= graph->GetStateMachine().Transitions.size())
            {
                return;
            }
            m_Transition = graph->GetStateMachine().Transitions[m_Index];
            m_Captured = true;
        }

        m_Editor.RemoveTransitionAt(m_Index);
    }

    void EditorRemoveAnimTransitionCommand::Undo()
    {
        if (!m_Captured)
        {
            return;
        }
        m_Editor.ApplyInsertTransition(m_Index, m_Transition);
    }

    const char* EditorRemoveAnimTransitionCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorRemoveAnyStateTransitionCommand::EditorRemoveAnyStateTransitionCommand(
        AnimationGraphEditor& editor,
        size_t transitionIndex)
        : m_Editor(editor)
        , m_Index(transitionIndex)
    {
        m_Description = "Remove Any-State Transition";
    }

    void EditorRemoveAnyStateTransitionCommand::Execute()
    {
        if (!m_Captured)
        {
            AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
            if (graph == nullptr || m_Index >= graph->GetStateMachine().AnyStateTransitions.size())
            {
                return;
            }
            m_Transition = graph->GetStateMachine().AnyStateTransitions[m_Index];
            m_Captured = true;
        }

        m_Editor.RemoveAnyStateTransitionAt(m_Index);
    }

    void EditorRemoveAnyStateTransitionCommand::Undo()
    {
        if (!m_Captured)
        {
            return;
        }
        m_Editor.ApplyInsertAnyStateTransition(m_Index, m_Transition);
    }

    const char* EditorRemoveAnyStateTransitionCommand::GetDescription() const
    {
        return m_Description.c_str();
    }

    EditorSetAnimDefaultStateCommand::EditorSetAnimDefaultStateCommand(
        AnimationGraphEditor& editor,
        std::string stateName)
        : m_Editor(editor)
        , m_NewName(std::move(stateName))
    {
        m_Description = "Set Default Animation State";
    }

    void EditorSetAnimDefaultStateCommand::Execute()
    {
        AnimationGraph* graph = m_Editor.GetSession().GraphAsset.get();
        if (graph == nullptr)
        {
            return;
        }

        if (!m_Captured)
        {
            m_PreviousName = graph->GetStateMachine().DefaultStateName;
            m_Captured = true;
        }

        m_Editor.ApplyDefaultStateName(m_NewName, nullptr);
    }

    void EditorSetAnimDefaultStateCommand::Undo()
    {
        if (!m_Captured)
        {
            return;
        }
        m_Editor.ApplyDefaultStateName(m_PreviousName, nullptr);
    }

    const char* EditorSetAnimDefaultStateCommand::GetDescription() const
    {
        return m_Description.c_str();
    }
}

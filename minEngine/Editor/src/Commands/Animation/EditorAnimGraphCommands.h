#pragma once

#include "Commands/Animation/AnimGraphCommandTypes.h"
#include "Shell/EditorCommandStack.h"

#include <cstddef>
#include <string>

namespace minEngine
{
    class AnimationGraphEditor;

    class EditorAddAnimStateCommand final : public EditorCommand
    {
    public:
        EditorAddAnimStateCommand(AnimationGraphEditor& editor, float editorPosX, float editorPosY);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        float m_EditorPosX = 0.0f;
        float m_EditorPosY = 0.0f;
        AnimState m_State;
        size_t m_StateIndex = 0;
        std::string m_DefaultStateName;
        bool m_HasState = false;
        mutable std::string m_Description;
    };

    class EditorRemoveAnimStateCommand final : public EditorCommand
    {
    public:
        explicit EditorRemoveAnimStateCommand(AnimationGraphEditor& editor, std::string stateName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        std::string m_StateName;
        AnimStateRemovalSnapshot m_Snapshot;
        bool m_Captured = false;
        mutable std::string m_Description;
    };

    class EditorRenameAnimStateCommand final : public EditorCommand
    {
    public:
        EditorRenameAnimStateCommand(AnimationGraphEditor& editor, std::string oldName, std::string newName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        std::string m_OldName;
        std::string m_NewName;
        mutable std::string m_Description;
    };

    class EditorAddAnimTransitionCommand final : public EditorCommand
    {
    public:
        EditorAddAnimTransitionCommand(AnimationGraphEditor& editor, std::string fromState, std::string toState);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        std::string m_FromState;
        std::string m_ToState;
        AnimTransition m_Transition;
        size_t m_Index = 0;
        bool m_HasTransition = false;
        mutable std::string m_Description;
    };

    class EditorAddAnyStateTransitionCommand final : public EditorCommand
    {
    public:
        EditorAddAnyStateTransitionCommand(AnimationGraphEditor& editor, std::string toState);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        std::string m_ToState;
        AnimTransition m_Transition;
        size_t m_Index = 0;
        bool m_HasTransition = false;
        mutable std::string m_Description;
    };

    class EditorRemoveAnimTransitionCommand final : public EditorCommand
    {
    public:
        EditorRemoveAnimTransitionCommand(AnimationGraphEditor& editor, size_t transitionIndex);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        size_t m_Index = 0;
        AnimTransition m_Transition;
        bool m_Captured = false;
        mutable std::string m_Description;
    };

    class EditorRemoveAnyStateTransitionCommand final : public EditorCommand
    {
    public:
        EditorRemoveAnyStateTransitionCommand(AnimationGraphEditor& editor, size_t transitionIndex);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        size_t m_Index = 0;
        AnimTransition m_Transition;
        bool m_Captured = false;
        mutable std::string m_Description;
    };

    class EditorSetAnimDefaultStateCommand final : public EditorCommand
    {
    public:
        EditorSetAnimDefaultStateCommand(AnimationGraphEditor& editor, std::string stateName);

        void Execute() override;
        void Undo() override;
        const char* GetDescription() const override;

    private:
        AnimationGraphEditor& m_Editor;
        std::string m_NewName;
        std::string m_PreviousName;
        bool m_Captured = false;
        mutable std::string m_Description;
    };
}

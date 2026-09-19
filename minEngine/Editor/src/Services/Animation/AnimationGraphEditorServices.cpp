#include "Services/Animation/AnimationGraphEditorServices.h"

#include "Commands/Animation/EditorAnimGraphCommands.h"
#include "Shell/Document/EditorDocumentSession.h"
#include "Shell/EditorContextHelpers.h"
#include "SubEditor/AnimationGraph/AnimationGraphEditor.h"

#include "Runtime/Function/Animation/AnimationGraph.h"

#include <utility>

namespace minEngine
{
    class AnimDocumentBinding
    {
    public:
        AnimDocumentBinding() = default;
        AnimDocumentBinding(const AnimDocumentBinding&) = delete;
        AnimDocumentBinding& operator=(const AnimDocumentBinding&) = delete;

        ~AnimDocumentBinding()
        {
            if (m_Editor == nullptr || !m_Switched)
            {
                return;
            }

            m_Editor->StashActiveSession();
            if (!m_RestoreKey.empty())
            {
                m_Editor->ActivateStoredSession(m_RestoreKey);
            }
        }

        bool Bind(AnimationGraphEditor& editor, const std::string& assetKey, GraphEditorResult& outError)
        {
            m_Editor = &editor;
            if (editor.GetSession().HasOpenGraph() && editor.GetSession().AssetPath == assetKey)
            {
                return true;
            }

            const std::string previous =
                editor.GetSession().HasOpenGraph() ? editor.GetSession().AssetPath : std::string();
            if (!editor.ActivateStoredSession(assetKey))
            {
                m_Editor = nullptr;
                outError.Ok = false;
                outError.Code = GraphEditorError::NotFound;
                outError.Message = "Animation graph session is not loaded.";
                return false;
            }

            m_Switched = true;
            m_RestoreKey = previous;
            return true;
        }

    private:
        AnimationGraphEditor* m_Editor = nullptr;
        std::string m_RestoreKey;
        bool m_Switched = false;
    };

    GraphEditorResult AnimationGraphEditorServices::OkResult()
    {
        GraphEditorResult result;
        result.Ok = true;
        return result;
    }

    GraphEditorResult AnimationGraphEditorServices::Fail(GraphEditorError code, std::string message)
    {
        GraphEditorResult result;
        result.Ok = false;
        result.Code = code;
        result.Message = std::move(message);
        return result;
    }

    GraphEditorResult AnimationGraphEditorServices::EnsureAnimSession(const EditorDocumentSession& session)
    {
        if (session.GetTypeId() != "AnimationGraph")
        {
            return Fail(GraphEditorError::InvalidArgument, "Session is not an AnimationGraph document.");
        }
        if (session.GetAssetKey().empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "Animation graph session has no asset key.");
        }
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::GetAnimationGraph(
        IEditorContext& context,
        EditorDocumentSession& session,
        AnimationGraph** outGraph) const
    {
        if (outGraph == nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "outGraph is null.");
        }
        *outGraph = nullptr;

        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        AnimationGraph* graph = editor->FindOpenGraph(session.GetAssetKey());
        if (graph == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Animation graph is not open for this session.");
        }

        *outGraph = graph;
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::AddState(
        IEditorContext& context,
        EditorDocumentSession& session,
        float editorPosX,
        float editorPosY,
        std::string* outStateName)
    {
        if (outStateName != nullptr)
        {
            outStateName->clear();
        }

        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Animation graph is not open.");
        }

        const size_t before = graph->GetStateMachine().States.size();
        session.GetCommandStack().Execute(
            std::make_unique<EditorAddAnimStateCommand>(*editor, editorPosX, editorPosY));
        if (graph->GetStateMachine().States.size() != before + 1)
        {
            return Fail(GraphEditorError::Internal, "Add state failed.");
        }

        if (outStateName != nullptr)
        {
            *outStateName = graph->GetStateMachine().States.back().Name;
        }
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::RemoveState(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view stateName)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (stateName.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "stateName is empty.");
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || graph->FindState(stateName) == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "State was not found.");
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorRemoveAnimStateCommand>(*editor, std::string(stateName)));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::RenameState(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view oldName,
        std::string_view newName)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (oldName.empty() || newName.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "State name must be non-empty.");
        }
        if (oldName == newName)
        {
            return OkResult();
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || graph->FindState(oldName) == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "State was not found.");
        }
        if (graph->FindState(newName) != nullptr)
        {
            return Fail(GraphEditorError::InvalidArgument, "State name already exists.");
        }

        session.GetCommandStack().Execute(std::make_unique<EditorRenameAnimStateCommand>(
            *editor,
            std::string(oldName),
            std::string(newName)));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::AddTransition(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view fromState,
        std::string_view toState,
        size_t* outIndex)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (fromState.empty() || toState.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "Transition requires From and To states.");
        }
        if (fromState == toState)
        {
            return Fail(GraphEditorError::InvalidArgument, "Self-transitions are not allowed.");
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || graph->FindState(fromState) == nullptr || graph->FindState(toState) == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "Transition states must exist.");
        }

        const size_t before = graph->GetStateMachine().Transitions.size();
        session.GetCommandStack().Execute(std::make_unique<EditorAddAnimTransitionCommand>(
            *editor,
            std::string(fromState),
            std::string(toState)));
        if (graph->GetStateMachine().Transitions.size() != before + 1)
        {
            return Fail(GraphEditorError::Internal, "Add transition failed.");
        }

        if (outIndex != nullptr)
        {
            *outIndex = before;
        }
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::AddAnyStateTransition(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view toState,
        size_t* outIndex)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (toState.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "AnyState transition requires a To state.");
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || graph->FindState(toState) == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "AnyState transition To state must exist.");
        }

        const size_t before = graph->GetStateMachine().AnyStateTransitions.size();
        session.GetCommandStack().Execute(
            std::make_unique<EditorAddAnyStateTransitionCommand>(*editor, std::string(toState)));
        if (graph->GetStateMachine().AnyStateTransitions.size() != before + 1)
        {
            return Fail(GraphEditorError::Internal, "Add AnyState transition failed.");
        }

        if (outIndex != nullptr)
        {
            *outIndex = before;
        }
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::RemoveTransition(
        IEditorContext& context,
        EditorDocumentSession& session,
        size_t transitionIndex)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || transitionIndex >= graph->GetStateMachine().Transitions.size())
        {
            return Fail(GraphEditorError::NotFound, "Transition index is out of range.");
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorRemoveAnimTransitionCommand>(*editor, transitionIndex));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::RemoveAnyStateTransition(
        IEditorContext& context,
        EditorDocumentSession& session,
        size_t transitionIndex)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || transitionIndex >= graph->GetStateMachine().AnyStateTransitions.size())
        {
            return Fail(GraphEditorError::NotFound, "AnyState transition index is out of range.");
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorRemoveAnyStateTransitionCommand>(*editor, transitionIndex));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::SetDefaultState(
        IEditorContext& context,
        EditorDocumentSession& session,
        std::string_view stateName)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (stateName.empty())
        {
            return Fail(GraphEditorError::InvalidArgument, "DefaultStateName cannot be empty.");
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        AnimationGraph* graph = editor->GetSession().GraphAsset.get();
        if (graph == nullptr || graph->FindState(stateName) == nullptr)
        {
            return Fail(GraphEditorError::NotFound, "DefaultStateName must match an existing state.");
        }

        session.GetCommandStack().Execute(
            std::make_unique<EditorSetAnimDefaultStateCommand>(*editor, std::string(stateName)));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::CommitPropertyBlobs(
        IEditorContext& context,
        EditorDocumentSession& session,
        const GUID& ownerGuid,
        std::string_view ownerClassName,
        std::string_view propertyPath,
        std::vector<uint8_t> beforeValue,
        std::vector<uint8_t> afterValue,
        bool applyOnFirstExecute,
        EditorSetObjectPropertySideEffects sideEffects)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }
        if (beforeValue == afterValue)
        {
            return OkResult();
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        session.GetCommandStack().Execute(std::make_unique<EditorSetObjectPropertyCommand>(
            *editor,
            ownerGuid,
            std::string(ownerClassName),
            std::string(propertyPath),
            std::move(beforeValue),
            std::move(afterValue),
            applyOnFirstExecute,
            std::move(sideEffects)));
        return OkResult();
    }

    GraphEditorResult AnimationGraphEditorServices::SaveDocument(
        IEditorContext& context,
        EditorDocumentSession& session)
    {
        const GraphEditorResult sessionCheck = EnsureAnimSession(session);
        if (!sessionCheck.Ok)
        {
            return sessionCheck;
        }

        AnimationGraphEditor* editor = GetAnimationGraphEditor(&context);
        if (editor == nullptr)
        {
            return Fail(GraphEditorError::Internal, "AnimationGraphEditor is unavailable.");
        }

        GraphEditorResult bindError = OkResult();
        AnimDocumentBinding binding;
        if (!binding.Bind(*editor, session.GetAssetKey(), bindError))
        {
            return bindError;
        }

        if (!editor->SaveActiveGraph())
        {
            return Fail(GraphEditorError::Internal, "Animation graph save failed.");
        }
        return OkResult();
    }
}

#pragma once

#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "Services/Graph/GraphEditorResult.h"

#include "Runtime/Core/GUID/GUID.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class AnimationGraph;
    class EditorDocumentSession;
    class IEditorContext;

    /**
     * Animation graph primitives.
     * Topology writes go through undo commands. No graph JSON on this type.
     */
    class AnimationGraphEditorServices
    {
    public:
        GraphEditorResult GetAnimationGraph(
            IEditorContext& context,
            EditorDocumentSession& session,
            AnimationGraph** outGraph) const;

        GraphEditorResult AddState(
            IEditorContext& context,
            EditorDocumentSession& session,
            float editorPosX,
            float editorPosY,
            std::string* outStateName);

        GraphEditorResult RemoveState(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view stateName);

        GraphEditorResult RenameState(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view oldName,
            std::string_view newName);

        GraphEditorResult AddTransition(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view fromState,
            std::string_view toState,
            size_t* outIndex);

        GraphEditorResult AddAnyStateTransition(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view toState,
            size_t* outIndex);

        GraphEditorResult RemoveTransition(
            IEditorContext& context,
            EditorDocumentSession& session,
            size_t transitionIndex);

        GraphEditorResult RemoveAnyStateTransition(
            IEditorContext& context,
            EditorDocumentSession& session,
            size_t transitionIndex);

        GraphEditorResult SetDefaultState(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view stateName);

        GraphEditorResult CommitPropertyBlobs(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& ownerGuid,
            std::string_view ownerClassName,
            std::string_view propertyPath,
            std::vector<uint8_t> beforeValue,
            std::vector<uint8_t> afterValue,
            bool applyOnFirstExecute = true,
            EditorSetObjectPropertySideEffects sideEffects = {});

        GraphEditorResult SaveDocument(IEditorContext& context, EditorDocumentSession& session);

    private:
        static GraphEditorResult OkResult();
        static GraphEditorResult Fail(GraphEditorError code, std::string message);
        static GraphEditorResult EnsureAnimSession(const EditorDocumentSession& session);
    };
}

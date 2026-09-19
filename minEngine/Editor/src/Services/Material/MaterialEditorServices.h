#pragma once

#include "Commands/Scene/EditorSetObjectPropertyCommand.h"
#include "Services/Graph/GraphEditorResult.h"

#include "Runtime/Core/GUID/GUID.h"

#include <string>
#include <string_view>
#include <vector>

namespace minEngine
{
    class EditorDocumentSession;
    class IEditorContext;
    class Material;

    /**
     * Material graph primitives (ED-F19).
     * No graph JSON. Commands go on the given document session stack, not the implicit Active stack.
     */
    class MaterialEditorServices
    {
    public:
        GraphEditorResult GetMaterial(
            IEditorContext& context,
            EditorDocumentSession& session,
            Material** outMaterial) const;

        GraphEditorResult AddNode(
            IEditorContext& context,
            EditorDocumentSession& session,
            std::string_view nodeDefClassName,
            float editorPosX,
            float editorPosY,
            GUID* outNodeDefGuid);

        GraphEditorResult RemoveNode(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& nodeDefGuid);

        GraphEditorResult ConnectPins(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& fromNodeDefGuid,
            int32_t fromOutputIndex,
            const GUID& toNodeDefGuid,
            int32_t toInputIndex);

        GraphEditorResult DisconnectInput(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& toNodeDefGuid,
            int32_t toInputIndex);

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

        GraphEditorResult Compile(IEditorContext& context, EditorDocumentSession& session);

    private:
        static GraphEditorResult OkResult();
        static GraphEditorResult Fail(GraphEditorError code, std::string message);
        static GraphEditorResult EnsureMaterialSession(const EditorDocumentSession& session);
    };
}

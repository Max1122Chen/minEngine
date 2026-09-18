#pragma once

#include "Runtime/Core/GUID/GUID.h"

#include <string>
#include <string_view>

namespace minEngine
{
    class EditorDocumentSession;
    class IEditorContext;

    struct EditorControlResult
    {
        bool Ok = false;
        std::string Message;
        std::string SessionId;
    };

    /**
     * Changes what the human is looking at. Does not push a Scene undo command.
     */
    class EditorControlServices
    {
    public:
        EditorControlResult ActivateSession(IEditorContext& context, std::string_view sessionId);

        EditorControlResult OpenAsset(IEditorContext& context, std::string_view assetPath);

        EditorControlResult RevealAsset(IEditorContext& context, std::string_view assetPath);

        EditorControlResult SelectObject(
            IEditorContext& context,
            EditorDocumentSession& session,
            const GUID& objectId);
    };
}

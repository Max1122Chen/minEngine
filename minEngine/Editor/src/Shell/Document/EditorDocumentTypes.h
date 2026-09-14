#pragma once

#include "Shell/Document/EditorDocumentHost.h"
#include "Shell/IEditorContext.h"

namespace minEngine
{
    void RegisterBuiltinEditorDocumentTypes(IEditorContext& context, EditorDocumentHost& host);
}

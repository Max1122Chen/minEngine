#pragma once

#include "Core.h"

namespace minEngine
{
    class IEditorContext;
    class InspectorAssetInspection;

    class InspectorPreviewPresenter
    {
    public:
        static constexpr float kMaxSquareSize = 224.0f;

        static void DrawSquarePreviewSlot(IEditorContext& context, InspectorAssetInspection& inspection);
    };
}

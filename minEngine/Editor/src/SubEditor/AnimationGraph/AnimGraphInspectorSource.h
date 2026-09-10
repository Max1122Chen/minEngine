#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

namespace minEngine
{
    class AnimationGraphEditor;

    class AnimGraphInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit AnimGraphInspectorSource(AnimationGraphEditor& animGraphEditor);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        void DrawNoSelection();
        void DrawStateDetails();
        void DrawTransitionDetails(bool anyState);

        AnimationGraphEditor& m_AnimGraphEditor;
        static constexpr const char* kWindowTitle = "Inspector";
    };
}

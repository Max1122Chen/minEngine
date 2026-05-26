#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

namespace minEngine
{
    class MaterialEditor;

    class MaterialEditorInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit MaterialEditorInspectorSource(MaterialEditor& materialEditor);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        MaterialEditor& m_MaterialEditor;
        static constexpr const char* kWindowTitle = "Inspector";
    };
}

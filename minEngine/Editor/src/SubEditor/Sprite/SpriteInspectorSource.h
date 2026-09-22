#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

namespace minEngine
{
    class SpriteEditor;

    class SpriteInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit SpriteInspectorSource(SpriteEditor& owner);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        SpriteEditor& m_Owner;
    };
}

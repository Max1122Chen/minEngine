#pragma once

#include "Core.h"
#include "Shell/IEditorInspectorSource.h"

namespace minEngine
{
    class SpriteAtlasEditor;

    class SpriteAtlasInspectorSource : public IEditorInspectorSource
    {
    public:
        explicit SpriteAtlasInspectorSource(SpriteAtlasEditor& owner);

        bool HasInspectableSelection() const override;
        void DrawInspector() override;

    private:
        SpriteAtlasEditor& m_Owner;
    };
}

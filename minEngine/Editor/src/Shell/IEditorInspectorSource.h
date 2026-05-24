#pragma once

#include "Core.h"

namespace minEngine
{
    class IEditorInspectorSource
    {
    public:
        virtual ~IEditorInspectorSource() = default;

        virtual bool HasInspectableSelection() const = 0;
        virtual void DrawInspector() = 0;
    };
}

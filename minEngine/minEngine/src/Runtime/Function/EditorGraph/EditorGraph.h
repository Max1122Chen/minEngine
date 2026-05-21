#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"

namespace minEngine
{
    class EditorGraphNode;

    ME_CLASS()
    class EditorGraph : public MEObject
    {
        ME_GENERATED_BODY(EditorGraph)
    public:
        virtual ~EditorGraph() = default;
    };
}

#include "Generated/Reflection/EditorGraph.gen.h"

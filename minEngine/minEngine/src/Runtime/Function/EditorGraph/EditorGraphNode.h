#pragma once
#include "Core.h"
#include "Runtime/Core/Object/MEObject.h"
#include "EditorGraphNodePin.h"

namespace minEngine
{
    class EditorGraph;

    ME_CLASS()
    class EditorGraphNode : public MEObject
    {
        ME_GENERATED_BODY(EditorGraphNode)
    public:
        virtual ~EditorGraphNode() = default;

        std::string Name;
        std::vector<EditorGraphNodePin> Pins;

        std::vector<EditorGraphNodePin>& GetPins() { return Pins; }
        EditorGraphNodePin* GetInputPin(int32_t index);
        EditorGraphNodePin* GetOutputPin(int32_t index);
    };
}

#include "Generated/Reflection/EditorGraphNode.gen.h"

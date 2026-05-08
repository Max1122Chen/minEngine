#pragma once
#include "Core.h"
#include "EditorGraphNodePin.h"
namespace minEngine
{
    class EditorGraph;

    class EditorGraphNode
    {
    public:
        std::string Name;
        std::vector<EditorGraphNodePin> Pins;

    public:
        std::vector<EditorGraphNodePin>& GetPins() { return Pins; }
        EditorGraphNodePin* GetInputPin(int32_t Index);
        EditorGraphNodePin* GetOutputPin(int32_t Index);
    };

}
#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraphNode.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialTypes.h"

namespace minEngine
{
    class MaterialEdGraphNode : public EditorGraphNode
    {
    public:
        MaterialGraphNodeDef* m_Definition = nullptr;

        void CreateInputPins();
        void CreateOutputPins();
    };
}
#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraphNode.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialTypes.h"

#include <memory>

namespace minEngine
{
    class MaterialEdGraphNode : public EditorGraphNode
    {
    public:
        void SetDefinition(std::unique_ptr<MaterialGraphNodeDef> definition);
        MaterialGraphNodeDef* GetDefinition() const { return m_Definition.get(); }

        void CreateInputPins();
        void CreateOutputPins();

    private:
        std::unique_ptr<MaterialGraphNodeDef> m_Definition;
    };
}

#pragma once
#include "Core.h"
#include "Function/EditorGraph/EditorGraphNode.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

#include <memory>

namespace minEngine
{
    ME_CLASS()
    class MaterialEdGraphNode : public EditorGraphNode
    {
        ME_GENERATED_BODY(MaterialEdGraphNode)

    public:
        MaterialEdGraphNode() = default;

        void SetNodeDef(std::shared_ptr<MaterialGraphNodeDef> nodeDef);
        MaterialGraphNodeDef* GetNodeDef() const { return m_NodeDef.get(); }
        MaterialGraphNodeDef* GetDefinition() const { return GetNodeDef(); }

        void RebuildPins();

        ME_PROPERTY()
        float m_EditorPosX = 0.0f;

        ME_PROPERTY()
        float m_EditorPosY = 0.0f;

        ME_PROPERTY()
        std::string m_Comment;

        ME_PROPERTY()
        std::string m_Title;

        ME_PROPERTY(Instanced)
        std::shared_ptr<MaterialGraphNodeDef> m_NodeDef;
    };
}

#include "Generated/Reflection/MaterialEdGraphNode.gen.h"

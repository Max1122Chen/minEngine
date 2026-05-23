#include "MaterialEdGraph.h"

#include "../Material.h"
#include "Runtime/Core/Object/ObjectManager.h"
#include "Runtime/Core/Reflection/Reflection.h"
#include "MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "MaterialCapability.h"
#include "MaterialPropertyUtil.h"
#include "MaterialValueType.h"

#include <algorithm>

namespace minEngine
{
    MaterialEdGraphNode* MaterialEdGraph::FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef)
    {
        if (nodeDef == nullptr)
        {
            return nullptr;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& node : m_Nodes)
        {
            if (node && node->GetNodeDef() == nodeDef)
            {
                return node.get();
            }
        }

        return nullptr;
    }

    const MaterialEdGraphNode* MaterialEdGraph::FindEdNodeByNodeDef(const MaterialGraphNodeDef* nodeDef) const
    {
        return const_cast<MaterialEdGraph*>(this)->FindEdNodeByNodeDef(nodeDef);
    }

    bool MaterialEdGraph::IsNodeOutputConnected(const MaterialGraphNodeDef& nodeDef, int32_t outputIndex) const
    {
        if (outputIndex < 0)
        {
            return false;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& node : m_Nodes)
        {
            if (node == nullptr)
            {
                continue;
            }

            MaterialGraphNodeDef* consumerDef = node->GetNodeDef();
            if (consumerDef == nullptr)
            {
                continue;
            }

            for (int32_t inputIndex = 0; inputIndex < consumerDef->GetInputCount(); ++inputIndex)
            {
                const MaterialGraphNodeDefInput* input = consumerDef->GetInput(inputIndex);
                if (input != nullptr && input->IsConnected() && input->NodeDef == &nodeDef
                    && input->OutputIndex == outputIndex)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool MaterialEdGraph::CanConnectPins(
        const MaterialEdGraphNode& fromNode,
        int32_t fromOutputIndex,
        const MaterialEdGraphNode& toNode,
        int32_t toInputIndex,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode,
        std::string* outReason) const
    {
        const MaterialGraphNodeDef* fromDef = fromNode.GetDefinition();
        const MaterialGraphNodeDef* toDef = toNode.GetDefinition();
        if (fromDef == nullptr || toDef == nullptr)
        {
            if (outReason != nullptr)
            {
                *outReason = "Missing node definition for pin connection.";
            }
            return false;
        }

        if (fromOutputIndex < 0 || fromOutputIndex >= fromDef->GetOutputCount() || toInputIndex < 0 ||
            toInputIndex >= toDef->GetInputCount())
        {
            if (outReason != nullptr)
            {
                *outReason = "Invalid pin index for connection.";
            }
            return false;
        }

        MaterialValueType fromType =
            MaterialValueTypeUtil::GetNodeOutputPinType(fromDef, fromOutputIndex);
        MaterialValueType toType = MaterialValueTypeUtil::GetNodeInputPinType(toDef, toInputIndex);
        if (!MaterialValueTypeUtil::AreConnectable(fromType, toType))
        {
            if (outReason != nullptr)
            {
                *outReason = std::string("Incompatible pin types (") +
                    MaterialValueTypeUtil::GetDisplayName(fromType) + " -> " +
                    MaterialValueTypeUtil::GetDisplayName(toType) + ").";
            }
            return false;
        }

        if (toDef->IsMaterialOutputNode())
        {
            const MaterialGraphNodeDefInput* targetInput = toDef->GetInput(toInputIndex);
            if (targetInput != nullptr)
            {
                const MaterialPropertyPinVisibility visibility =
                    MaterialCapabilityUtil::GetMaterialOutputInputVisibility(
                        targetInput->Name.c_str(),
                        shadingModel,
                        blendMode);

                if (visibility != MaterialPropertyPinVisibility::Active)
                {
                    if (outReason != nullptr)
                    {
                        *outReason = std::string("Material output pin '") + targetInput->Name +
                            "' is not connectable for the current shading or blend mode.";
                    }
                    return false;
                }
            }
        }

        return true;
    }

    bool MaterialEdGraph::ConnectPins(
        MaterialEdGraphNode& fromNode,
        int32_t fromOutputIndex,
        MaterialEdGraphNode& toNode,
        int32_t toInputIndex,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        if (!CanConnectPins(
                fromNode,
                fromOutputIndex,
                toNode,
                toInputIndex,
                shadingModel,
                blendMode,
                nullptr))
        {
            return false;
        }

        EditorGraphNodePin* fromPin = fromNode.GetOutputPin(fromOutputIndex);
        EditorGraphNodePin* toPin = toNode.GetInputPin(toInputIndex);
        MaterialGraphNodeDef* fromDef = fromNode.GetDefinition();
        MaterialGraphNodeDef* toDef = toNode.GetDefinition();
        if (fromPin == nullptr || toPin == nullptr || fromDef == nullptr || toDef == nullptr)
        {
            return false;
        }

        MaterialGraphNodeDefInput* input = toDef->GetInput(toInputIndex);
        if (input == nullptr)
        {
            return false;
        }

        DisconnectInput(toNode, toInputIndex);

        if (!toPin->MakeLinkTo(fromPin))
        {
            return false;
        }

        input->NodeDef = fromDef;
        input->OutputIndex = fromOutputIndex;
        input->ConnectedNodeDefGuid = fromDef->GetGuid();
        return true;
    }

    void MaterialEdGraph::DisconnectInput(MaterialEdGraphNode& toNode, int32_t toInputIndex)
    {
        MaterialGraphNodeDef* toDef = toNode.GetDefinition();
        if (toDef == nullptr)
        {
            return;
        }

        MaterialGraphNodeDefInput* input = toDef->GetInput(toInputIndex);
        if (input != nullptr)
        {
            input->NodeDef = nullptr;
            input->ConnectedNodeDefGuid = GUID::Zero();
            input->OutputIndex = 0;
        }

        EditorGraphNodePin* toPin = toNode.GetInputPin(toInputIndex);
        if (toPin == nullptr)
        {
            return;
        }

        std::vector<EditorGraphNodePin*> linkedPins = toPin->LinkedTo;
        for (EditorGraphNodePin* linkedPin : linkedPins)
        {
            toPin->BreakLinkTo(linkedPin);
        }
    }

    bool MaterialEdGraph::ConnectToMaterialProperty(
        MaterialEdGraphNode& fromNode,
        int32_t fromOutputIndex,
        MaterialEdGraphNode& outputNode,
        MaterialProperty property,
        MaterialShadingModel shadingModel,
        MaterialBlendMode blendMode)
    {
        MaterialPropertyInputDescription description;
        if (!GetMaterialPropertyInputDescription(property, description))
        {
            return false;
        }

        MaterialGraphNodeDef* outputDef = outputNode.GetDefinition();
        if (outputDef == nullptr)
        {
            return false;
        }

        for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = outputDef->GetInput(inputIndex); ++inputIndex)
        {
            if (input->Name == description.InputName)
            {
                return ConnectPins(fromNode, fromOutputIndex, outputNode, inputIndex, shadingModel, blendMode);
            }
        }

        return false;
    }

    MaterialGraphNodeDefInput* MaterialEdGraph::FindPropertyGraphInput(MaterialProperty property) const
    {
        MaterialPropertyInputDescription description;
        if (!GetMaterialPropertyInputDescription(property, description))
        {
            return nullptr;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& node : m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            MaterialGraphNodeDef* definition = node->GetDefinition();
            if (definition == nullptr || !definition->IsMaterialOutputNode())
            {
                continue;
            }

            if (MaterialGraphNodeDefInput* input = definition->FindInputByName(description.InputName))
            {
                return input;
            }
        }

        return nullptr;
    }

    bool MaterialEdGraph::ResolveMaterialPropertyInput(
        MaterialProperty property,
        MaterialPropertyInputDescription& inOutDescription) const
    {
        if (!GetMaterialPropertyInputDescription(property, inOutDescription))
        {
            return false;
        }

        inOutDescription.GraphInput = FindPropertyGraphInput(property);
        return true;
    }

    std::vector<MaterialGraphNodeDef*> MaterialEdGraph::GetMaterialOutputNodeDefs() const
    {
        std::vector<MaterialGraphNodeDef*> outputNodes;
        outputNodes.reserve(m_Nodes.size());

        for (const std::shared_ptr<MaterialEdGraphNode>& node : m_Nodes)
        {
            if (!node)
            {
                continue;
            }

            MaterialGraphNodeDef* definition = node->GetDefinition();
            if (definition != nullptr && definition->IsMaterialOutputNode())
            {
                outputNodes.push_back(definition);
            }
        }

        return outputNodes;
    }

    MaterialEdGraphNode& MaterialEdGraph::AddNode(
        const Reflection::MEClass* nodeDefClass,
        float editorPosX,
        float editorPosY)
    {
        ME_ASSERT(nodeDefClass != nullptr, "AddNode requires a valid NodeDef class.");
        ME_ASSERT(
            nodeDefClass->IsA(MaterialGraphNodeDef::StaticClass()),
            "AddNode class must derive from MaterialGraphNodeDef.");

        std::shared_ptr<MaterialEdGraphNode> edNode = NewObject<MaterialEdGraphNode>("", this);
        std::shared_ptr<MEObject> nodeDefObject = ObjectManager::Get().NewObject(nodeDefClass, "", edNode.get());
        ME_ASSERT(nodeDefObject != nullptr, "Failed to create MaterialGraphNodeDef instance.");

        std::shared_ptr<MaterialGraphNodeDef> nodeDef =
            std::static_pointer_cast<MaterialGraphNodeDef>(nodeDefObject);
        edNode->SetNodeDef(std::move(nodeDef));
        edNode->m_EditorPosX = editorPosX;
        edNode->m_EditorPosY = editorPosY;
        m_Nodes.push_back(std::move(edNode));
        return *m_Nodes.back();
    }

    bool MaterialEdGraph::RemoveNode(MaterialEdGraphNode& node)
    {
        MaterialGraphNodeDef* removedDef = node.GetNodeDef();
        if (removedDef == nullptr)
        {
            return false;
        }

        for (const std::shared_ptr<MaterialEdGraphNode>& otherNodePtr : m_Nodes)
        {
            if (!otherNodePtr || otherNodePtr.get() == &node)
            {
                continue;
            }

            MaterialGraphNodeDef* otherDef = otherNodePtr->GetDefinition();
            if (otherDef == nullptr)
            {
                continue;
            }

            for (int32_t inputIndex = 0; MaterialGraphNodeDefInput* input = otherDef->GetInput(inputIndex);
                 ++inputIndex)
            {
                if (input->NodeDef == removedDef)
                {
                    DisconnectInput(*otherNodePtr, inputIndex);
                }
            }
        }

        const auto removeIter = std::remove_if(
            m_Nodes.begin(),
            m_Nodes.end(),
            [&node](const std::shared_ptr<MaterialEdGraphNode>& candidate)
            {
                return candidate.get() == &node;
            });
        if (removeIter == m_Nodes.end())
        {
            return false;
        }

        m_Nodes.erase(removeIter, m_Nodes.end());
        return true;
    }
}

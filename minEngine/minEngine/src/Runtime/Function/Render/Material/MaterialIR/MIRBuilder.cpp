#include "MIRBuilder.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

namespace minEngine
{
    MIRBuilder::MIRBuilder(MIRGraph& graph)
        : m_Graph(graph)
    {
    }

    MIRValue* MIRBuilder::BuildNodeOutput(MaterialGraphNodeDef& nodeDef, int32_t outputIndex)
    {
        OutputKey key{ &nodeDef, outputIndex };
        auto cachedIter = m_OutputCache.find(key);
        if (cachedIter != m_OutputCache.end())
        {
            return cachedIter->second;
        }

        if (m_ActiveBuilds.find(key) != m_ActiveBuilds.end())
        {
            return nullptr;
        }

        m_ActiveBuilds.insert(key);
        MIRValue* value = nodeDef.BuildIR(*this, outputIndex);
        m_ActiveBuilds.erase(key);

        m_OutputCache.emplace(key, value);
        return value;
    }

    MIRValue* MIRBuilder::BuildInput(const MaterialGraphNodeDefInput& input)
    {
        if (input.ConnectedNodeDef == nullptr || input.ConnectedOutputIndex < 0)
        {
            return nullptr;
        }

        return BuildNodeOutput(*input.ConnectedNodeDef, input.ConnectedOutputIndex);
    }

    MIRValue* MIRBuilder::ConstantFloat(float x)
    {
        return CreateLiteralValue(MaterialLiteralValue::MakeFloat(x));
    }

    MIRValue* MIRBuilder::ConstantFloat2(float x, float y)
    {
        return CreateLiteralValue(MaterialLiteralValue::MakeVector2(x, y));
    }

    MIRValue* MIRBuilder::ConstantFloat3(float x, float y, float z)
    {
        return CreateLiteralValue(MaterialLiteralValue::MakeVector3(x, y, z));
    }

    MIRValue* MIRBuilder::ConstantFloat4(float x, float y, float z, float w)
    {
        return CreateLiteralValue(MaterialLiteralValue::MakeVector4(x, y, z, w));
    }

    MIRValue* MIRBuilder::Add(MIRValue* left, MIRValue* right)
    {
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }

        MaterialValueType resultType = ResolveBinaryResultType(left->ValueType, right->ValueType);
        if (resultType == MaterialValueType::Unknown)
        {
            return nullptr;
        }

        std::vector<MIRValue*> inputs{ left, right };
        return CreateNodeValue(MaterialOp::Add, resultType, inputs);
    }

    MIRValue* MIRBuilder::Multiply(MIRValue* left, MIRValue* right)
    {
        if (left == nullptr || right == nullptr)
        {
            return nullptr;
        }

        MaterialValueType resultType = ResolveBinaryResultType(left->ValueType, right->ValueType);
        if (resultType == MaterialValueType::Unknown)
        {
            return nullptr;
        }

        std::vector<MIRValue*> inputs{ left, right };
        return CreateNodeValue(MaterialOp::Multiply, resultType, inputs);
    }

    MIRValue* MIRBuilder::Texture2DParameter(const std::string& name)
    {
        std::vector<MIRValue*> inputs;
        return CreateNodeValue(MaterialOp::TextureObject, MaterialValueType::Texture2D, inputs, name);
    }

    MIRValue* MIRBuilder::TextureSample(MIRValue* texture, MIRValue* uv)
    {
        if (texture == nullptr || uv == nullptr)
        {
            return nullptr;
        }

        if (texture->ValueType != MaterialValueType::Texture2D)
        {
            return nullptr;
        }

        if (uv->ValueType != MaterialValueType::Vector2)
        {
            return nullptr;
        }

        std::vector<MIRValue*> inputs{ texture, uv };
        return CreateNodeValue(MaterialOp::TextureSample, MaterialValueType::Vector4, inputs);
    }

    void MIRBuilder::SetMaterialOutput(MaterialProperty property, MIRValue* value)
    {
        if (value == nullptr)
        {
            return;
        }

        m_Graph.Outputs[property] = value;
    }

    MIRValue* MIRBuilder::CreateLiteralValue(const MaterialLiteralValue& value)
    {
        std::unique_ptr<MIRValue> newValue = std::make_unique<MIRValue>();
        newValue->Id = m_Graph.NextValueId++;
        newValue->ValueType = value.Type;
        newValue->LiteralValue = value;

        MIRValue* valuePtr = newValue.get();
        m_Graph.Values.push_back(std::move(newValue));
        return valuePtr;
    }

    MIRValue* MIRBuilder::CreateNodeValue(MaterialOp op, MaterialValueType outputType, const std::vector<MIRValue*>& inputs, const std::string& symbolName)
    {
        MIRNode* node = CreateNode(op, inputs, symbolName);

        std::unique_ptr<MIRValue> newValue = std::make_unique<MIRValue>();
        newValue->Id = m_Graph.NextValueId++;
        newValue->ValueType = outputType;
        newValue->Producer = node;

        MIRValue* valuePtr = newValue.get();
        node->Outputs.push_back(valuePtr);
        m_Graph.Values.push_back(std::move(newValue));
        return valuePtr;
    }

    MIRNode* MIRBuilder::CreateNode(MaterialOp op, const std::vector<MIRValue*>& inputs, const std::string& symbolName)
    {
        std::unique_ptr<MIRNode> node = std::make_unique<MIRNode>();
        node->Id = m_Graph.NextNodeId++;
        node->Op = op;
        node->Inputs = inputs;
        node->SymbolName = symbolName;

        MIRNode* nodePtr = node.get();
        m_Graph.Nodes.push_back(std::move(node));
        return nodePtr;
    }

    MaterialValueType MIRBuilder::ResolveBinaryResultType(MaterialValueType leftType, MaterialValueType rightType) const
    {
        if (leftType == rightType)
        {
            return leftType;
        }

        if (leftType == MaterialValueType::Float)
        {
            return rightType;
        }

        if (rightType == MaterialValueType::Float)
        {
            return leftType;
        }

        return MaterialValueType::Unknown;
    }
}
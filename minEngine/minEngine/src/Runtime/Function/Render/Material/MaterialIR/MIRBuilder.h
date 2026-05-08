#pragma once
#include "Core.h"
#include "MIRGraph.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace minEngine
{
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;

    class MIRBuilder
    {
    public:
        explicit MIRBuilder(MIRGraph& graph);

        MIRValue* BuildNodeOutput(MaterialGraphNodeDef& nodeDef, int32_t outputIndex);
        MIRValue* BuildInput(const MaterialGraphNodeDefInput& input);

        // Constant builders
        MIRValue* ConstantFloat(float x);
        MIRValue* ConstantFloat2(float x, float y);
        MIRValue* ConstantFloat3(float x, float y, float z);
        MIRValue* ConstantFloat4(float x, float y, float z, float w);

        // Binary ops builders
        MIRValue* Add(MIRValue* left, MIRValue* right);
        MIRValue* Multiply(MIRValue* left, MIRValue* right);

        // Resource builders
        MIRValue* Texture2DParameter(const std::string& name);
        MIRValue* TextureSample(MIRValue* texture, MIRValue* uv);

        void SetMaterialOutput(MaterialProperty property, MIRValue* value);

    private:
        struct OutputKey
        {
            const MaterialGraphNodeDef* Node = nullptr;
            int32_t OutputIndex = -1;

            bool operator==(const OutputKey& other) const
            {
                return Node == other.Node && OutputIndex == other.OutputIndex;
            }
        };

        struct OutputKeyHash
        {
            size_t operator()(const OutputKey& key) const
            {
                return std::hash<const MaterialGraphNodeDef*>()(key.Node) ^ (std::hash<int32_t>()(key.OutputIndex) << 1);
            }
        };

        MIRValue* CreateLiteralValue(const MaterialLiteralValue& value);
        MIRValue* CreateNodeValue(MaterialOp op, MaterialValueType outputType, const std::vector<MIRValue*>& inputs, const std::string& symbolName = std::string());
        MIRNode* CreateNode(MaterialOp op, const std::vector<MIRValue*>& inputs, const std::string& symbolName);
        MaterialValueType ResolveBinaryResultType(MaterialValueType leftType, MaterialValueType rightType) const;

        MIRGraph& m_Graph;
        std::unordered_map<OutputKey, MIRValue*, OutputKeyHash> m_OutputCache;
        std::unordered_set<OutputKey, OutputKeyHash> m_ActiveBuilds;
    };
}
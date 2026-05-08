#pragma once

#include "Core.h"
#include "../MaterialTypes.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace minEngine
{
    struct MIRNode;

    struct MIRValue
    {
        int32_t Id = -1;
        MaterialValueType ValueType = MaterialValueType::Unknown;
        MaterialLiteralValue LiteralValue{};
        MIRNode* Producer = nullptr;
    };

    struct MIRNode
    {
        int32_t Id = -1;
        MaterialOp Op = MaterialOp::Constant;
        std::vector<MIRValue*> Inputs;
        std::vector<MIRValue*> Outputs;
        std::string SymbolName;
    };

    struct MIRGraph
    {
        int32_t NextNodeId = 0;
        int32_t NextValueId = 0;
        std::vector<std::unique_ptr<MIRNode>> Nodes;
        std::vector<std::unique_ptr<MIRValue>> Values;
        std::unordered_map<MaterialProperty, MIRValue*> Outputs;
    };
}

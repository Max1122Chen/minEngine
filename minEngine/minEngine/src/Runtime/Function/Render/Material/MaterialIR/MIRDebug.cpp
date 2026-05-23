#include "MIRDebug.h"

#include "MIRGraph.h"
#include "MaterialIRTypes.h"

#include <fstream>
#include <sstream>
#include <unordered_map>

namespace minEngine
{
    namespace
    {
        const char* LexStageName(ShaderStage stage)
        {
            switch (stage)
            {
            case Stage_Vertex: return "Vertex";
            case Stage_Fragment: return "Fragment";
            default: return "Unknown";
            }
        }

        const char* LexValueKind(MIRValueKind kind)
        {
            switch (kind)
            {
            case VK_Poison: return "Poison";
            case VK_Constant: return "Constant";
            case VK_Operator: return "Operator";
            case VK_Cast: return "Cast";
            case VK_Dimensional: return "Dimensional";
            case VK_Subscript: return "Subscript";
            case VK_ExternalInput: return "ExternalInput";
            case VK_TextureObject: return "TextureObject";
            case VK_TextureRead: return "TextureRead";
            case VK_UniformParameter: return "UniformParameter";
            case VK_Branch: return "Branch";
            case VK_SetMaterialOutput: return "SetMaterialOutput";
            default: return "Unknown";
            }
        }

        const char* LexScalarKind(MIRScalarKind kind)
        {
            switch (kind)
            {
            case SK_Bool: return "bool";
            case SK_Int: return "int";
            case SK_Float: return "float";
            default: return "?";
            }
        }

        const char* LexOperator(MIROperatorCode op)
        {
            switch (op)
            {
            case UO_Negative: return "Negative";
            case UO_Not: return "Not";
            case BO_Add: return "Add";
            case BO_Multiply: return "Multiply";
            case BO_Subtract: return "Subtract";
            case BO_Divide: return "Divide";
            case BO_Max: return "Max";
            case BO_Min: return "Min";
            case TO_Select: return "Select";
            default: return "Invalid";
            }
        }

        const char* LexMaterialProperty(MaterialProperty property)
        {
            switch (property)
            {
            case MP_Albedo: return "Albedo";
            case MP_Normal: return "Normal";
            case MP_AO: return "AO";
            case MP_Metallic: return "Metallic";
            case MP_Roughness: return "Roughness";
            case MP_Emissive: return "Emissive";
            case MP_Opacity: return "Opacity";
            default: return "Unknown";
            }
        }

        std::string GetTypeSpelling(const MIRValueType* type)
        {
            if (type == nullptr || type->IsPoison())
            {
                return "poison";
            }

            if (const MIRPrimitiveType* primitive = type->AsPrimitive())
            {
                const char* scalar = LexScalarKind(primitive->ScalarKind);
                if (primitive->IsScalar())
                {
                    return scalar;
                }
                if (primitive->IsVector())
                {
                    std::ostringstream out;
                    out << scalar << primitive->NumRows;
                    return out.str();
                }
                if (primitive->IsMatrix())
                {
                    std::ostringstream out;
                    out << scalar << primitive->NumRows << 'x' << primitive->NumCols;
                    return out.str();
                }
            }

            if (const MIRObjectType* object = type->AsObject())
            {
                if (object->ObjectKind == OK_Texture2D)
                {
                    return "Texture2D";
                }
            }

            return "unknown";
        }

        bool InstrHasVariableArgCount(MIRValueKind kind)
        {
            return kind == VK_Operator;
        }

        struct MIRDebugDumpState
        {
            const MIRGraph* Graph = nullptr;
            std::ostringstream Out;
            std::string Temp;
            ShaderStage CurrentStage = Stage_Fragment;
            std::unordered_map<const MIRValue*, uint32_t> ValueToId;
            std::unordered_map<const MIRBlock*, uint32_t> BlockToId;
            uint32_t NextValueId = 0;
            uint32_t NextBlockId = 0;

            uint32_t ReferenceBlock(const MIRBlock* block)
            {
                if (block == nullptr)
                {
                    return 0;
                }
                auto found = BlockToId.find(block);
                if (found != BlockToId.end())
                {
                    return found->second;
                }
                const uint32_t id = ++NextBlockId;
                BlockToId.emplace(block, id);
                return id;
            }

            uint32_t ReferenceInstruction(const MIRInstruction* instr)
            {
                auto found = ValueToId.find(instr);
                if (found != ValueToId.end())
                {
                    return found->second;
                }
                const uint32_t id = NextValueId++;
                ValueToId.emplace(instr, id);
                return id;
            }

            void AppendLeftColumn(int indentation, const std::string& leftColumn)
            {
                for (int i = 0; i < indentation; ++i)
                {
                    Out << "        ";
                }

                const int padding = 8 - static_cast<int>(leftColumn.size());
                for (int i = 0; i < padding; ++i)
                {
                    Out << ' ';
                }
                Out << leftColumn;
            }

            void AppendValueReference(const MIRValue* value)
            {
                if (value == nullptr)
                {
                    Out << "null";
                    return;
                }

                if (value->IsA(VK_Poison))
                {
                    Out << "Poison";
                    return;
                }

                if (const MIRConstant* constant = dynamic_cast<const MIRConstant*>(value))
                {
                    if (constant->IsBool())
                    {
                        Out << (constant->Get<bool>() ? "true" : "false");
                    }
                    else if (constant->IsInt())
                    {
                        Out << constant->Get<int64_t>();
                    }
                    else if (constant->IsFloat())
                    {
                        Out << constant->Get<double>() << 'f';
                    }
                    else
                    {
                        Out << "[Constant]";
                    }
                    return;
                }

                if (const MIRDimensional* dimensional = AsDimensional(value))
                {
                    const MIRPrimitiveType* primitive = dimensional->Type != nullptr ? dimensional->Type->AsPrimitive() : nullptr;
                    if (primitive != nullptr && primitive->IsVector())
                    {
                        Out << GetTypeSpelling(primitive) << '(';
                        for (int componentIndex = 0; componentIndex < dimensional->GetNumComponents(); ++componentIndex)
                        {
                            if (componentIndex > 0)
                            {
                                Out << ", ";
                            }
                            AppendValueReference(dimensional->Components[componentIndex]);
                        }
                        Out << ')';
                        return;
                    }
                }

                if (value->IsInstructionValue())
                {
                    auto found = ValueToId.find(value);
                    if (found != ValueToId.end())
                    {
                        Out << '%' << found->second;
                        return;
                    }
                    Out << '[' << LexValueKind(value->Kind) << ']';
                    return;
                }

                Out << '[' << LexValueKind(value->Kind) << ']';
            }

            void AppendUseLabel(const MIRInstruction* instr, int32_t useIndex)
            {
                if (instr->Kind == VK_Branch)
                {
                    static const char* labels[] = { "condition", "true", "false" };
                    if (useIndex >= 0 && useIndex < 3)
                    {
                        Out << labels[useIndex];
                        return;
                    }
                }
                else if (instr->Kind == VK_Operator)
                {
                    static const char* labels[] = { "a", "b", "c" };
                    if (useIndex >= 0 && useIndex < 3)
                    {
                        Out << labels[useIndex];
                        return;
                    }
                }
                Out << "use" << useIndex;
            }

            void AppendInstructionProperties(const MIRInstruction* instr)
            {
                if (const SetMaterialOutput* output = dynamic_cast<const SetMaterialOutput*>(instr))
                {
                    Out << " \"" << LexMaterialProperty(output->Property) << '"';
                }
                else if (const MIROperator* op = dynamic_cast<const MIROperator*>(instr))
                {
                    Out << " \"" << LexOperator(op->Op) << '"';
                }
                else if (const MIRCast* cast = dynamic_cast<const MIRCast*>(instr))
                {
                    Out << " -> " << GetTypeSpelling(cast->Type);
                }
                else if (const MIRSubscript* subscript = dynamic_cast<const MIRSubscript*>(instr))
                {
                    Out << " [" << subscript->Index << ']';
                }
                else if (const MIRExternalInput* externalInput = dynamic_cast<const MIRExternalInput*>(instr))
                {
                    Out << " \"" << static_cast<int>(externalInput->InputId) << '"';
                }
                else if (const MIRTextureObject* textureObject = dynamic_cast<const MIRTextureObject*>(instr))
                {
                    Out << " slot " << textureObject->TextureSlotIndex;
                }
                else if (const MIRUniformParameter* uniformParameter = dynamic_cast<const MIRUniformParameter*>(instr))
                {
                    Out << " slot " << uniformParameter->UniformSlotIndex
                        << " default " << uniformParameter->DefaultValue << 'f';
                }
            }

            void AppendBlock(const MIRBlock& block, int indentation)
            {
                const int stageIndex = static_cast<int>(CurrentStage);
                for (MIRInstruction* instr = block.FirstInstruction; instr != nullptr; instr = instr->Next[stageIndex])
                {
                    Temp.clear();
                    if (instr->Kind != VK_SetMaterialOutput)
                    {
                        Temp = '%' + std::to_string(ReferenceInstruction(instr)) + " = ";
                    }

                    AppendLeftColumn(indentation, Temp);
                    Out << LexValueKind(instr->Kind) << " (";

                    const std::vector<MIRValue*> uses = instr->GetUsesForStage(CurrentStage);
                    bool addComma = false;
                    for (int32_t useIndex = 0; useIndex < static_cast<int32_t>(uses.size()); ++useIndex)
                    {
                        MIRValue* use = uses[useIndex];
                        if (use == nullptr && InstrHasVariableArgCount(instr->Kind))
                        {
                            continue;
                        }

                        if (addComma)
                        {
                            Out << ", ";
                        }
                        addComma = true;

                        if (use == nullptr)
                        {
                            Out << "null";
                            continue;
                        }

                        Out << GetTypeSpelling(use->Type) << ' ';

                        MIRBlock* useBlock = instr->GetDesiredBlockForUse(CurrentStage, useIndex);
                        MIRBlock* instrBlock = instr->Block[stageIndex];
                        if (useBlock != nullptr && useBlock != instrBlock && useBlock->FirstInstruction != nullptr)
                        {
                            Out << "{ ; block B" << ReferenceBlock(useBlock) << '\n';
                            AppendBlock(*useBlock, indentation + 1);
                            AppendLeftColumn(indentation, "");
                            Out << "} ";
                        }

                        AppendValueReference(use);
                    }

                    Out << ')';
                    AppendInstructionProperties(instr);
                    Out << '\n';
                }
            }

            void DumpStage(ShaderStage stage)
            {
                CurrentStage = stage;
                ValueToId.clear();
                NextValueId = 0;

                const int stageIndex = static_cast<int>(stage);
                Out << "\n; Stage " << stageIndex << " \"" << LexStageName(stage) << "\"\n";

                MIRBlock* rootBlock = Graph->GetRootBlock(stage);
                if (rootBlock == nullptr)
                {
                    Out << "; (no root block)\n";
                    return;
                }

                Out << "; root block B" << ReferenceBlock(rootBlock) << '\n';
                AppendBlock(*rootBlock, 0);

                const std::vector<SetMaterialOutput*>& outputs = Graph->GetOutputs(stage);
                if (!outputs.empty())
                {
                    Out << "; outputs (" << outputs.size() << ")\n";
                    for (const SetMaterialOutput* output : outputs)
                    {
                        Out << ";   SetMaterialOutput \"" << LexMaterialProperty(output->Property) << "\"";
                        if (output->Block[stageIndex] != nullptr)
                        {
                            Out << " in block B" << ReferenceBlock(output->Block[stageIndex]);
                        }
                        Out << '\n';
                    }
                }
            }
        };
    }

    std::string DebugDumpMIR(const MIRGraph& graph, const char* materialName)
    {
        MIRDebugDumpState state;
        state.Graph = &graph;

        state.Out << "; minEngine Material IR dump\n";
        state.Out << ";    Material: " << (materialName != nullptr ? materialName : "Material") << '\n';

        for (int stageIndex = 0; stageIndex < NumStages; ++stageIndex)
        {
            state.DumpStage(static_cast<ShaderStage>(stageIndex));
        }

        const std::vector<std::string>& diagnostics = graph.GetDiagnostics();
        if (!diagnostics.empty())
        {
            state.Out << "\n; Build diagnostics\n";
            for (const std::string& diagnostic : diagnostics)
            {
                state.Out << ";   " << diagnostic << '\n';
            }
        }

        return state.Out.str();
    }

    bool WriteMIRDumpToFile(const MIRGraph& graph, const std::string& filePath, const char* materialName)
    {
        std::ofstream output(filePath, std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }
        output << DebugDumpMIR(graph, materialName);
        return output.good();
    }
}

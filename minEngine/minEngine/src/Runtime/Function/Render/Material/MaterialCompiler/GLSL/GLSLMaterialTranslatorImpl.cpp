#include "GLSLMaterialTranslatorImpl.h"

#include "GLSLShaderBinding.h"

#include <algorithm>
#include <vector>
#include "Render/Material/MaterialIR/MIRGraph.h"
#include "Render/Material/MaterialIR/MaterialIR.h"
#include "Render/Material/MaterialIR/MaterialIRTypes.h"
#include "Render/Material/MaterialPropertyUtil.h"

namespace minEngine
{
    MaterialCompileResult GLSLMaterialTranslatorImpl::Translate(const MIRGraph& graph, const MaterialCompileEnvironment& env)
    {
        (void)env;
        MaterialCompileResult result;

        if (graph.GetOutputs(Stage_Fragment).empty())
        {
            result.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "MIR graph has no fragment material outputs.",
            });
            return result;
        }

        MIRBlock* fragmentRootBlock = graph.GetRootBlock(Stage_Fragment);
        if (fragmentRootBlock == nullptr || fragmentRootBlock->FirstInstruction == nullptr)
        {
            result.Diagnostics.push_back({
                MaterialCompileDiagnostic::Error,
                "MIR graph has no linked fragment instructions.",
            });
            return result;
        }

        m_Graph = &graph;
        m_ActiveResult = &result;
        m_UsedTextureSlots.clear();
        m_UsedScalarUniformSlots.clear();
        m_UsesTexCoord0 = false;

        BeginStage(Stage_Vertex);
        if (MIRBlock* vertexRootBlock = graph.GetRootBlock(Stage_Vertex);
            vertexRootBlock != nullptr && vertexRootBlock->FirstInstruction != nullptr)
        {
            LowerBlock(*vertexRootBlock);
        }
        result.Stages[Stage_Vertex].Stage = Stage_Vertex;
        result.Stages[Stage_Vertex].Body = m_Printer.TakeBuffer();

        BeginStage(Stage_Fragment);
        LowerBlock(*fragmentRootBlock);
        result.Stages[Stage_Fragment].Stage = Stage_Fragment;
        result.Stages[Stage_Fragment].Preamble = BuildFragmentShaderPreamble();
        result.Stages[Stage_Fragment].Body = m_Printer.TakeBuffer();
        result.UsesTexCoord0 = m_UsesTexCoord0;
        FillParameterLayout(result);

        m_ActiveResult = nullptr;
        m_Graph = nullptr;

        result.Succeeded = std::none_of(
            result.Diagnostics.begin(),
            result.Diagnostics.end(),
            [](const MaterialCompileDiagnostic& diagnostic)
            {
                return diagnostic.Level == MaterialCompileDiagnostic::Error;
            });
        return result;
    }

    void GLSLMaterialTranslatorImpl::AddLoweringError(MaterialCompileResult& result, const char* message) const
    {
        result.Diagnostics.push_back({ MaterialCompileDiagnostic::Error, message });
    }

    void GLSLMaterialTranslatorImpl::FillParameterLayout(MaterialCompileResult& result) const
    {
        result.ParameterLayout.Parameters.clear();

        std::vector<int> textureSlots(m_UsedTextureSlots.begin(), m_UsedTextureSlots.end());
        std::sort(textureSlots.begin(), textureSlots.end());
        for (int textureSlotIndex : textureSlots)
        {
            MaterialShaderParameterDesc desc;
            desc.Type = MaterialShaderParameterType::Texture2D;
            desc.SlotIndex = textureSlotIndex;
            desc.ShaderSymbolName = GetTextureSamplerName(textureSlotIndex);
            result.ParameterLayout.Parameters.push_back(desc);
        }

        std::vector<int> scalarSlots(m_UsedScalarUniformSlots.begin(), m_UsedScalarUniformSlots.end());
        std::sort(scalarSlots.begin(), scalarSlots.end());
        for (int scalarSlotIndex : scalarSlots)
        {
            MaterialShaderParameterDesc desc;
            desc.Type = MaterialShaderParameterType::Scalar;
            desc.SlotIndex = scalarSlotIndex;
            desc.ShaderSymbolName = GetScalarUniformName(scalarSlotIndex);
            result.ParameterLayout.Parameters.push_back(desc);
        }
    }

    void GLSLMaterialTranslatorImpl::BeginStage(ShaderStage stage)
    {
        m_Stage = stage;
        m_Printer = {};
        m_Printer.Tabs = 1;
        m_Printer.Indent();
        m_NumLocals = 0;
        m_LocalIdentifier.clear();
    }

    bool GLSLMaterialTranslatorImpl::IsFoldable(const MIRInstruction& instr, ShaderStage stage)
    {
        if (instr.Kind == VK_Branch)
        {
            const MIRBranch& branch = static_cast<const MIRBranch&>(instr);
            return branch.TrueBlock[stage].FirstInstruction == nullptr
                && branch.FalseBlock[stage].FirstInstruction == nullptr;
        }

        if (instr.Kind == VK_TextureRead)
        {
            return false;
        }

        return true;
    }

    bool GLSLMaterialTranslatorImpl::IsOperatorInfix(MIROperatorCode op)
    {
        switch (op)
        {
        case BO_Add:
        case BO_Multiply:
        case BO_Subtract:
        case BO_Divide:
            return true;
        default:
            return false;
        }
    }

    void GLSLMaterialTranslatorImpl::AppendFragmentMaterialInputName(MaterialProperty property)
    {
        m_Printer.Append(GetFragmentMaterialInputsSymbol()).Append(".").Append(GetMaterialPropertyName(property));
    }

    void GLSLMaterialTranslatorImpl::LowerBlock(const MIRBlock& block)
    {
        const int oldNumLocals = m_NumLocals;
        for (MIRInstruction* instr = block.FirstInstruction; instr != nullptr; instr = instr->Next[m_Stage])
        {
            if (instr->NumUsers[m_Stage] == 1 && IsFoldable(*instr, m_Stage))
            {
                continue;
            }

            if (instr->NumUsers[m_Stage] >= 1)
            {
                const std::string localName = "_" + std::to_string(m_NumLocals++);
                m_LocalIdentifier[instr] = localName;

                LowerType(instr->Type);
                m_Printer.Append(' ').Append(localName);

                if (instr->Kind != VK_Branch && instr->Kind != VK_SetMaterialOutput)
                {
                    m_Printer.Append(" = ");
                }
            }

            LowerInstruction(*instr);

            if (m_Printer.EndsWith('}'))
            {
                m_Printer.NewLine();
            }
            else
            {
                m_Printer.EndStatement();
            }
        }
        m_NumLocals = oldNumLocals;
    }

    void GLSLMaterialTranslatorImpl::LowerValue(const MIRValue* value)
    {
        if (value == nullptr)
        {
            m_Printer.Append("0.0");
            return;
        }

        if (const MIRInstruction* instr = AsInstruction(value))
        {
            if (auto localIt = m_LocalIdentifier.find(instr); localIt != m_LocalIdentifier.end())
            {
                m_Printer.Append(localIt->second);
                return;
            }

            if (instr->NumUsers[m_Stage] <= 1 && IsFoldable(*instr, m_Stage))
            {
                LowerInstruction(*instr);
                return;
            }

            if (IsFoldable(*instr, m_Stage))
            {
                // Pure multi-use value not materialized in the current block — duplicate inline (UE-safe for constants).
                LowerInstruction(*instr);
                return;
            }

            if (m_ActiveResult != nullptr)
            {
                AddLoweringError(
                    *m_ActiveResult,
                    "MIR lowering referenced a non-foldable instruction without a pre-declared local.");
            }
            m_Printer.Append("0.0");
            return;
        }

        switch (value->Kind)
        {
        case VK_Constant:
            LowerConstant(static_cast<const MIRConstant*>(value));
            break;
        case VK_Poison:
            m_Printer.Append("0.0");
            break;
        default:
            if (m_ActiveResult != nullptr)
            {
                AddLoweringError(*m_ActiveResult, "Unsupported MIR value kind in GLSL lowering.");
            }
            m_Printer.Append("0.0");
            break;
        }
    }

    void GLSLMaterialTranslatorImpl::LowerInstruction(const MIRInstruction& instr)
    {
        switch (instr.Kind)
        {
        case VK_Cast:
            LowerCast(static_cast<const MIRCast&>(instr));
            break;
        case VK_Dimensional:
            LowerDimensional(static_cast<const MIRDimensional&>(instr));
            break;
        case VK_Subscript:
            LowerSubscript(static_cast<const MIRSubscript&>(instr));
            break;
        case VK_ExternalInput:
            LowerExternalInput(static_cast<const MIRExternalInput&>(instr));
            break;
        case VK_TextureObject:
            LowerTextureObject(static_cast<const MIRTextureObject&>(instr));
            break;
        case VK_TextureRead:
            LowerTextureRead(static_cast<const MIRTextureRead&>(instr));
            break;
        case VK_UniformParameter:
            LowerUniformParameter(static_cast<const MIRUniformParameter&>(instr));
            break;
        case VK_Operator:
            LowerOperator(static_cast<const MIROperator&>(instr));
            break;
        case VK_Branch:
            LowerBranch(static_cast<const MIRBranch&>(instr));
            break;
        case VK_SetMaterialOutput:
            LowerSetMaterialOutput(static_cast<const SetMaterialOutput&>(instr));
            break;
        default:
            if (m_ActiveResult != nullptr)
            {
                AddLoweringError(*m_ActiveResult, "Unsupported MIR instruction kind in GLSL lowering.");
            }
            m_Printer.Append("0.0");
            break;
        }
    }

    void GLSLMaterialTranslatorImpl::LowerType(const MIRValueType* type)
    {
        const MIRPrimitiveType* primitive = type != nullptr ? type->AsPrimitive() : nullptr;
        if (primitive == nullptr)
        {
            m_Printer.Append("float");
            return;
        }

        if (primitive->IsVector())
        {
            m_Printer.Append("vec").Append(std::to_string(primitive->NumRows));
            return;
        }

        switch (primitive->ScalarKind)
        {
        case SK_Bool:
            m_Printer.Append("bool");
            break;
        case SK_Int:
            m_Printer.Append("int");
            break;
        case SK_Float:
            m_Printer.Append("float");
            break;
        default:
            m_Printer.Append("float");
            break;
        }
    }

    int GLSLMaterialTranslatorImpl::ExternalInputIdToTexCoordIndex(MIRExternalInputId inputId)
    {
        switch (inputId)
        {
        case EI_TexCoord0:
            return 0;
        default:
            return -1;
        }
    }

    std::string GLSLMaterialTranslatorImpl::GetTextureSamplerName(int textureSlotIndex) const
    {
        return "u_Texture" + std::to_string(textureSlotIndex);
    }

    std::string GLSLMaterialTranslatorImpl::GetScalarUniformName(int uniformSlotIndex) const
    {
        return "u_ScalarParams[" + std::to_string(uniformSlotIndex) + "]";
    }

    std::string GLSLMaterialTranslatorImpl::BuildFragmentShaderPreamble() const
    {
        std::string preamble;

        std::vector<int> textureSlots(m_UsedTextureSlots.begin(), m_UsedTextureSlots.end());
        std::sort(textureSlots.begin(), textureSlots.end());
        for (int textureSlotIndex : textureSlots)
        {
            preamble += "layout (binding = ";
            preamble += std::to_string(textureSlotIndex);
            preamble += ") uniform sampler2D ";
            preamble += GetTextureSamplerName(textureSlotIndex);
            preamble += ";\n";
        }

        std::vector<int> scalarUniformSlots(m_UsedScalarUniformSlots.begin(), m_UsedScalarUniformSlots.end());
        std::sort(scalarUniformSlots.begin(), scalarUniformSlots.end());
        if (!scalarUniformSlots.empty())
        {
            const int maxSlot = scalarUniformSlots.back();
            preamble += "layout (std140, binding = 8) uniform MaterialScalarParams\n{\n";
            preamble += "    float u_ScalarParams[" + std::to_string(maxSlot + 1) + "];\n";
            preamble += "};\n";
        }

        if (!preamble.empty())
        {
            preamble += '\n';
        }

        return preamble;
    }

    void GLSLMaterialTranslatorImpl::LowerExternalInput(const MIRExternalInput& externalInput)
    {
        if (externalInput.InputId == EI_WorldNormal)
        {
            m_Printer.Append("v_WorldNormal");
            return;
        }

        const int texCoordIndex = ExternalInputIdToTexCoordIndex(externalInput.InputId);
        if (texCoordIndex >= 0)
        {
            if (texCoordIndex == 0)
            {
                m_UsesTexCoord0 = true;
            }
            m_Printer.Append(GetGLSLMaterialParametersTexCoordAccess(texCoordIndex));
            return;
        }

        m_Printer.Append("vec2(0.0)");
    }

    void GLSLMaterialTranslatorImpl::LowerTextureObject(const MIRTextureObject& textureObject)
    {
        m_UsedTextureSlots.insert(textureObject.TextureSlotIndex);
        m_Printer.Append(GetTextureSamplerName(textureObject.TextureSlotIndex));
    }

    void GLSLMaterialTranslatorImpl::LowerTextureRead(const MIRTextureRead& textureRead)
    {
        if (const MIRTextureObject* textureObject = dynamic_cast<const MIRTextureObject*>(textureRead.Texture))
        {
            m_UsedTextureSlots.insert(textureObject->TextureSlotIndex);
        }

        if (const MIRExternalInput* externalInput = dynamic_cast<const MIRExternalInput*>(textureRead.TexCoord))
        {
            if (externalInput->InputId == EI_TexCoord0)
            {
                m_UsesTexCoord0 = true;
            }
        }

        m_Printer.Append("texture(");
        LowerValue(textureRead.Texture);
        m_Printer.Append(", ");
        LowerValue(textureRead.TexCoord);
        m_Printer.Append(')');
    }

    void GLSLMaterialTranslatorImpl::LowerUniformParameter(const MIRUniformParameter& uniformParameter)
    {
        m_UsedScalarUniformSlots.insert(uniformParameter.UniformSlotIndex);
        m_Printer.Append(GetScalarUniformName(uniformParameter.UniformSlotIndex));
    }

    void GLSLMaterialTranslatorImpl::LowerSubscript(const MIRSubscript& subscript)
    {
        LowerValue(subscript.Arg);
        static const char* const kSwizzles[] = { ".x", ".y", ".z", ".w" };
        if (subscript.Index >= 0 && subscript.Index < 4)
        {
            m_Printer.Append(kSwizzles[subscript.Index]);
        }
        else
        {
            m_Printer.Append(".x");
        }
    }

    void GLSLMaterialTranslatorImpl::LowerDimensional(const MIRDimensional& dimensional)
    {
        const MIRPrimitiveType* primitive = dimensional.Type != nullptr ? dimensional.Type->AsPrimitive() : nullptr;
        const int numComponents = dimensional.GetNumComponents();
        if (primitive == nullptr || !primitive->IsVector() || numComponents <= 0)
        {
            m_Printer.Append("vec3(0.0)");
            return;
        }

        m_Printer.Append("vec").Append(std::to_string(primitive->NumRows)).Append('(');
        for (int componentIndex = 0; componentIndex < numComponents; ++componentIndex)
        {
            if (componentIndex > 0)
            {
                m_Printer.Append(", ");
            }
            LowerValue(dimensional.Components[componentIndex]);
        }
        m_Printer.Append(')');
    }

    void GLSLMaterialTranslatorImpl::LowerCast(const MIRCast& cast)
    {
        const MIRPrimitiveType* targetPrimitive = cast.Type != nullptr ? cast.Type->AsPrimitive() : nullptr;
        const MIRPrimitiveType* sourcePrimitive =
            cast.Arg != nullptr && cast.Arg->Type != nullptr ? cast.Arg->Type->AsPrimitive() : nullptr;

        if (targetPrimitive != nullptr && targetPrimitive->ScalarKind == SK_Bool && sourcePrimitive != nullptr
            && sourcePrimitive->ScalarKind == SK_Float)
        {
            m_Printer.Append('(');
            LowerValue(cast.Arg);
            m_Printer.Append(" != 0.0)");
            return;
        }

        LowerValue(cast.Arg);
    }

    void GLSLMaterialTranslatorImpl::LowerConstant(const MIRConstant* constant)
    {
        if (constant == nullptr || constant->Type == nullptr)
        {
            m_Printer.Append("0.0");
            return;
        }

        if (constant->IsBool())
        {
            m_Printer.Append(constant->Get<bool>() ? "true" : "false");
            return;
        }

        if (constant->IsInt())
        {
            m_Printer.Append(std::to_string(constant->Get<int64_t>()));
            return;
        }

        m_Printer.Append(std::to_string(constant->Get<double>()));
    }

    void GLSLMaterialTranslatorImpl::LowerOperator(const MIROperator& op)
    {
        if (IsOperatorInfix(op.Op))
        {
            const char* opToken = "+";
            switch (op.Op)
            {
            case BO_Multiply: opToken = "*"; break;
            case BO_Subtract: opToken = "-"; break;
            case BO_Divide: opToken = "/"; break;
            default: break;
            }

            m_Printer.Append('(');
            LowerValue(op.Args[0]);
            m_Printer.Append(' ').Append(opToken).Append(' ');
            LowerValue(op.Args[1]);
            m_Printer.Append(')');
            return;
        }

        switch (op.Op)
        {
        case UO_Negative:
            m_Printer.Append("(-");
            LowerValue(op.Args[0]);
            m_Printer.Append(')');
            return;
        case UO_Not:
            m_Printer.Append("(!");
            LowerValue(op.Args[0]);
            m_Printer.Append(')');
            return;
        case BO_Max:
            m_Printer.Append("max(");
            LowerValue(op.Args[0]);
            m_Printer.Append(", ");
            LowerValue(op.Args[1]);
            m_Printer.Append(')');
            return;
        case BO_Min:
            m_Printer.Append("min(");
            LowerValue(op.Args[0]);
            m_Printer.Append(", ");
            LowerValue(op.Args[1]);
            m_Printer.Append(')');
            return;
        case TO_Select:
            LowerValue(op.Args[0]);
            m_Printer.Append(" ? ");
            LowerValue(op.Args[1]);
            m_Printer.Append(" : ");
            LowerValue(op.Args[2]);
            return;
        default:
            break;
        }

        if (m_ActiveResult != nullptr)
        {
            AddLoweringError(*m_ActiveResult, "Unsupported MIR operator in GLSL lowering.");
        }
        m_Printer.Append("0.0");
    }

    void GLSLMaterialTranslatorImpl::LowerBranch(const MIRBranch& branch)
    {
        if (IsFoldable(branch, m_Stage))
        {
            LowerValue(branch.Condition);
            m_Printer.Append(" ? ");
            LowerValue(branch.TrueArg);
            m_Printer.Append(" : ");
            LowerValue(branch.FalseArg);
            return;
        }

        auto localIt = m_LocalIdentifier.find(&branch);
        if (localIt == m_LocalIdentifier.end())
        {
            if (m_ActiveResult != nullptr)
            {
                AddLoweringError(*m_ActiveResult, "Branch lowering requires a pre-declared local.");
            }
            return;
        }

        const std::string& localName = localIt->second;

        m_Printer.EndStatement();
        m_Printer.Append("if (");
        LowerValue(branch.Condition);
        m_Printer.Append(')').NewLine().OpenBrace();
        LowerBlock(branch.TrueBlock[m_Stage]);
        m_Printer.Indent().Append(localName).Append(" = ");
        LowerValue(branch.TrueArg);
        m_Printer.EndStatement();
        m_Printer.CloseBrace().NewLine();
        m_Printer.Append("else").NewLine().OpenBrace();
        LowerBlock(branch.FalseBlock[m_Stage]);
        m_Printer.Indent().Append(localName).Append(" = ");
        LowerValue(branch.FalseArg);
        m_Printer.EndStatement();
        m_Printer.CloseBrace();
    }

    void GLSLMaterialTranslatorImpl::LowerSetMaterialOutput(const SetMaterialOutput& output)
    {
        if (output.Property == MP_WorldPositionOffset)
        {
            m_Printer.Append("return ");
            LowerValue(output.Arg);
            return;
        }

        AppendFragmentMaterialInputName(output.Property);
        m_Printer.Append(" = ");
        LowerValue(output.Arg);
    }
}

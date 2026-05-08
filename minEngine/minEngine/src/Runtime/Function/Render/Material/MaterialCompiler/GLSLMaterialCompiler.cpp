#include "GLSLMaterialCompiler.h"

#include "../MaterialEdGraph.h"
#include "../MaterialIR/MIRGraph.h"
#include "../MaterialIR/MIRBuilder.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef.h"
#include "../MaterialGraphNodeDefs/MaterialGraphNodeDef_Output.h"

#include <algorithm>

namespace minEngine
{
    MaterialCompileResult GLSLMaterialCompiler::Compile(const MaterialEdGraph& graph, int32_t nodeIndex, int32_t outputIndex)
    {
        MaterialCompileResult result;

        int32_t outputIndexNode = -1;
        for (int32_t i = 0; i < static_cast<int32_t>(graph.m_Nodes.size()); ++i)
        {
            const MaterialEdGraphNode& node = graph.m_Nodes[i];
            if (node.m_Definition && dynamic_cast<MaterialGraphNodeDef_MaterialOutput*>(node.m_Definition))
            {
                outputIndexNode = i;
                break;
            }
        }

        if (outputIndexNode < 0)
        {
            result.Diagnostics.push_back({ MaterialCompileDiagnostic::Severity::Error, "MaterialEdGraph does not contain a MaterialOutput node." });
            return result;
        }

        if (nodeIndex >= 0 && nodeIndex != outputIndexNode)
        {
            result.Diagnostics.push_back({ MaterialCompileDiagnostic::Severity::Error, "Arbitrary node compilation is not supported; compile from MaterialOutput." });
            return result;
        }

        const MaterialEdGraphNode& entryNode = graph.m_Nodes[outputIndexNode];
        if (entryNode.m_Definition == nullptr)
        {
            result.Diagnostics.push_back({ MaterialCompileDiagnostic::Severity::Error, "MaterialEdGraph entry node does not have a MaterialGraphNodeDef." });
            return result;
        }

        MIRGraph irGraph;
        MIRBuilder builder(irGraph);
        MIRValue* outputValue = builder.BuildNodeOutput(*entryNode.m_Definition, outputIndex);
        if (outputValue == nullptr)
        {
            result.Diagnostics.push_back({ MaterialCompileDiagnostic::Severity::Error, "Failed to build MIR from the MaterialOutput node." });
            return result;
        }

        m_CodeChunks.clear();
        m_ValueToCode.clear();
        m_TextureUniforms.clear();

        const int32_t finalCode = TranslateMaterialProperty(irGraph, MaterialProperty::Albedo);
        if (finalCode < 0)
        {
            result.Diagnostics.push_back({ MaterialCompileDiagnostic::Severity::Error, "Failed to translate BaseColor from MIR." });
            return result;
        }

        result.ShaderSource = MakeGLSLSourceFromCode(finalCode);
        return result;
    }

    int32_t GLSLMaterialCompiler::Constant(float x)
    {
        return GenericConstant(MaterialLiteralValue::MakeFloat(x));
    }

    int32_t GLSLMaterialCompiler::Constant2(float x, float y)
    {
        return GenericConstant(MaterialLiteralValue::MakeVector2(x, y));
    }

    int32_t GLSLMaterialCompiler::Constant3(float x, float y, float z)
    {
        return GenericConstant(MaterialLiteralValue::MakeVector3(x, y, z));
    }

    int32_t GLSLMaterialCompiler::Constant4(float x, float y, float z, float w)
    {
        return GenericConstant(MaterialLiteralValue::MakeVector4(x, y, z, w));
    }

    int32_t GLSLMaterialCompiler::Add(int32_t left, int32_t right)
    {
        return ResolveBinaryOp(left, right, "+");
    }

    int32_t GLSLMaterialCompiler::Sub(int32_t left, int32_t right)
    {
        return ResolveBinaryOp(left, right, "-");
    }

    int32_t GLSLMaterialCompiler::Multiply(int32_t left, int32_t right)
    {
        return ResolveBinaryOp(left, right, "*");
    }

    int32_t GLSLMaterialCompiler::GenericConstant(const MaterialLiteralValue& value)
    {
        return AddCodeChunk(value.Type, MakeLiteralExpression(value));
    }

    MaterialValueType GLSLMaterialCompiler::GetType(int32_t code) const
    {
        if (code < 0 || code >= static_cast<int32_t>(m_CodeChunks.size()))
        {
            return MaterialValueType::Unknown;
        }

        return m_CodeChunks[code].Type;
    }

    int32_t GLSLMaterialCompiler::AddCodeChunk(MaterialValueType type, std::string code)
    {
        m_CodeChunks.push_back(CodeChunk{ type, std::move(code) });
        return static_cast<int32_t>(m_CodeChunks.size() - 1);
    }

    int32_t GLSLMaterialCompiler::ResolveBinaryOp(int32_t left, int32_t right, const char* op)
    {
        if (left < 0 || right < 0)
        {
            return -1;
        }

        const MaterialValueType leftType = GetType(left);
        const MaterialValueType rightType = GetType(right);
        const MaterialValueType resultType = ResolveBinaryResultType(leftType, rightType);
        if (resultType == MaterialValueType::Unknown)
        {
            return -1;
        }

        return AddCodeChunk(resultType, MakeBinaryExpression(left, right, op));
    }

    MaterialValueType GLSLMaterialCompiler::ResolveBinaryResultType(MaterialValueType leftType, MaterialValueType rightType) const
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

    std::string GLSLMaterialCompiler::MakeBinaryExpression(int32_t left, int32_t right, const char* op)
    {
        const MaterialValueType leftType = GetType(left);
        const MaterialValueType rightType = GetType(right);
        std::string leftExpression = m_CodeChunks[left].Code;
        std::string rightExpression = m_CodeChunks[right].Code;

        if (leftType != rightType)
        {
            if (leftType == MaterialValueType::Float)
            {
                leftExpression = MakeTypedCastExpression(rightType, leftExpression);
            }
            else if (rightType == MaterialValueType::Float)
            {
                rightExpression = MakeTypedCastExpression(leftType, rightExpression);
            }
        }

        return "(" + leftExpression + " " + op + " " + rightExpression + ")";
    }

    std::string GLSLMaterialCompiler::MakeTypedCastExpression(MaterialValueType targetType, const std::string& expression) const
    {
        switch (targetType)
        {
        case MaterialValueType::Vector2:
            return "vec2(" + expression + ")";
        case MaterialValueType::Vector3:
            return "vec3(" + expression + ")";
        case MaterialValueType::Vector4:
            return "vec4(" + expression + ")";
        case MaterialValueType::Float:
        case MaterialValueType::Unknown:
        default:
            return expression;
        }
    }

    std::string GLSLMaterialCompiler::MakeLiteralExpression(const MaterialLiteralValue& value) const
    {
        switch (value.Type)
        {
        case MaterialValueType::Float:
            return MakeScalarLiteral(value.Data[0]);
        case MaterialValueType::Vector2:
            return "vec2(" + MakeScalarLiteral(value.Data[0]) + ", " + MakeScalarLiteral(value.Data[1]) + ")";
        case MaterialValueType::Vector3:
            return "vec3(" + MakeScalarLiteral(value.Data[0]) + ", " + MakeScalarLiteral(value.Data[1]) + ", " + MakeScalarLiteral(value.Data[2]) + ")";
        case MaterialValueType::Vector4:
            return "vec4(" + MakeScalarLiteral(value.Data[0]) + ", " + MakeScalarLiteral(value.Data[1]) + ", " + MakeScalarLiteral(value.Data[2]) + ", " + MakeScalarLiteral(value.Data[3]) + ")";
        case MaterialValueType::Texture2D:
        case MaterialValueType::TextureCube:
        case MaterialValueType::Unknown:
        default:
            return "0.0";
        }
    }

    std::string GLSLMaterialCompiler::MakeGLSLSourceFromCode(int32_t code) const
    {
        std::string source;
        source += "#version 330 core\n\n";

        for (const std::string& uniformName : m_TextureUniforms)
        {
            source += "uniform sampler2D " + uniformName + ";\n";
        }

        if (!m_TextureUniforms.empty())
        {
            source += "\n";
        }

        source += "out vec4 FragColor;\n\n";
        source += "void main()\n";
        source += "{\n";
        source += "    vec3 BaseColor = ";
        source += MakeColorExpression(code);
        source += ";\n";
        source += "    FragColor = vec4(BaseColor, 1.0);\n";
        source += "}\n";

        return source;
    }

    std::string GLSLMaterialCompiler::MakeColorExpression(int32_t code) const
    {
        if (code < 0 || code >= static_cast<int32_t>(m_CodeChunks.size()))
        {
            return "vec3(0.0)";
        }

        const CodeChunk& chunk = m_CodeChunks[code];
        switch (chunk.Type)
        {
        case MaterialValueType::Float:
            return "vec3(" + chunk.Code + ")";
        case MaterialValueType::Vector2:
            return "vec3(" + chunk.Code + ", 0.0)";
        case MaterialValueType::Vector3:
            return chunk.Code;
        case MaterialValueType::Vector4:
            return chunk.Code + ".rgb";
        case MaterialValueType::Unknown:
        case MaterialValueType::Texture2D:
        case MaterialValueType::TextureCube:
        default:
            return "vec3(0.0)";
        }
    }

    int32_t GLSLMaterialCompiler::TranslateValue(const MIRValue* value)
    {
        if (value == nullptr)
        {
            return -1;
        }

        const auto cachedIter = m_ValueToCode.find(value);
        if (cachedIter != m_ValueToCode.end())
        {
            return cachedIter->second;
        }

        int32_t code = -1;
        const MIRNode* producer = value->Producer;
        if (producer == nullptr)
        {
            code = GenericConstant(value->LiteralValue);
        }
        else
        {
            std::vector<int32_t> inputCodes;
            inputCodes.reserve(producer->Inputs.size());
            for (const MIRValue* input : producer->Inputs)
            {
                inputCodes.push_back(TranslateValue(input));
            }

            switch (producer->Op)
            {
            case MaterialOp::Constant:
                code = GenericConstant(value->LiteralValue);
                break;
            case MaterialOp::TextureObject:
                if (!producer->SymbolName.empty())
                {
                    RegisterTextureUniform(producer->SymbolName);
                    code = AddCodeChunk(value->ValueType, producer->SymbolName);
                }
                break;
            case MaterialOp::TextureSample:
                if (inputCodes.size() >= 2 && inputCodes[0] >= 0 && inputCodes[1] >= 0)
                {
                    std::string texCode = m_CodeChunks[inputCodes[0]].Code;
                    std::string uvCode = m_CodeChunks[inputCodes[1]].Code;
                    code = AddCodeChunk(MaterialValueType::Vector4, "texture(" + texCode + ", " + uvCode + ")");
                }
                break;
            case MaterialOp::Add:
                if (inputCodes.size() >= 2)
                {
                    code = Add(inputCodes[0], inputCodes[1]);
                }
                break;
            case MaterialOp::Multiply:
                if (inputCodes.size() >= 2)
                {
                    code = Multiply(inputCodes[0], inputCodes[1]);
                }
                break;
            default:
                break;
            }
        }

        if (code >= 0)
        {
            m_ValueToCode.emplace(value, code);
        }
        return code;
    }

    int32_t GLSLMaterialCompiler::TranslateMaterialProperty(const MIRGraph& graph, MaterialProperty property)
    {
        auto iter = graph.Outputs.find(property);
        if (iter == graph.Outputs.end())
        {
            return -1;
        }

        return TranslateValue(iter->second);
    }

    void GLSLMaterialCompiler::RegisterTextureUniform(const std::string& name)
    {
        if (name.empty())
        {
            return;
        }

        if (std::find(m_TextureUniforms.begin(), m_TextureUniforms.end(), name) == m_TextureUniforms.end())
        {
            m_TextureUniforms.push_back(name);
        }
    }

    std::string GLSLMaterialCompiler::MakeScalarLiteral(float value) const
    {
        std::string text = std::to_string(value);
        while (!text.empty() && text.back() == '0')
        {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.')
        {
            text.push_back('0');
        }
        if (text.empty())
        {
            text = "0.0";
        }
        return text;
    }
}
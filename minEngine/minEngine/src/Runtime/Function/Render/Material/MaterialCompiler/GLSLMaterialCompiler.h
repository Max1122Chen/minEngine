#pragma once

#include "MaterialCompiler.h"
#include "Runtime/Function/Render/Material/MaterialIR/MIRGraph.h"
#include "Runtime/Function/Render/Material/MaterialIR/MIRBuilder.h"
#include "Runtime/Function/Render/Material/MaterialGraphNodeDefs/MaterialGraphNodeDef.h"

namespace minEngine
{
    class MaterialEdGraph;

    class GLSLMaterialCompiler final : public MaterialCompiler
    {
    public:
        GLSLMaterialCompiler() = default;
        ~GLSLMaterialCompiler() override = default;

        MaterialCompileResult Compile(const MaterialEdGraph& graph, int32_t nodeIndex = -1, int32_t outputIndex = 0) override;

        int32_t Constant(float x) override;
        int32_t Constant2(float x, float y) override;
        int32_t Constant3(float x, float y, float z) override;
        int32_t Constant4(float x, float y, float z, float w) override;

        int32_t Add(int32_t left, int32_t right) override;
        int32_t Sub(int32_t left, int32_t right) override;
        int32_t Multiply(int32_t left, int32_t right) override;

        int32_t GenericConstant(const MaterialLiteralValue& value) override;
        MaterialValueType GetType(int32_t code) const override;

    private:
        struct CodeChunk
        {
            MaterialValueType Type = MaterialValueType::Unknown;
            std::string Code;
        };

        int32_t AddCodeChunk(MaterialValueType type, std::string code);
        int32_t ResolveBinaryOp(int32_t left, int32_t right, const char* op);
        MaterialValueType ResolveBinaryResultType(MaterialValueType leftType, MaterialValueType rightType) const;
        std::string MakeBinaryExpression(int32_t left, int32_t right, const char* op);
        std::string MakeTypedCastExpression(MaterialValueType targetType, const std::string& expression) const;
        std::string MakeLiteralExpression(const MaterialLiteralValue& value) const;
        std::string MakeGLSLSourceFromCode(int32_t code) const;
        std::string MakeColorExpression(int32_t code) const;

        int32_t TranslateValue(const MIRValue* value);
        int32_t TranslateMaterialProperty(const MIRGraph& graph, MaterialProperty property);
        std::string MakeScalarLiteral(float value) const;
        void RegisterTextureUniform(const std::string& name);

    private:
        std::vector<CodeChunk> m_CodeChunks;
        std::unordered_map<const MIRValue*, int32_t> m_ValueToCode;
        std::vector<std::string> m_TextureUniforms;
    };
}
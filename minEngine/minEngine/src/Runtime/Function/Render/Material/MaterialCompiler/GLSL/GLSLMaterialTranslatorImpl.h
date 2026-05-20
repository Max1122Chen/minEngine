#pragma once

#include "../MaterialCompileTypes.h"
#include "MIRGLSLPrinter.h"
#include "Render/Material/MaterialIR/MaterialIR.h"

#include <unordered_map>
#include <unordered_set>

namespace minEngine
{
    class MIRGraph;
    class MIRValueType;

    class GLSLMaterialTranslatorImpl
    {
    public:
        MaterialCompileResult Translate(const MIRGraph& graph, const MaterialCompileEnvironment& env);

    private:
        static bool IsFoldable(const MIRInstruction& instr, ShaderStage stage);
        static bool IsOperatorInfix(MIROperatorCode op);

        void BeginStage(ShaderStage stage);
        void LowerBlock(const MIRBlock& block);
        void LowerValue(const MIRValue* value);
        void LowerInstruction(const MIRInstruction& instr);
        void LowerType(const MIRValueType* type);
        void LowerConstant(const MIRConstant* constant);
        void LowerDimensional(const MIRDimensional& dimensional);
        void LowerSubscript(const MIRSubscript& subscript);
        void LowerOperator(const MIROperator& op);
        void LowerBranch(const MIRBranch& branch);
        void LowerSetMaterialOutput(const SetMaterialOutput& output);
        void LowerExternalInput(const MIRExternalInput& externalInput);
        void LowerTextureObject(const MIRTextureObject& textureObject);
        void LowerTextureRead(const MIRTextureRead& textureRead);
        void LowerUniformParameter(const MIRUniformParameter& uniformParameter);

        void AppendFragmentMaterialInputName(MaterialProperty property);
        std::string BuildFragmentShaderPreamble() const;
        std::string GetTextureSamplerName(int textureSlotIndex) const;
        std::string GetScalarUniformName(int uniformSlotIndex) const;
        void FillParameterLayout(MaterialCompileResult& result) const;
        static int ExternalInputIdToTexCoordIndex(MIRExternalInputId inputId);

        const MIRGraph* m_Graph = nullptr;
        ShaderStage m_Stage = Stage_Fragment;
        MIRGLSLPrinter m_Printer;
        int m_NumLocals = 0;
        std::unordered_map<const MIRInstruction*, std::string> m_LocalIdentifier;
        std::unordered_set<int> m_UsedTextureSlots;
        std::unordered_set<int> m_UsedScalarUniformSlots;
        bool m_UsesTexCoord0 = false;
    };
}

#include "MaterialGraphNodeDef.h"
#include "Render/Material/MaterialIR/MIRBuilder.h"

#include "MaterialGraphNodeDef_Const.h"
#include "MaterialGraphNodeDef_Add.h"
#include "MaterialGraphNodeDef_Multiply.h"
#include "MaterialGraphNodeDef_Texture.h"
#include "MaterialGraphNodeDef_Output.h"

namespace minEngine
{
    MaterialGraphNodeDef_Constant::MaterialGraphNodeDef_Constant(float value)
    {
        m_X = value;
        m_Outputs.push_back({ "Value" });
    }

    MIRValue* MaterialGraphNodeDef_Constant::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        return builder.ConstantFloat(m_X);
    }

    MaterialGraphNodeDef_Constant2::MaterialGraphNodeDef_Constant2(float x, float y)
        : m_X(x)
        , m_Y(y)
    {
        m_Outputs.push_back({ "Value" });
    }

    MIRValue* MaterialGraphNodeDef_Constant2::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        return builder.ConstantFloat2(m_X, m_Y);
    }

    MaterialGraphNodeDef_Add::MaterialGraphNodeDef_Add()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    MIRValue* MaterialGraphNodeDef_Add::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        MIRValue* left = builder.BuildInput(m_Inputs[0]);
        MIRValue* right = builder.BuildInput(m_Inputs[1]);
        return builder.Add(left, right);
    }

    MaterialGraphNodeDef_Multiply::MaterialGraphNodeDef_Multiply()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    MIRValue* MaterialGraphNodeDef_Multiply::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        MIRValue* left = builder.BuildInput(m_Inputs[0]);
        MIRValue* right = builder.BuildInput(m_Inputs[1]);
        return builder.Multiply(left, right);
    }

    MaterialGraphNodeDef_Texture2DParameter::MaterialGraphNodeDef_Texture2DParameter(const std::string& name)
        : m_Name(name)
    {
        m_Outputs.push_back({ "Texture" });
    }

    MIRValue* MaterialGraphNodeDef_Texture2DParameter::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        return builder.Texture2DParameter(m_Name);
    }

    MaterialGraphNodeDef_TextureSample::MaterialGraphNodeDef_TextureSample()
    {
        m_Inputs.push_back({ "Texture" });
        m_Inputs.push_back({ "UV" });
        m_Outputs.push_back({ "RGBA" });
    }

    MIRValue* MaterialGraphNodeDef_TextureSample::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        MIRValue* texture = builder.BuildInput(m_Inputs[0]);
        MIRValue* uv = builder.BuildInput(m_Inputs[1]);
        return builder.TextureSample(texture, uv);
    }

    MaterialGraphNodeDef_MaterialOutput::MaterialGraphNodeDef_MaterialOutput()
    {
        m_Inputs.push_back({ "BaseColor" });
        m_Outputs.push_back({ "Result" });
    }

    MIRValue* MaterialGraphNodeDef_MaterialOutput::BuildIR(MIRBuilder& builder, int32_t outputIndex)
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }

        MIRValue* baseColor = builder.BuildInput(m_Inputs[0]);
        if (baseColor == nullptr)
        {
            baseColor = builder.ConstantFloat3(0.0f, 0.0f, 0.0f);
        }

        builder.SetMaterialOutput(MaterialProperty::Albedo, baseColor);
        return baseColor;
    }
}
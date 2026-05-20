#include "MaterialGraphNodeDef.h"
#include "Render/Material/MaterialIR/MIREmitter.h"

namespace minEngine
{
    MaterialGraphNodeDef_Constant::MaterialGraphNodeDef_Constant(float value)
        : Value(value)
    {
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Constant::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.ConstantFloat(Value));
    }

    MaterialGraphNodeDef_Constant3::MaterialGraphNodeDef_Constant3(float r, float g, float b)
        : R(r)
        , G(g)
        , B(b)
    {
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Constant3::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.ConstantFloat3(R, G, B));
    }

    MaterialGraphNodeDef_MakeFloat3::MaterialGraphNodeDef_MakeFloat3()
    {
        m_Inputs.push_back({ "R" });
        m_Inputs.push_back({ "G" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_MakeFloat3::BuildIR(MIREmitter& emitter)
    {
        MIRValue* r = emitter.Input(GetInput(0));
        MIRValue* g = emitter.Input(GetInput(1));
        MIRValue* b = emitter.Input(GetInput(2));
        emitter.Output(0, emitter.Vector3(r, g, b));
    }

    MaterialGraphNodeDef_ConstantInt::MaterialGraphNodeDef_ConstantInt(int64_t value)
        : Value(value)
    {
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_ConstantInt::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.ConstantInt(Value));
    }

    MaterialGraphNodeDef_Add::MaterialGraphNodeDef_Add()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Add::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Add(a, b));
    }

    MaterialGraphNodeDef_Multiply::MaterialGraphNodeDef_Multiply()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Multiply::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Multiply(a, b));
    }

    MaterialGraphNodeDef_Negative::MaterialGraphNodeDef_Negative()
    {
        m_Inputs.push_back({ "Value" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Negative::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.Negative(emitter.Input(GetInput(0))));
    }

    MaterialGraphNodeDef_Not::MaterialGraphNodeDef_Not()
    {
        m_Inputs.push_back({ "Value" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Not::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.Not(emitter.Input(GetInput(0))));
    }

    MaterialGraphNodeDef_Subtract::MaterialGraphNodeDef_Subtract()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Subtract::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Subtract(a, b));
    }

    MaterialGraphNodeDef_Divide::MaterialGraphNodeDef_Divide()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Divide::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Divide(a, b));
    }

    MaterialGraphNodeDef_Max::MaterialGraphNodeDef_Max()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Max::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Max(a, b));
    }

    MaterialGraphNodeDef_Min::MaterialGraphNodeDef_Min()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Min::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Min(a, b));
    }

    MaterialGraphNodeDef_Select::MaterialGraphNodeDef_Select()
    {
        m_Inputs.push_back({ "Condition" });
        m_Inputs.push_back({ "True" });
        m_Inputs.push_back({ "False" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Select::BuildIR(MIREmitter& emitter)
    {
        MIRValue* condition = emitter.Input(GetInput(0));
        MIRValue* trueValue = emitter.Input(GetInput(1));
        MIRValue* falseValue = emitter.Input(GetInput(2));
        emitter.Output(0, emitter.Select(condition, trueValue, falseValue));
    }

    MaterialGraphNodeDef_IfThenElse::MaterialGraphNodeDef_IfThenElse()
    {
        m_Inputs.push_back({ "Condition" });
        m_Inputs.push_back({ "True" });
        m_Inputs.push_back({ "False" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_IfThenElse::BuildIR(MIREmitter& emitter)
    {
        MIRValue* condition = emitter.Input(GetInput(0));
        MIRValue* trueValue = emitter.Input(GetInput(1));
        MIRValue* falseValue = emitter.Input(GetInput(2));
        emitter.Output(0, emitter.Branch(condition, trueValue, falseValue));
    }

    MaterialGraphNodeDef_TextureCoordinate::MaterialGraphNodeDef_TextureCoordinate()
    {
        m_Outputs.push_back({ "UV" });
    }

    void MaterialGraphNodeDef_TextureCoordinate::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.ExternalInput(EI_TexCoord0));
    }

    MaterialGraphNodeDef_TextureObject::MaterialGraphNodeDef_TextureObject(
        std::string parameterName,
        int textureSlotIndex)
        : ParameterName(std::move(parameterName))
        , TextureSlotIndex(textureSlotIndex)
    {
        m_Outputs.push_back({ "Texture" });
    }

    void MaterialGraphNodeDef_TextureObject::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.TextureObject(TextureSlotIndex));
    }

    MaterialGraphNodeDef_TextureSample::MaterialGraphNodeDef_TextureSample()
    {
        m_Inputs.push_back({ "Texture" });
        m_Inputs.push_back({ "UV" });
        m_Outputs.push_back({ "RGBA" });
        m_Outputs.push_back({ "RGB" });
    }

    void MaterialGraphNodeDef_TextureSample::BuildIR(MIREmitter& emitter)
    {
        MIRValue* texture = emitter.Input(GetInput(0));
        MIRValue* texCoord = emitter.Input(GetInput(1));
        MIRValue* sample = emitter.TextureSample(texture, texCoord);
        emitter.Output(0, sample);
        emitter.Output(1, emitter.Cast(sample, MIRPrimitiveType::GetFloat3()));
    }

    MaterialGraphNodeDef_ScalarParameter::MaterialGraphNodeDef_ScalarParameter(
        std::string parameterName,
        int uniformSlotIndex,
        float defaultValue)
        : ParameterName(std::move(parameterName))
        , UniformSlotIndex(uniformSlotIndex)
        , DefaultValue(defaultValue)
    {
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_ScalarParameter::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.UniformScalar(UniformSlotIndex, DefaultValue));
    }

    MaterialGraphNodeDef_ComponentMask::MaterialGraphNodeDef_ComponentMask(int channelIndex)
        : ChannelIndex(channelIndex)
    {
        m_Inputs.push_back({ "Vector" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_ComponentMask::BuildIR(MIREmitter& emitter)
    {
        MIRValue* vector = emitter.Input(GetInput(0));
        emitter.Output(0, emitter.SubscriptChannel(vector, ChannelIndex));
    }

    MaterialGraphNodeDef_MaterialOutput::MaterialGraphNodeDef_MaterialOutput()
    {
        m_Inputs.push_back({ "Albedo" });
        m_Inputs.push_back({ "Metallic" });
        m_Inputs.push_back({ "Roughness" });
        m_Inputs.push_back({ "Emissive" });
        m_Inputs.push_back({ "Opacity" });
        m_Outputs.push_back({ "Result" });
    }

    void MaterialGraphNodeDef_MaterialOutput::BuildIR(MIREmitter& emitter)
    {
        // Attribute values are wired in MIRBuilder::Step_FlowValuesIntoMaterialOutputs.
        MIRValue* result = emitter.Input(GetInput(0));
        emitter.Output(0, result != nullptr ? result : emitter.ConstantFloat(0.0f));
    }
}
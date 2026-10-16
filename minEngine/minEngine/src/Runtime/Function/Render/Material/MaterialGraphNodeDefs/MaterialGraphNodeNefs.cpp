#include "MaterialGraphNodeDef.h"
#include "Render/Material/MaterialIR/MIREmitter.h"
#include "Render/Material/MaterialIR/MaterialIRTypes.h"
#include "Render/Material/MaterialPropertyUtil.h"
#include "Render/Material/MaterialTypes.h"

namespace minEngine
{
    MaterialGraphNodeDef_Constant::MaterialGraphNodeDef_Constant()
        : MaterialGraphNodeDef_Constant(0.0f)
    {
    }

    MaterialGraphNodeDef_Constant::MaterialGraphNodeDef_Constant(float value)
        : Value(value)
    {
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Constant::BuildIR(MIREmitter& emitter)
    {
        emitter.Output(0, emitter.ConstantFloat(Value));
    }

    MaterialValueType MaterialGraphNodeDef_Constant::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
    }

    MaterialGraphNodeDef_Constant3::MaterialGraphNodeDef_Constant3()
        : MaterialGraphNodeDef_Constant3(0.0f, 0.0f, 0.0f)
    {
    }

    MaterialGraphNodeDef_Constant3::MaterialGraphNodeDef_Constant3(float r, float g, float b)
        : R(r)
        , G(g)
        , B(b)
    {
        m_Outputs.push_back({ "Value" });
        m_Outputs.push_back({ "R" });
        m_Outputs.push_back({ "G" });
        m_Outputs.push_back({ "B" });
    }

    void MaterialGraphNodeDef_Constant3::BuildIR(MIREmitter& emitter)
    {
        MIRValue* vector = emitter.ConstantFloat3(R, G, B);
        emitter.Output(0, vector);
        if (emitter.IsOutputConnected(1))
        {
            emitter.Output(1, emitter.SubscriptChannel(vector, 0));
        }
        if (emitter.IsOutputConnected(2))
        {
            emitter.Output(2, emitter.SubscriptChannel(vector, 1));
        }
        if (emitter.IsOutputConnected(3))
        {
            emitter.Output(3, emitter.SubscriptChannel(vector, 2));
        }
    }

    MaterialValueType MaterialGraphNodeDef_Constant3::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex == 0)
        {
            return MIRPrimitiveType::GetFloat3();
        }
        if (outputIndex >= 1 && outputIndex <= 3)
        {
            return MIRPrimitiveType::GetFloat();
        }
        return nullptr;
    }

    MaterialGraphNodeDef_MakeFloat2::MaterialGraphNodeDef_MakeFloat2()
    {
        m_Inputs.push_back({ "X" });
        m_Inputs.push_back({ "Y" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_MakeFloat2::BuildIR(MIREmitter& emitter)
    {
        MIRValue* x = emitter.Input(GetInput(0));
        MIRValue* y = emitter.Input(GetInput(1));
        emitter.Output(0, emitter.Vector2(x, y));
    }

    MaterialValueType MaterialGraphNodeDef_MakeFloat2::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat2();
    }

    MaterialValueType MaterialGraphNodeDef_MakeFloat2::GetInputPinType(int32_t inputIndex) const
    {
        if (GetInput(inputIndex) == nullptr)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
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

    MaterialValueType MaterialGraphNodeDef_MakeFloat3::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat3();
    }

    MaterialValueType MaterialGraphNodeDef_MakeFloat3::GetInputPinType(int32_t inputIndex) const
    {
        if (GetInput(inputIndex) == nullptr)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
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

    MaterialValueType MaterialGraphNodeDef_Not::GetInputPinType(int32_t inputIndex) const
    {
        if (GetInput(inputIndex) == nullptr)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetBool();
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

    MaterialValueType MaterialGraphNodeDef_TextureCoordinate::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat2();
    }

    MaterialGraphNodeDef_VertexColor::MaterialGraphNodeDef_VertexColor()
    {
        m_Outputs.push_back({ "RGBA" });
        m_Outputs.push_back({ "RGB" });
    }

    void MaterialGraphNodeDef_VertexColor::BuildIR(MIREmitter& emitter)
    {
        MIRValue* color = emitter.ExternalInput(EI_VertexColor);
        emitter.Output(0, color);
        if (emitter.IsOutputConnected(1))
        {
            emitter.Output(1, emitter.Cast(color, MIRPrimitiveType::GetFloat3()));
        }
    }

    MaterialValueType MaterialGraphNodeDef_VertexColor::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex == 0)
        {
            return MIRPrimitiveType::GetFloat4();
        }
        if (outputIndex == 1)
        {
            return MIRPrimitiveType::GetFloat3();
        }
        return nullptr;
    }

    MaterialGraphNodeDef_TextureObject::MaterialGraphNodeDef_TextureObject()
        : MaterialGraphNodeDef_TextureObject("", 0)
    {
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

    MaterialValueType MaterialGraphNodeDef_TextureObject::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRObjectType::GetTexture2D();
    }

    MaterialGraphNodeDef_TextureSample::MaterialGraphNodeDef_TextureSample()
    {
        m_Inputs.push_back({ "Texture" });
        m_Inputs.push_back({ "UV" });
        m_Outputs.push_back({ "RGBA" });
        m_Outputs.push_back({ "RGB" });
        m_Outputs.push_back({ "R" });
    }

    void MaterialGraphNodeDef_TextureSample::BuildIR(MIREmitter& emitter)
    {
        MIRValue* texture = emitter.Input(GetInput(0));
        MIRValue* texCoord = emitter.Input(GetInput(1));
        MIRValue* sample = emitter.TextureSample(texture, texCoord);
        emitter.Output(0, sample);
        if (emitter.IsOutputConnected(1))
        {
            emitter.Output(1, emitter.Cast(sample, MIRPrimitiveType::GetFloat3()));
        }
        if (emitter.IsOutputConnected(2))
        {
            emitter.Output(2, emitter.SubscriptChannel(sample, 0));
        }
    }

    MaterialValueType MaterialGraphNodeDef_TextureSample::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex == 0)
        {
            return MIRPrimitiveType::GetFloat4();
        }
        if (outputIndex == 1)
        {
            return MIRPrimitiveType::GetFloat3();
        }
        if (outputIndex == 2)
        {
            return MIRPrimitiveType::GetFloat();
        }
        return nullptr;
    }

    MaterialValueType MaterialGraphNodeDef_TextureSample::GetInputPinType(int32_t inputIndex) const
    {
        const Input* input = GetInput(inputIndex);
        if (input == nullptr)
        {
            return nullptr;
        }
        if (input->Name == "Texture")
        {
            return MIRObjectType::GetTexture2D();
        }
        if (input->Name == "UV")
        {
            return MIRPrimitiveType::GetFloat2();
        }
        return nullptr;
    }

    MaterialGraphNodeDef_ScalarParameter::MaterialGraphNodeDef_ScalarParameter()
        : MaterialGraphNodeDef_ScalarParameter("", 0, 0.0f)
    {
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

    MaterialValueType MaterialGraphNodeDef_ScalarParameter::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
    }

    MaterialGraphNodeDef_ComponentMask::MaterialGraphNodeDef_ComponentMask()
        : MaterialGraphNodeDef_ComponentMask(0)
    {
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

    MaterialValueType MaterialGraphNodeDef_ComponentMask::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
    }

    MaterialGraphNodeDef_Lerp::MaterialGraphNodeDef_Lerp()
    {
        m_Inputs.push_back({ "A" });
        m_Inputs.push_back({ "B" });
        m_Inputs.push_back({ "Alpha" });
        m_Outputs.push_back({ "Value" });
    }

    void MaterialGraphNodeDef_Lerp::BuildIR(MIREmitter& emitter)
    {
        MIRValue* a = emitter.Input(GetInput(0));
        MIRValue* b = emitter.Input(GetInput(1));
        MIRValue* alpha = emitter.Input(GetInput(2));
        if (a == nullptr || b == nullptr || alpha == nullptr)
        {
            emitter.Output(0, emitter.Poison());
            return;
        }

        MIRValue* delta = emitter.Subtract(b, a);
        MIRValue* scaled = emitter.Multiply(alpha, delta);
        emitter.Output(0, emitter.Add(a, scaled));
    }

    MaterialValueType MaterialGraphNodeDef_Lerp::GetInputPinType(int32_t inputIndex) const
    {
        const Input* input = GetInput(inputIndex);
        if (input == nullptr)
        {
            return nullptr;
        }
        if (input->Name == "Alpha")
        {
            return MIRPrimitiveType::GetFloat();
        }
        return nullptr;
    }

    MaterialGraphNodeDef_NormalUnpack::MaterialGraphNodeDef_NormalUnpack()
    {
        m_Inputs.push_back({ "RGB" });
        m_Outputs.push_back({ "Normal" });
    }

    void MaterialGraphNodeDef_NormalUnpack::BuildIR(MIREmitter& emitter)
    {
        MIRValue* rgb = emitter.Input(GetInput(0));
        if (rgb == nullptr)
        {
            emitter.Output(0, emitter.Poison());
            return;
        }

        MIRValue* scaled = emitter.Multiply(rgb, emitter.ConstantFloat3(2.0f, 2.0f, 2.0f));
        emitter.Output(0, emitter.Subtract(scaled, emitter.ConstantFloat3(1.0f, 1.0f, 1.0f)));
    }

    MaterialValueType MaterialGraphNodeDef_NormalUnpack::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat3();
    }

    MaterialValueType MaterialGraphNodeDef_NormalUnpack::GetInputPinType(int32_t inputIndex) const
    {
        if (GetInput(inputIndex) == nullptr)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat3();
    }

    MaterialGraphNodeDef_MaterialOutput::MaterialGraphNodeDef_MaterialOutput()
    {
        m_Inputs.push_back({ "Albedo" });
        m_Inputs.push_back({ "Normal" });
        m_Inputs.push_back({ "Metallic" });
        m_Inputs.push_back({ "Roughness" });
        m_Inputs.push_back({ "AO" });
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

    MaterialValueType MaterialGraphNodeDef_MaterialOutput::GetOutputPinType(int32_t outputIndex) const
    {
        if (outputIndex != 0)
        {
            return nullptr;
        }
        return MIRPrimitiveType::GetFloat();
    }

    MaterialValueType MaterialGraphNodeDef_MaterialOutput::GetInputPinType(int32_t inputIndex) const
    {
        const Input* input = GetInput(inputIndex);
        if (input == nullptr)
        {
            return nullptr;
        }

        for (int32_t propertyIndex = 0; propertyIndex < MaterialShadingPropertyCount; ++propertyIndex)
        {
            const MaterialProperty property = static_cast<MaterialProperty>(propertyIndex);
            MaterialPropertyInputDescription description;
            if (!GetMaterialPropertyInputDescription(property, description))
            {
                continue;
            }
            if (input->Name == description.InputName)
            {
                return description.ExpectedType;
            }
        }
        return nullptr;
    }
}
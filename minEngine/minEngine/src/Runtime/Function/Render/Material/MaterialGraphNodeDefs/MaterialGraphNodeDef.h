#pragma once
#include "Core.h"
#include "Runtime/Core/GUID/GUID.h"
#include "Runtime/Core/Object/MEObject.h"
#include "../MaterialTypes.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace minEngine
{
    class Texture2D;
    class MaterialTranslator;
    class MaterialEdGraphNode;
    class MIRBuilder;
    class MIREmitter;
    struct MIRValue;

    class MaterialGraphNodeDef;
    struct MaterialGraphNodeDefOutput;

    ME_STRUCT()
    struct MaterialGraphNodeDefInput
    {
        ME_GENERATED_BODY(MaterialGraphNodeDefInput)

        ME_PROPERTY()
        std::string Name;

        ME_PROPERTY()
        GUID ConnectedNodeDefGuid = GUID::Zero();

        ME_PROPERTY()
        int32_t OutputIndex = 0;

        // Runtime link; rebuilt after deserialize (LinkNodeDefGraph).
        MaterialGraphNodeDef* NodeDef = nullptr;

        bool IsConnected() const { return NodeDef != nullptr; }
        MaterialGraphNodeDefOutput* GetConnectedOutput() const;
    };

    ME_STRUCT()
    struct MaterialGraphNodeDefOutput
    {
        ME_GENERATED_BODY(MaterialGraphNodeDefOutput)

        ME_PROPERTY()
        std::string Name;
    };

    ME_CLASS(Abstract)
    class MaterialGraphNodeDef : public MEObject
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef)

    public:
        using Input = MaterialGraphNodeDefInput;
        using Output = MaterialGraphNodeDefOutput;

        MaterialGraphNodeDef() = default;
        virtual ~MaterialGraphNodeDef() override = default;

        virtual void BuildIR(MIREmitter& emitter) = 0;
        virtual bool IsMaterialOutputNode() const { return false; }

        int32_t GetInputCount() const { return static_cast<int32_t>(m_Inputs.size()); }
        Input* FindInputByName(const char* name);
        const Input* FindInputByName(const char* name) const;
        int32_t GetOutputCount() const { return static_cast<int32_t>(m_Outputs.size()); }

        virtual Input* GetInput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Inputs.size())) ? &m_Inputs[index] : nullptr; }
        virtual const Input* GetInput(int32_t index) const { return (0 <= index && index < static_cast<int32_t>(m_Inputs.size())) ? &m_Inputs[index] : nullptr; }
        virtual Output* GetOutput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Outputs.size())) ? &m_Outputs[index] : nullptr; }
        virtual const Output* GetOutput(int32_t index) const { return (0 <= index && index < static_cast<int32_t>(m_Outputs.size())) ? &m_Outputs[index] : nullptr; }

        std::vector<Output>& GetOutputs() { return m_Outputs; }
        std::vector<Input>& GetInputs() { return m_Inputs; }

    protected:
        ME_PROPERTY()
        std::vector<Input> m_Inputs;

        ME_PROPERTY()
        std::vector<Output> m_Outputs;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Constant : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Constant)

    public:
        MaterialGraphNodeDef_Constant();
        explicit MaterialGraphNodeDef_Constant(float value);
        void BuildIR(MIREmitter& emitter) override;

        ME_PROPERTY()
        float Value = 0.0f;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Constant3 : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Constant3)

    public:
        MaterialGraphNodeDef_Constant3();
        MaterialGraphNodeDef_Constant3(float r, float g, float b);
        void BuildIR(MIREmitter& emitter) override;

        ME_PROPERTY()
        float R = 0.0f;

        ME_PROPERTY()
        float G = 0.0f;

        ME_PROPERTY()
        float B = 0.0f;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_MakeFloat3 : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_MakeFloat3)

    public:
        MaterialGraphNodeDef_MakeFloat3();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_ConstantInt : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_ConstantInt(int64_t value = 0);
        void BuildIR(MIREmitter& emitter) override;

    public:
        int64_t Value = 0;
    };

    class MaterialGraphNodeDef_Add : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Add();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Multiply : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Multiply)

    public:
        MaterialGraphNodeDef_Multiply();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Negative : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Negative();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Not : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Not();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Subtract : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Subtract)

    public:
        MaterialGraphNodeDef_Subtract();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Divide : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Divide)

    public:
        MaterialGraphNodeDef_Divide();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Max : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Max)

    public:
        MaterialGraphNodeDef_Max();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Min : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Min)

    public:
        MaterialGraphNodeDef_Min();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Lerp : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Lerp)

    public:
        MaterialGraphNodeDef_Lerp();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_NormalUnpack : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_NormalUnpack)

    public:
        MaterialGraphNodeDef_NormalUnpack();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_Select : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_Select)

    public:
        MaterialGraphNodeDef_Select();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_IfThenElse : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_IfThenElse)

    public:
        MaterialGraphNodeDef_IfThenElse();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_TextureCoordinate : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_TextureCoordinate)

    public:
        MaterialGraphNodeDef_TextureCoordinate();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_TextureObject : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_TextureObject)

    public:
        MaterialGraphNodeDef_TextureObject();
        MaterialGraphNodeDef_TextureObject(std::string parameterName, int textureSlotIndex = 0);
        void BuildIR(MIREmitter& emitter) override;

        ME_PROPERTY()
        std::string ParameterName;

        ME_PROPERTY()
        int TextureSlotIndex = 0;

        // Serialized as asset $guid; resolved via AssetManager on material load.
        ME_PROPERTY()
        std::shared_ptr<Texture2D> DefaultTexture;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_TextureSample : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_TextureSample)

    public:
        MaterialGraphNodeDef_TextureSample();
        void BuildIR(MIREmitter& emitter) override;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_ScalarParameter : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_ScalarParameter)

    public:
        MaterialGraphNodeDef_ScalarParameter();
        MaterialGraphNodeDef_ScalarParameter(
            std::string parameterName,
            int uniformSlotIndex = 0,
            float defaultValue = 0.0f);
        void BuildIR(MIREmitter& emitter) override;

        ME_PROPERTY()
        std::string ParameterName;

        ME_PROPERTY()
        int UniformSlotIndex = 0;

        ME_PROPERTY()
        float DefaultValue = 0.0f;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_ComponentMask : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_ComponentMask)

    public:
        MaterialGraphNodeDef_ComponentMask();
        explicit MaterialGraphNodeDef_ComponentMask(int channelIndex);
        void BuildIR(MIREmitter& emitter) override;

        ME_PROPERTY()
        int ChannelIndex = 0;
    };

    ME_CLASS()
    class MaterialGraphNodeDef_MaterialOutput : public MaterialGraphNodeDef
    {
        ME_GENERATED_BODY(MaterialGraphNodeDef_MaterialOutput)

    public:
        MaterialGraphNodeDef_MaterialOutput();
        void BuildIR(MIREmitter& emitter) override;
        bool IsMaterialOutputNode() const override { return true; }
    };
}

#include "Generated/Reflection/MaterialGraphNodeDef.gen.h"

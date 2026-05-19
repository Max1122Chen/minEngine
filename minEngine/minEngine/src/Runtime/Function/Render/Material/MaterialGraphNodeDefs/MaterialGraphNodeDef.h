#pragma once
#include "Core.h"
#include "../MaterialTypes.h"
namespace minEngine
{
    class MaterialTranslator;
    class MaterialEdGraphNode;
    class MIRBuilder;
    class MIREmitter;
    struct MIRValue;

    class MaterialGraphNodeDef;
    struct MaterialGraphNodeDefOutput;

    struct MaterialGraphNodeDefInput
    {
        std::string Name;

        // The nodedef that this input is connected to, nullptr if not connected
        MaterialGraphNodeDef* NodeDef = nullptr;

        // The output index of the connected nodedef, only valid when NodeDef is not nullptr
        int OutputIndex = 0;    

        bool IsConnected() const { return NodeDef != nullptr; }
        MaterialGraphNodeDefOutput* GetConnectedOutput() const;
    };

    struct MaterialGraphNodeDefOutput
    {
        std::string Name;
    };

    class MaterialGraphNodeDef
    {
    public:
        using Input = MaterialGraphNodeDefInput;
        using Output = MaterialGraphNodeDefOutput;

        virtual ~MaterialGraphNodeDef() = default;
        virtual void BuildIR(MIREmitter& emitter) = 0;
        virtual bool IsMaterialOutputNode() const { return false; }

        int32_t GetInputCount() const { return static_cast<int32_t>(m_Inputs.size()); }
        Input* FindInputByName(const char* name);
        const Input* FindInputByName(const char* name) const;
        int32_t GetOutputCount() const { return static_cast<int32_t>(m_Outputs.size()); }

        virtual Input* GetInput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Inputs.size())) ? &m_Inputs[index] : nullptr; }
        virtual Output* GetOutput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Outputs.size())) ? &m_Outputs[index] : nullptr; }

        std::vector<Output>& GetOutputs() { return m_Outputs; }
        std::vector<Input>& GetInputs() { return m_Inputs; }

    protected:
        std::vector<Input> m_Inputs;
        std::vector<Output> m_Outputs;
    };

    class MaterialGraphNodeDef_Constant : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_Constant(float value = 0.0f);
        void BuildIR(MIREmitter& emitter) override;

    public:
        float Value = 0.0f;
    };

    class MaterialGraphNodeDef_Constant3 : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Constant3(float r = 0.0f, float g = 0.0f, float b = 0.0f);
        void BuildIR(MIREmitter& emitter) override;

        float R = 0.0f;
        float G = 0.0f;
        float B = 0.0f;
    };

    class MaterialGraphNodeDef_MakeFloat3 : public MaterialGraphNodeDef
    {
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

    class MaterialGraphNodeDef_Multiply : public MaterialGraphNodeDef
    {
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

    class MaterialGraphNodeDef_Subtract : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Subtract();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Divide : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Divide();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Max : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Max();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Min : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Min();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_Select : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_Select();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_IfThenElse : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_IfThenElse();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_TextureCoordinate : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_TextureCoordinate();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_TextureObject : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_TextureObject(int textureSlotIndex = 0);
        void BuildIR(MIREmitter& emitter) override;

        int TextureSlotIndex = 0;
    };

    class MaterialGraphNodeDef_TextureSample : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_TextureSample();
        void BuildIR(MIREmitter& emitter) override;
    };

    class MaterialGraphNodeDef_ScalarParameter : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_ScalarParameter(int uniformSlotIndex = 0, float defaultValue = 0.0f);
        void BuildIR(MIREmitter& emitter) override;

        int UniformSlotIndex = 0;
        float DefaultValue = 0.0f;
    };

    // Single-channel extract from a vector (R=0, G=1, B=2).
    class MaterialGraphNodeDef_ComponentMask : public MaterialGraphNodeDef
    {
    public:
        explicit MaterialGraphNodeDef_ComponentMask(int channelIndex = 0);
        void BuildIR(MIREmitter& emitter) override;

        int ChannelIndex = 0;
    };

    class MaterialGraphNodeDef_MaterialOutput : public MaterialGraphNodeDef
    {
    public:
        MaterialGraphNodeDef_MaterialOutput();
        void BuildIR(MIREmitter& emitter) override;
        bool IsMaterialOutputNode() const override { return true; }
    };
}
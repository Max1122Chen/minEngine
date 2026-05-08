#pragma once
#include "Core.h"
#include "../MaterialTypes.h"
namespace minEngine
{
    class MaterialTranslator;
    class MaterialEdGraphNode;
    class MIRBuilder;
    struct MIRValue;

    class MaterialGraphNodeDef;

    struct MaterialGraphNodeDefInput
    {
        std::string Name;
        MaterialGraphNodeDef* ConnectedNodeDef = nullptr;
        int32_t ConnectedOutputIndex = -1;
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
        virtual MIRValue* BuildIR(MIRBuilder& builder, int32_t outputIndex) = 0;

        int32_t GetInputCount() const { return static_cast<int32_t>(m_Inputs.size()); }
        int32_t GetOutputCount() const { return static_cast<int32_t>(m_Outputs.size()); }

        virtual Input* GetInput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Inputs.size())) ? &m_Inputs[index] : nullptr; }
        virtual Output* GetOutput(int32_t index) { return (0 <= index && index < static_cast<int32_t>(m_Outputs.size())) ? &m_Outputs[index] : nullptr; }

        std::vector<Output>& GetOutputs() { return m_Outputs; }
        std::vector<Input>& GetInputs() { return m_Inputs; }

        void ConnectInput(int32_t inputIndex, MaterialGraphNodeDef* connectedNodeDef, int32_t connectedOutputIndex)
        {
            if (inputIndex < 0 || inputIndex >= static_cast<int32_t>(m_Inputs.size()))
            {
                return;
            }

            m_Inputs[inputIndex].ConnectedNodeDef = connectedNodeDef;
            m_Inputs[inputIndex].ConnectedOutputIndex = connectedOutputIndex;
        }

    protected:
        std::vector<Input> m_Inputs;
        std::vector<Output> m_Outputs;
    };

    

    

}
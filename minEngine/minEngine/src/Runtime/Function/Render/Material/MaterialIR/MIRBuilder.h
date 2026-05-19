#pragma once
#include "Core.h"
#include "MIRGraph.h"

namespace minEngine
{
    class MaterialEdGraph;
    class MaterialGraphNodeDef;
    class MaterialGraphNodeDefInput;
    struct MaterialGraphNodeDefOutput;
    class MIREmitter;

    struct BuildContext
    {
        std::unordered_set<MaterialGraphNodeDef*> BuiltDefs;
        std::vector<MaterialGraphNodeDef*> NodeDefStack;
        std::unordered_map<const MaterialGraphNodeDefInput*, MIRValue*> InputValues;
        std::unordered_map<const MaterialGraphNodeDefOutput*, MIRValue*> OutputValues;

        MIRValue* GetInputValue(const MaterialGraphNodeDefInput* input) const
        {
            auto it = InputValues.find(input);
            return (it != InputValues.end()) ? it->second : nullptr;
        }

        void SetInputValue(const MaterialGraphNodeDefInput* input, MIRValue* value)
        {
            InputValues[input] = value;
        }

        MIRValue* GetOutputValue(const MaterialGraphNodeDefOutput* output) const
        {
            auto it = OutputValues.find(output);
            return (it != OutputValues.end()) ? it->second : nullptr;
        }

        void SetOutputValue(const MaterialGraphNodeDefOutput* output, MIRValue* value)
        {
            OutputValues[output] = value;
        }
    };

    class MIRBuilder
    {
        friend class MIREmitter;
    public:
        MIRBuilder();

        void AddRootNodeDef(MaterialGraphNodeDef* nodeDef);
        void Build(const MaterialEdGraph& graph, MIRGraph& targetGraph);

    private:
        // Build steps
        void Step_Initialize();
        void Step_GenerateOutputInstructions();
        void Step_BuildNodeDefsToIRGraph();
        void Step_FlowValuesIntoMaterialOutputs();
        void Step_AnalyzeIRGraph();
        void Step_LinkInstructions();
        void Step_Finalize();

        // Helper functions
        void PrepareSingleMaterialAttribute(MaterialProperty property);
        void FlowValueIntoMaterialOutput(MaterialProperty property, MIRValue* value);
        MIRValue* FetchFlowValueForMaterialProperty(MaterialProperty property);
        void BuildTopNodeDef();

        // Value retrieval functions
        void BindValueToOutput(const MaterialGraphNodeDefOutput* output, MIRValue* value) { m_BuildContext.SetOutputValue(output, value); }
        MIRValue* FetchValueFromInput(const MaterialGraphNodeDefInput* input) { return m_BuildContext.GetInputValue(input); }


        const MaterialEdGraph* m_Graph = nullptr;
        MIREmitter* m_Emitter = nullptr;         // The emitter used to emit IR instructions, we need this to be a member variable because we need to pass it into the BuildIR function of each nodedef 
        BuildContext m_BuildContext;    // Since we dont have the "Material Function", so we only need a single BuildContext, if we have "Material Function", then we need a stack of BuildContext to support the nested function calls
        std::vector<MaterialGraphNodeDef*> m_RootNodeDefs;
        SetMaterialOutput* m_AttributeOutputs[MaterialPropCount] = {};
    };
}